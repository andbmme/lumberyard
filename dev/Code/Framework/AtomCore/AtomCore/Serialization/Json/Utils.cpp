/*
* All or portions of this file Copyright (c) Amazon.com, Inc. or its affiliates or
* its licensors.
*
* For complete copyright and license terms please see the LICENSE at the root of this
* distribution (the "License"). All use of this software is governed by the License,
* or, if provided, by the license below or the license accompanying this file. Do not
* remove or modify any license notices. This file is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
*
*/

#include <AzCore/base.h>
#include <AzCore/Component/ComponentApplicationBus.h>
#include <AzCore/IO/ByteContainerStream.h>
#include <AzCore/IO/FileIO.h>
#include <AzCore/IO/GenericStreams.h>
#include <AzCore/IO/SystemFile.h>
#include <AzCore/IO/TextStreamWriters.h>
#include <AzCore/JSON/error/error.h>
#include <AzCore/JSON/error/en.h>
#include <AzCore/JSON/prettywriter.h>
#include <AzCore/Memory/OSAllocator.h>
#include <AzCore/Serialization/Json/JsonSerialization.h>
#include <AzCore/Serialization/Utils.h>

namespace AZ
{
    namespace JsonSerializationUtils
    {
        static const char* FileTypeTag = "Type";
        static const char* FileType = "JsonSerialization";
        static const char* VersionTag = "Version";
        static const char* ClassNameTag = "ClassName";
        static const char* ClassIdTag = "ClassId";
        static const char* ClassDataTag = "ClassData";

        static constexpr size_t MaxFileSize = 1024 * 1024;

        AZ::Outcome<void, AZStd::string> SaveObjectToStreamByType(const void* objectPtr, const Uuid& classId, IO::GenericStream& stream,
             const void* defaultObjectPtr, const JsonSerializerSettings* settings)
        {
            if (!stream.CanWrite())
            {
                return AZ::Failure(AZStd::string("The GenericStream can't be written to"));
            }

            JsonSerializerSettings saveSettings;
            if (settings)
            {
                saveSettings = *settings;
            }

            AZ::SerializeContext* serializeContext = saveSettings.m_serializeContext;
            if (!serializeContext)
            {
                AZ::ComponentApplicationBus::BroadcastResult(serializeContext, &AZ::ComponentApplicationBus::Events::GetSerializeContext); 
                if (!serializeContext)
                {
                    return AZ::Failure(AZStd::string::format("Need SerializeContext for saving"));
                }
                saveSettings.m_serializeContext = serializeContext;
            }

            rapidjson::Document jsonDocument;
            jsonDocument.SetObject();
            jsonDocument.AddMember(rapidjson::StringRef(FileTypeTag), rapidjson::StringRef(FileType), jsonDocument.GetAllocator());
            
            rapidjson::Value serializedObject;
            
            JsonSerializationResult::ResultCode jsonResult = JsonSerialization::Store(serializedObject, jsonDocument.GetAllocator(),
                objectPtr, defaultObjectPtr, classId, saveSettings);

            if (jsonResult.GetProcessing() != JsonSerializationResult::Processing::Completed)
            {
                return AZ::Failure(jsonResult.ToString(""));
            }

            const SerializeContext::ClassData* classData = serializeContext->FindClassData(classId);

            jsonDocument.AddMember(rapidjson::StringRef(VersionTag), 1, jsonDocument.GetAllocator());
            jsonDocument.AddMember(rapidjson::StringRef(ClassNameTag), rapidjson::StringRef(classData->m_name), jsonDocument.GetAllocator());
            jsonDocument.AddMember(rapidjson::StringRef(ClassDataTag), AZStd::move(serializedObject), jsonDocument.GetAllocator());

            AZ::IO::RapidJSONStreamWriter jsonStreamWriter(&stream);
            rapidjson::PrettyWriter<AZ::IO::RapidJSONStreamWriter> writer(jsonStreamWriter);
            bool jsonWriteResult = jsonDocument.Accept(writer);
            if (!jsonWriteResult)
            {
                return AZ::Failure(AZStd::string::format("Unable to write class %s with json serialization format'",
                    classId.ToString<AZStd::string>().data()));
            }

            return AZ::Success();
        }

        AZ::Outcome<void, AZStd::string> SaveObjectToFileByType(const void* classPtr, const Uuid& classId, const AZStd::string& filePath,
            const void* defaultClassPtr, const JsonSerializerSettings* settings)
        {
            AZStd::vector<char> buffer;
            buffer.reserve(1024);
            AZ::IO::ByteContainerStream<AZStd::vector<char> > byteStream(&buffer);
            auto saveResult = SaveObjectToStreamByType(classPtr, classId, byteStream, defaultClassPtr, settings);
            if (saveResult.IsSuccess())
            {
                AZ::IO::FileIOStream outputFileStream;
                if (!outputFileStream.Open(filePath.c_str(), AZ::IO::OpenMode::ModeWrite | AZ::IO::OpenMode::ModeCreatePath | AZ::IO::OpenMode::ModeText))
                {
                    return AZ::Failure(AZStd::string::format("Error opening file '%s' for writing", filePath.c_str()));
                }
                outputFileStream.Write(buffer.size(), buffer.data());
            }
            return saveResult;
        }

        // Helper function to check whether the load outcome was success (for loading json serialization file)
        bool WasLoadSuccess(JsonSerializationResult::Outcomes outcome)
        {
            return (outcome == JsonSerializationResult::Outcomes::Success
                || outcome == JsonSerializationResult::Outcomes::DefaultsUsed
                || outcome == JsonSerializationResult::Outcomes::PartialDefaults);
        }
        
        AZ::Outcome<void, AZStd::string> PrepareDeserializerSettings(const JsonDeserializerSettings* inputSettings, JsonDeserializerSettings& returnSettings
            , AZStd::string& deserializeError)
        {
            if (inputSettings)
            {
                returnSettings = *inputSettings;
            }

            if (!returnSettings.m_serializeContext)
            {
                AZ::ComponentApplicationBus::BroadcastResult(returnSettings.m_serializeContext, &AZ::ComponentApplicationBus::Events::GetSerializeContext);
                if (!returnSettings.m_serializeContext)
                {
                    return AZ::Failure(AZStd::string("Need SerializeContext for loading"));
                }
            }

            // Report unused data field as error by default
            auto reporting = returnSettings.m_reporting;
            auto issueReportingCallback = [&deserializeError, reporting](AZStd::string_view message, JsonSerializationResult::ResultCode result, AZStd::string_view target) -> JsonSerializationResult::ResultCode
            {
                using namespace JsonSerializationResult;

                if (!WasLoadSuccess(result.GetOutcome()))
                {
                    // This if is a hack around fault in the JSON serialization system
                    // Jira: https://jira.agscollab.com/browse/LY-106587
                    if (message != "No part of the string could be interpreted as a uuid.")
                    {
                        deserializeError.append(message);
                        deserializeError.append(AZStd::string::format(" '%s' \n", target.data()));
                    }
                }

                if (reporting)
                {
                    result = reporting(message, result, target);
                }

                return result;
            };

            returnSettings.m_reporting = issueReportingCallback;

            return AZ::Success();
        }


        AZ::Outcome<rapidjson::Document, AZStd::string> ParseJson(AZStd::string_view jsonText)
        {
            rapidjson::Document jsonDocument;
            jsonDocument.Parse(jsonText.data(), jsonText.size());
            if (jsonDocument.HasParseError())
            {
                size_t lineNumber = 1;

                const size_t errorOffset = jsonDocument.GetErrorOffset();
                for (size_t searchOffset = jsonText.find('\n');
                    searchOffset < errorOffset && searchOffset < AZStd::string::npos;
                    searchOffset = jsonText.find('\n', searchOffset + 1))
                {
                    lineNumber++;
                }
                
                return AZ::Failure(AZStd::string::format("JSON parse error at line %zu: %s", lineNumber, rapidjson::GetParseError_En(jsonDocument.GetParseError())));
            }
            else
            {
                return AZ::Success(AZStd::move(jsonDocument));
            }
        }

        AZ::Outcome<rapidjson::Document, AZStd::string> LoadJson(IO::GenericStream& stream)
        {
            IO::SizeType length = stream.GetLength();

            // Protect from allocating too much memory. The choice of a 1MB threshold is arbitrary, but it's doubtful that there would be a
            // legitimate json file this large.
            if (length > MaxFileSize)
            {
                return AZ::Failure(AZStd::string{"Data is too large."});
            }

            AZStd::vector<char> memoryBuffer;
            memoryBuffer.resize_no_construct(static_cast<AZStd::vector<char>::size_type>(static_cast<AZStd::vector<char>::size_type>(length) + 1));

            IO::SizeType bytesRead = stream.Read(length, memoryBuffer.data());
            if (bytesRead != length)
            {
                return AZ::Failure(AZStd::string{"Cannot to read input stream."});
            }

            memoryBuffer.back() = 0;

            return ParseJson(AZStd::string_view{memoryBuffer.data(), memoryBuffer.size()});
        }

        AZ::Outcome<rapidjson::Document, AZStd::string> LoadJson(AZStd::string_view filePath)
        {
            IO::FileIOStream file;
            if (!file.Open(filePath.data(), IO::OpenMode::ModeRead))
            {
                return AZ::Failure(AZStd::string::format("Failed to open '%.*s'.", AZ_STRING_ARG(filePath)));
            }

            auto result = LoadJson(file);
            if (!result.IsSuccess())
            {
                return AZ::Failure(AZStd::string::format("Failed to load '%.*s'. %s", AZ_STRING_ARG(filePath), result.GetError().c_str()));
            }
            else
            {
                return result;
            }
        }

        // Helper function to validate the JSON is structured with the standard header for a generic class
        AZ::Outcome<void, AZStd::string> ValidateJsonClassHeader(const rapidjson::Document& jsonDocument)
        {
            auto typeItr = jsonDocument.FindMember(FileTypeTag);
            if (typeItr == jsonDocument.MemberEnd() || !typeItr->value.IsString() || azstricmp(typeItr->value.GetString(), FileType) != 0)
            {
                return AZ::Failure(AZStd::string::format("Not a valid JsonSerialization file"));
            }

            auto nameItr = jsonDocument.FindMember(ClassNameTag);
            if (nameItr == jsonDocument.MemberEnd() || !nameItr->value.IsString())
            {
                return AZ::Failure(AZStd::string::format("File should contain ClassName"));
            }

            auto dataItr = jsonDocument.FindMember(ClassDataTag);
            // data can be empty but it should be an object
            if (dataItr != jsonDocument.MemberEnd() && !dataItr->value.IsObject())
            {
                return AZ::Failure(AZStd::string::format("ClassData should be an object"));
            }

            return AZ::Success();
        }

        AZ::Outcome<void, AZStd::string> LoadObjectFromStreamByType(void* objectToLoad, const Uuid& classId, IO::GenericStream& stream,
            const JsonDeserializerSettings* settings)
        {
            JsonDeserializerSettings loadSettings;
            AZStd::string deserializeErrors;
            auto prepare = PrepareDeserializerSettings(settings, loadSettings, deserializeErrors);
            if (!prepare.IsSuccess())
            {
                return AZ::Failure(prepare.GetError());
            }

            auto parseResult = LoadJson(stream);
            if (!parseResult.IsSuccess())
            {
                return AZ::Failure(parseResult.GetError());
            }

            const rapidjson::Document& jsonDocument = parseResult.GetValue();

            auto validateResult = ValidateJsonClassHeader(jsonDocument);
            if (!validateResult.IsSuccess())
            {
                return AZ::Failure(validateResult.GetError());
            }

            const char* className = jsonDocument.FindMember(ClassNameTag)->value.GetString();

            // validate class name 
            auto classData = loadSettings.m_serializeContext->FindClassData(classId);
            if (azstricmp(classData->m_name, className) != 0)
            {
                return AZ::Failure(AZStd::string::format("Try to load class %s from class %s data", classData->m_name, className));
            }
            
            JsonSerializationResult::ResultCode result = JsonSerialization::Load(objectToLoad, classId, jsonDocument.FindMember(ClassDataTag)->value, loadSettings);

            if (!WasLoadSuccess(result.GetOutcome()) || !deserializeErrors.empty())
            {
                return AZ::Failure(deserializeErrors);
            }
            return AZ::Success();
        }
       
        
        AZ::Outcome<AZStd::any, AZStd::string> LoadAnyObjectFromStream(IO::GenericStream& stream, const JsonDeserializerSettings* settings)
        {
            JsonDeserializerSettings loadSettings;
            AZStd::string deserializeErrors;
            auto prepare = PrepareDeserializerSettings(settings, loadSettings, deserializeErrors);
            if (!prepare.IsSuccess())
            {
                return AZ::Failure(prepare.GetError());
            }

            auto parseResult = LoadJson(stream);
            if (!parseResult.IsSuccess())
            {
                return AZ::Failure(parseResult.GetError());
            }

            const rapidjson::Document& jsonDocument = parseResult.GetValue();

            auto validateResult = ValidateJsonClassHeader(jsonDocument);
            if (!parseResult.IsSuccess())
            {
                return AZ::Failure(parseResult.GetError());
            }

            const char* className = jsonDocument.FindMember(ClassNameTag)->value.GetString();
            AZStd::vector<AZ::Uuid> ids = loadSettings.m_serializeContext->FindClassId(AZ::Crc32(className));

            // Load with first found class id
            if (ids.size() >= 1)
            {
                auto classId = ids[0];
                AZStd::any anyData = loadSettings.m_serializeContext->CreateAny(classId);
                auto& objectData = jsonDocument.FindMember(ClassDataTag)->value;
                JsonSerializationResult::ResultCode result = JsonSerialization::Load(AZStd::any_cast<void>(&anyData), classId, objectData, loadSettings);

                if (!WasLoadSuccess(result.GetOutcome()) || !deserializeErrors.empty())
                {
                    return AZ::Failure(deserializeErrors);
                }

                return AZ::Success(anyData);
            }

            return AZ::Failure(AZStd::string::format("Can't find serialize context for class %s", className));
        }

        AZ::Outcome<AZStd::any, AZStd::string> LoadAnyObjectFromFile(const AZStd::string& filePath, const JsonDeserializerSettings* settings = nullptr)
        {
            AZ::IO::FileIOStream inputFileStream;
            if (!inputFileStream.Open(filePath.c_str(), AZ::IO::OpenMode::ModeRead | AZ::IO::OpenMode::ModeText))
            {
                return AZ::Failure(AZStd::string::format("Error opening file '%s' for reading", filePath.c_str()));
            }
            return LoadAnyObjectFromStream(inputFileStream, settings);
        }

        //! Reporting call back that can be used in JsonSerializerSettings to report AZ_Waring when fields are skipped
        AZ::JsonSerializationResult::ResultCode ReportCommonWarnings(AZStd::string_view /*message*/, AZ::JsonSerializationResult::ResultCode result, AZStd::string_view path)
        {
            if (result.GetOutcome() == JsonSerializationResult::Outcomes::Skipped)
            {
                AZStd::vector<AZStd::string> tokens;
                if (path.find("/#") == AZStd::string::npos) // Allow fields to start with '#' to indicate a comment
                {
                    AZ_Warning("JSON", false, "Skipped unrecognized field '%.*s'", AZ_STRING_ARG(path));
                }
            }
            else if (result.GetProcessing() != JsonSerializationResult::Processing::Completed ||
                     result.GetOutcome() >= JsonSerializationResult::Outcomes::Unsupported)
            {
                AZ_Warning("JSON", false, "'%.*s': %s", AZ_STRING_ARG(path), result.ToString("").c_str());
            }

            return result;
        }

    } // namespace JsonSerializationUtils
} // namespace AZ
