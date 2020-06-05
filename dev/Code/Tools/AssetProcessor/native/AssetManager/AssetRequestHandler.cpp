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
#include "AssetRequestHandler.h"

#include "native/utilities/AssetUtilEBusHelper.h"

#include <AzCore/Serialization/Utils.h>
#include <AzFramework/Asset/AssetProcessorMessages.h>
#include <AzCore/IO/GenericStreams.h>
#include <AzCore/Component/ComponentApplicationBus.h>
#include <QDir>
#include <QFile>
#include <QTimer>
#include <AzToolsFramework/API/EditorAssetSystemAPI.h>
#include <AzToolsFramework/ToolsComponents/ToolsAssetCatalogBus.h>

using namespace AssetProcessor;

namespace
{
    static const uint32_t s_assetPath = AssetUtilities::ComputeCRC32Lowercase("assetPath");
}

AssetRequestHandler::AssetRequestLine::AssetRequestLine(QString platform, QString searchTerm, const AZ::Data::AssetId& assetId, bool isStatusRequest)
    : m_platform(platform)
    , m_searchTerm(searchTerm)
    , m_isStatusRequest(isStatusRequest)
    , m_assetId(assetId)
{
}

bool AssetRequestHandler::AssetRequestLine::IsStatusRequest() const
{
    return m_isStatusRequest;
}

QString AssetRequestHandler::AssetRequestLine::GetPlatform() const
{
    return m_platform;
}
QString AssetRequestHandler::AssetRequestLine::GetSearchTerm() const
{
    return m_searchTerm;
}

const AZ::Data::AssetId& AssetRequestHandler::AssetRequestLine::GetAssetId() const
{
    return m_assetId;
}

QString AssetRequestHandler::AssetRequestLine::GetDisplayString() const
{
    if (m_assetId.IsValid())
    {
        return QString::fromUtf8(m_assetId.ToString<AZStd::string>().c_str());
    }
    return m_searchTerm;
}

int AssetRequestHandler::GetNumOutstandingAssetRequests() const
{
    return m_pendingAssetRequests.size();
}

bool AssetRequestHandler::InvokeHandler(AzFramework::AssetSystem::BaseAssetProcessorMessage* message, NetworkRequestID key, QString platform, bool fencingFailed)
{
    // This function checks to see whether the incoming message is either one of those request, which require decoding the type of message and then invoking the appropriate EBUS handler.
    // If the message is not one of those type than it checks to see whether some one has registered a request handler for that message type and then invokes it.

    using namespace AzFramework::AssetSystem;
    if (message->GetMessageType() == GetFullSourcePathFromRelativeProductPathRequest::MessageType())
    {
        GetFullSourcePathFromRelativeProductPathRequest* request = azrtti_cast<GetFullSourcePathFromRelativeProductPathRequest*>(message);
        if (!request)
        {
            AZ_TracePrintf(AssetProcessor::DebugChannel, "Expected GetFullSourcePathFromRelativeProductPathRequest(%d) but incoming message type is %d.\n", GetFullSourcePathFromRelativeProductPathRequest::MessageType(), message->GetMessageType());
            return true;
        }

        bool fullPathfound = false;
        AZStd::string fullSourcePath;
        EBUS_EVENT_RESULT(fullPathfound, AzToolsFramework::AssetSystemRequestBus, GetFullSourcePathFromRelativeProductPath, request->m_relativeProductPath, fullSourcePath);

        if (!fullPathfound)
        {
            AZ_TracePrintf(AssetProcessor::ConsoleChannel, "Could not find full source path from the relative product path (%s).\n", request->m_relativeProductPath.c_str());
        }

        GetFullSourcePathFromRelativeProductPathResponse response(fullPathfound, fullSourcePath);
        EBUS_EVENT_ID(key.first, AssetProcessor::ConnectionBus, SendResponse, key.second, response);
        return true;
    }
    else if (message->GetMessageType() == GetRelativeProductPathFromFullSourceOrProductPathRequest::MessageType())
    {
        GetRelativeProductPathFromFullSourceOrProductPathRequest* request = azrtti_cast<GetRelativeProductPathFromFullSourceOrProductPathRequest*>(message);
        if (!request)
        {
            AZ_TracePrintf(AssetProcessor::DebugChannel, "Expected GetRelativeProductPathFromFullSourceOrProductPathRequest(%d) but incoming message type is %d.\n", GetRelativeProductPathFromFullSourceOrProductPathRequest::MessageType(), message->GetMessageType());
            return true;
        }

        bool relPathFound = false;
        AZStd::string relProductPath;

        EBUS_EVENT_RESULT(relPathFound, AzToolsFramework::AssetSystemRequestBus, GetRelativeProductPathFromFullSourceOrProductPath, request->m_sourceOrProductPath, relProductPath);
        if (!relPathFound)
        {
            AZ_TracePrintf(AssetProcessor::ConsoleChannel, "Could not find relative product path for the source file (%s).", request->m_sourceOrProductPath.c_str());
        }

        GetRelativeProductPathFromFullSourceOrProductPathResponse response(relPathFound, relProductPath);
        EBUS_EVENT_ID(key.first, AssetProcessor::ConnectionBus, SendResponse, key.second, response);
        return true;
    }
    else if (message->GetMessageType() == SourceAssetInfoRequest::MessageType())
    {
        SourceAssetInfoRequest* request = azrtti_cast<SourceAssetInfoRequest*>(message);
        
        if (!request)
        {
            AZ_TracePrintf(AssetProcessor::DebugChannel, "Expected SourceAssetInfoRequest(%d) but incoming message type is %d.\n", SourceAssetInfoRequest::MessageType(), message->GetMessageType());
            return true;
        }

        SourceAssetInfoResponse response;
        if (request->m_assetId.IsValid())
        {

            AZStd::string rootFolder;
            AzToolsFramework::AssetSystemRequestBus::BroadcastResult(response.m_found, &AzToolsFramework::AssetSystem::AssetSystemRequest::GetSourceInfoBySourceUUID, request->m_assetId.m_guid, response.m_assetInfo, rootFolder);
 
            if (response.m_found)
            {
                response.m_assetInfo.m_assetId.m_subId = request->m_assetId.m_subId;
                response.m_assetInfo.m_assetType = request->m_assetType;
                response.m_rootFolder = rootFolder.c_str();
            }
            else 
            {
                response.m_assetInfo.m_assetId.SetInvalid();
            }
            AssetProcessor::ConnectionBus::Event(key.first, &AssetProcessor::ConnectionBusTraits::SendResponse, key.second, response);
        }
        else if (!request->m_assetPath.empty())
        {
            AZStd::string rootFolder;
            // its being asked for via path instead of ID.  slightly different call.
            AzToolsFramework::AssetSystemRequestBus::BroadcastResult(response.m_found, &AzToolsFramework::AssetSystem::AssetSystemRequest::GetSourceInfoBySourcePath, request->m_assetPath.c_str(), response.m_assetInfo, rootFolder);
            response.m_rootFolder = rootFolder.c_str();
        }
        // note that in the case of an invalid request, response is defaulted to false for m_found, so there is no need to
        // populate the response in that case.

        AssetProcessor::ConnectionBus::Event(key.first, &AssetProcessor::ConnectionBusTraits::SendResponse, key.second, response);
        return true;
    }
    else if (message->GetMessageType() == AzToolsFramework::AssetSystem::SourceAssetProductsInfoRequest::MessageType())
    {
        AzToolsFramework::AssetSystem::SourceAssetProductsInfoRequest* request = 
            azrtti_cast<AzToolsFramework::AssetSystem::SourceAssetProductsInfoRequest*>(message);

        if (!request)
        {
            AZ_TracePrintf(AssetProcessor::DebugChannel, 
                "Invalid Message Type: Message is not of type %d.  Incoming message type is %d.\n", 
                AzToolsFramework::AssetSystem::SourceAssetProductsInfoRequest::MessageType(), message->GetMessageType());
            return true;
        }

        AzToolsFramework::AssetSystem::SourceAssetProductsInfoResponse response;
        if (request->m_assetId.IsValid())
        {
            AzToolsFramework::AssetSystemRequestBus::BroadcastResult(response.m_found, 
                &AzToolsFramework::AssetSystem::AssetSystemRequest::GetAssetsProducedBySourceUUID, 
                request->m_assetId.m_guid, response.m_productsAssetInfo);

            AssetProcessor::ConnectionBus::Event(key.first, &AssetProcessor::ConnectionBusTraits::SendResponse, key.second, response);
        }

        // note that in the case of an invalid request, response is defaulted to false for m_found, so there is no need to
        // populate the response in that case.

        AssetProcessor::ConnectionBus::Event(key.first, &AssetProcessor::ConnectionBusTraits::SendResponse, key.second, response);
        return true;
    }
    else if (message->GetMessageType() == AzToolsFramework::AssetSystem::GetScanFoldersRequest::MessageType())
    {
        AzToolsFramework::AssetSystem::GetScanFoldersRequest* request = azrtti_cast<AzToolsFramework::AssetSystem::GetScanFoldersRequest*>(message);
        if (!request)
        {
            AZ_TracePrintf(AssetProcessor::DebugChannel, "Expected GetScanFoldersRequest(%d) but incoming message type is %d.\n", 
                AzToolsFramework::AssetSystem::GetScanFoldersRequest::MessageType(), message->GetMessageType());
            return true;
        }

        bool success = true;;
        AZStd::vector<AZStd::string> scanFolders;
        AzToolsFramework::AssetSystemRequestBus::BroadcastResult(success, &AzToolsFramework::AssetSystemRequestBus::Events::GetScanFolders, scanFolders);
        if (!success)
        {
            AZ_TracePrintf(AssetProcessor::ConsoleChannel, "Could not acquire a list of scan folders from the database.");
        }


        AzToolsFramework::AssetSystem::GetScanFoldersResponse response(AZStd::move(scanFolders));
        AssetProcessor::ConnectionBus::Event(key.first, &AssetProcessor::ConnectionBusTraits::SendResponse, key.second, response);
        return true;
    }
    else if (message->GetMessageType() == AzToolsFramework::AssetSystem::GetAssetSafeFoldersRequest::MessageType())
    {
        AzToolsFramework::AssetSystem::GetAssetSafeFoldersRequest* request = azrtti_cast<AzToolsFramework::AssetSystem::GetAssetSafeFoldersRequest*>(message);
        if (!request)
        {
            AZ_TracePrintf(AssetProcessor::DebugChannel, "Invalid Message Type: Message is not of type %d.Incoming message type is %d.\n",
                AzToolsFramework::AssetSystem::GetAssetSafeFoldersRequest::MessageType(), message->GetMessageType());
            return true;
        }

        bool success = true;;
        AZStd::vector<AZStd::string> assetSafeFolders;
        AzToolsFramework::AssetSystemRequestBus::BroadcastResult(success, &AzToolsFramework::AssetSystemRequestBus::Events::GetAssetSafeFolders, assetSafeFolders);
        if (!success)
        {
            AZ_TracePrintf(AssetProcessor::ConsoleChannel, "Could not acquire a list of asset safe folders from the database.");
        }


        AzToolsFramework::AssetSystem::GetAssetSafeFoldersResponse response(AZStd::move(assetSafeFolders));
        AssetProcessor::ConnectionBus::Event(key.first, &AssetProcessor::ConnectionBusTraits::SendResponse, key.second, response);
        return true;
    }

    else if (message->GetMessageType() == RegisterSourceAssetRequest::MessageType())
    {
        RegisterSourceAssetRequest* request = azrtti_cast<RegisterSourceAssetRequest*>(message);
        
        if (!request)
        {
            AZ_TracePrintf(AssetProcessor::DebugChannel, "Expected RegisterSourceAssetRequest(%d) but incoming message type is %d.\n", RegisterSourceAssetRequest::MessageType(), message->GetMessageType());
            return true;
        }

        AzToolsFramework::ToolsAssetSystemBus::Broadcast(&AzToolsFramework::ToolsAssetSystemRequests::RegisterSourceAssetType, request->m_assetType, request->m_assetFileFilter.c_str());

        return true;
    }
    else if (message->GetMessageType() == UnregisterSourceAssetRequest::MessageType())
    {
        UnregisterSourceAssetRequest* request = azrtti_cast<UnregisterSourceAssetRequest*>(message);

        if (!request)
        {
            AZ_TracePrintf(AssetProcessor::DebugChannel, "Expected UnregisterSourceAssetRequest(%d) but incoming message type is %d.\n", UnregisterSourceAssetRequest::MessageType(), message->GetMessageType());
            return true;
        }

        AzToolsFramework::ToolsAssetSystemBus::Broadcast(&AzToolsFramework::ToolsAssetSystemRequests::UnregisterSourceAssetType, request->m_assetType);

        return true;
    }
    else if (message->GetMessageType() == RequestEscalateAsset::MessageType())
    {
        RequestEscalateAsset* request = azrtti_cast<RequestEscalateAsset*>(message);

        if (!request)
        {
            AZ_TracePrintf(AssetProcessor::DebugChannel, "Expected RequestEscalateAsset(%d) but incoming message type is %d.\n", RequestEscalateAsset::MessageType(), message->GetMessageType());
            // returning true here means to delete the message.
            return true;
        }

        if (!request->m_assetUuid.IsNull())
        {
            // search by UUID is preferred.
            Q_EMIT RequestEscalateAssetByUuid(platform, request->m_assetUuid);
        }
        else if (!request->m_searchTerm.empty())
        {
            // fall back to search term.
            Q_EMIT RequestEscalateAssetBySearchTerm(platform, QString::fromUtf8(request->m_searchTerm.c_str()));
        }
        else
        {
            AZ_Warning(AssetProcessor::DebugChannel, false,"Invalid RequestEscalateAsset.  Both the search term and uuid are empty/null\n");
        }

        return true;
    }
    else if (message->GetMessageType() == AzFramework::AssetSystem::AssetInfoRequest::MessageType())
    {
        AssetInfoRequest* request = azrtti_cast<AssetInfoRequest*>(message);

        if (!request)
        {
            AZ_TracePrintf(AssetProcessor::DebugChannel, "Invalid Message Type: Message is not of type %d.  Incoming message type is %d.\n", AzFramework::AssetSystem::AssetInfoRequest::MessageType(), message->GetMessageType());
            return true;
        }

        AssetInfoResponse response;

        if (request->m_assetId.IsValid())
        {
            AZStd::string rootFilePath;
            AzToolsFramework::AssetSystemRequestBus::BroadcastResult(response.m_found, &AzToolsFramework::AssetSystemRequestBus::Events::GetAssetInfoById, request->m_assetId, request->m_assetType, response.m_assetInfo, rootFilePath);
            response.m_rootFolder = rootFilePath;
        }
        else if (!request->m_assetPath.empty())
        {
            bool autoRegisterIfNotFound = false;
            AZ::Data::AssetCatalogRequestBus::BroadcastResult(response.m_assetInfo.m_assetId, &AZ::Data::AssetCatalogRequestBus::Events::GetAssetIdByPath, request->m_assetPath.c_str(), AZ::Data::s_invalidAssetType, autoRegisterIfNotFound);
            response.m_found = response.m_assetInfo.m_assetId.IsValid();
        }

        AssetProcessor::ConnectionBus::Event(key.first, &AssetProcessor::ConnectionBusTraits::SendResponse, key.second, response);
        return true;
    }
    else
    {
        auto located = m_RequestHandlerMap.find(message->GetMessageType());
        if (located == m_RequestHandlerMap.end())
        {
            AZ_TracePrintf(AssetProcessor::DebugChannel, "OnNewIncomingRequest: Message Handler not found for message type %d, ignoring.\n", message->GetMessageType());
            return true;
        }
        // Invoke the registered handler
        QMetaObject::invokeMethod(located.value(), "RequestReady", Qt::QueuedConnection, Q_ARG(NetworkRequestID, key), Q_ARG(BaseAssetProcessorMessage*, message), Q_ARG(QString, platform), Q_ARG(bool, fencingFailed));
        return false;
    }
}

void AssetRequestHandler::ProcessAssetRequest(NetworkRequestID networkRequestId, BaseAssetProcessorMessage* message, QString platform, bool /*fencingFailed*/)
{
    using RequestAssetStatus = AzFramework::AssetSystem::RequestAssetStatus;
    RequestAssetStatus* stat = azrtti_cast<RequestAssetStatus*>(message);
    
    if (!stat)
    {
        AZ_TracePrintf(AssetProcessor::DebugChannel, "ProcessAssetRequest: Message is not of type %d.Incoming message type is %d.\n", RequestAssetStatus::MessageType(), message->GetMessageType());
        return;
    }
        
    if ((stat->m_searchTerm.empty())&&(!stat->m_assetId.IsValid()))
    {
        AZ_TracePrintf(AssetProcessor::DebugChannel, "Failed to decode incoming RequestAssetStatus - both path and uuid is empty\n");
        SendAssetStatus(networkRequestId, RequestAssetStatus::MessageType(), AssetStatus::AssetStatus_Unknown);
        return;
    }
    AssetRequestHandler::AssetRequestLine newLine(platform, QString::fromUtf8(stat->m_searchTerm.c_str()), stat->m_assetId, stat->m_isStatusRequest);
    AZ_TracePrintf(AssetProcessor::DebugChannel, "GetAssetStatus/CompileAssetSync: %s.\n", newLine.GetDisplayString().toUtf8().constData());

    QString assetPath = QString::fromUtf8(stat->m_searchTerm.c_str());  // utf8-decode just once here, reuse below
    m_pendingAssetRequests.insert(networkRequestId, newLine);
    Q_EMIT RequestCompileGroup(networkRequestId, platform, assetPath, stat->m_assetId, stat->m_isStatusRequest);
}

void AssetRequestHandler::OnCompileGroupCreated(NetworkRequestID groupID, AssetStatus status)
{
    using namespace AzFramework::AssetSystem;
    auto located = m_pendingAssetRequests.find(groupID);

    if (located == m_pendingAssetRequests.end())
    {
        AZ_TracePrintf(AssetProcessor::DebugChannel, "OnCompileGroupCreated: No such asset group found, ignoring.\n");
        return;
    }

    if (status == AssetStatus_Unknown)
    {
        // if this happens it means we made an async request and got a response from the build queue that no such thing
        // exists in the queue.  It might still be a valid asset - for example, it may have already finished compiling and thus
        // won't be in the queue.  To cover this we also make a request to the asset manager here (its also async)
        Q_EMIT RequestAssetExists(groupID, located.value().GetPlatform(), located.value().GetSearchTerm(), located.value().GetAssetId());
    }
    else
    {
        // if its a status request, return it immediately and then remove it.
        if (located.value().IsStatusRequest())
        {
            AZ_TracePrintf(AssetProcessor::DebugChannel, "GetAssetStatus: Responding with status of: %s\n", located.value().GetDisplayString().toUtf8().constData());
            SendAssetStatus(groupID, RequestAssetStatus::MessageType(), status);
            m_pendingAssetRequests.erase(located);
        }
        // if its not a status request then we'll wait for OnCompileGroupFinished before responding.
    }
}

void AssetRequestHandler::OnCompileGroupFinished(NetworkRequestID groupID, AssetStatus status)
{
    auto located = m_pendingAssetRequests.find(groupID);

    if (located == m_pendingAssetRequests.end())
    {
        // this is okay to happen if its a status request.
        return;
    }

    // if the compile group finished, but the request was for a SPECIFIC asset, we have to take an extra step since
    // the compile group being finished just means the source file has compiled, doesn't necessarly mean that specific asset is emitted.
    if (located.value().GetAssetId().IsValid())
    {
        Q_EMIT RequestAssetExists(groupID, located.value().GetPlatform(), located.value().GetSearchTerm(), located.value().GetAssetId());
    }
    else
    {
        AZ_TracePrintf(AssetProcessor::DebugChannel, "Compile Group finished: %s.\n", located.value().GetDisplayString().toUtf8().constData());
        SendAssetStatus(groupID, AzFramework::AssetSystem::RequestAssetStatus::MessageType(), status);
        m_pendingAssetRequests.erase(located);
    }
}

//! Called from the outside in response to a RequestAssetExists.
void AssetRequestHandler::OnRequestAssetExistsResponse(NetworkRequestID groupID, bool exists)
{
    using namespace AzFramework::AssetSystem;
    auto located = m_pendingAssetRequests.find(groupID);

    if (located == m_pendingAssetRequests.end())
    {
        AZ_TracePrintf(AssetProcessor::DebugChannel, "OnRequestAssetExistsResponse: No such compile group found, ignoring.\n");
        return;
    }
    
    AZ_TracePrintf(AssetProcessor::DebugChannel, "GetAssetStatus / CompileAssetSync: Asset %s is %s.\n", 
        located.value().GetDisplayString().toUtf8().constData(), 
        exists ? "compiled already" : "missing" );

    SendAssetStatus(groupID, RequestAssetStatus::MessageType(), exists ? AssetStatus_Compiled : AssetStatus_Missing);

    m_pendingAssetRequests.erase(located);
}

void AssetRequestHandler::SendAssetStatus(NetworkRequestID groupID, unsigned int /*type*/, AssetStatus status)
{
    AzFramework::AssetSystem::ResponseAssetStatus resp;
    resp.m_assetStatus = status;
    EBUS_EVENT_ID(groupID.first, AssetProcessor::ConnectionBus, SendResponse, groupID.second, resp);
}

AssetRequestHandler::AssetRequestHandler()
{
    RegisterRequestHandler(AzFramework::AssetSystem::RequestAssetStatus::MessageType(), this);
}

void AssetRequestHandler::RegisterRequestHandler(unsigned int messageId, QObject* object)
{
    m_RequestHandlerMap.insert(messageId, object);
}

void AssetRequestHandler::DeRegisterRequestHandler(unsigned int messageId, QObject* /*object*/)
{
    auto located = m_RequestHandlerMap.find(messageId);

    if (located == m_RequestHandlerMap.end())
    {
        AZ_TracePrintf(AssetProcessor::DebugChannel, "ProcessFilesToExamineQueue: Compile group request not found, ignoring.\n");
        return;
    }

    m_RequestHandlerMap.erase(located);
}


QString AssetRequestHandler::CreateFenceFile(unsigned int fenceId)
{
    QDir fenceDir;
    if (!AssetUtilities::ComputeFenceDirectory(fenceDir))
    {
        return QString();
    }

    QString fileName = QString("fenceFile~%1.%2").arg(fenceId).arg(FENCE_FILE_EXTENSION);
    QString fenceFileName = fenceDir.filePath(fileName);
    QFileInfo fileInfo(fenceFileName);

    if (!fileInfo.absoluteDir().exists())
    {
        // if fence dir does not exists ,than try to create it
        if (!fileInfo.absoluteDir().mkpath("."))
        {
            return QString();
        }
    }

    QFile fenceFile(fenceFileName);

    if (fenceFile.exists())
    {
        return QString();
    }

    bool result = fenceFile.open(QFile::WriteOnly);

    if (!result)
    {
        return QString();
    }

    fenceFile.close();
    return fileInfo.absoluteFilePath();
}

bool AssetRequestHandler::DeleteFenceFile(QString fenceFileName)
{
    return QFile::remove(fenceFileName);
}

void AssetRequestHandler::DeleteFenceFile_Retry(unsigned int fenceId, QString fenceFileName, NetworkRequestID key, BaseAssetProcessorMessage* message, QString platform, int retriesRemaining)
{
    if (DeleteFenceFile(fenceFileName))
    {
        // add an entry in map
        // We have successfully created and deleted the fence file, insert an entry for it in the pendingFenceRequest map
        // and return, we will only process this request once the APM indicates that it has detected the fence file
        m_pendingFenceRequestMap.insert(fenceId, RequestInfo(key, message, platform));
        return;
    }

    retriesRemaining--;

    if (retriesRemaining == 0)
    {
        AZ_TracePrintf(AssetProcessor::DebugChannel, "AssetProcessor was unable to delete the fence file");

        // send request to the appropriate handler with fencingfailed set to true and return
        if (InvokeHandler(message, key, platform, true))
        {
            delete message;
        }
    }
    else
    {

        auto deleteFenceFilefunctor = [this, fenceId, fenceFileName, key, message, platform, retriesRemaining]
        {
            DeleteFenceFile_Retry(fenceId, fenceFileName, key, message, platform, retriesRemaining);
        };

        QTimer::singleShot(100, this, deleteFenceFilefunctor);
    }
}

void AssetRequestHandler::OnNewIncomingRequest(unsigned int connId, unsigned int serial, QByteArray payload, QString platform)
{
    AZ::SerializeContext* serializeContext = nullptr;
    EBUS_EVENT_RESULT(serializeContext, AZ::ComponentApplicationBus, GetSerializeContext);
    AZ_Assert(serializeContext, "Unable to retrieve serialize context.");
    BaseAssetProcessorMessage* message = AZ::Utils::LoadObjectFromBuffer<BaseAssetProcessorMessage>(payload.constData(), payload.size(), serializeContext);
    if (!message)
    {
        AZ_Warning("Asset Request Handler", false, "OnNewIncomingRequest: Invalid object sent as network message to AssetRequestHandler.");
        return;
    }

    NetworkRequestID key(connId, serial);
    QString fenceFileName;
    if (message->RequireFencing())
    {
        bool successfullyCreatedFenceFile = false;
        int fenceID = 0;
        for (int idx = 0; idx < g_RetriesForFenceFile; ++idx)
        {
            fenceID = ++m_fenceId;
            fenceFileName = CreateFenceFile(fenceID);
            if (!fenceFileName.isEmpty())
            {
                successfullyCreatedFenceFile = true;
                break;
            }
        }

        if (!successfullyCreatedFenceFile)
        {
            AZ_TracePrintf(AssetProcessor::DebugChannel, "AssetProcessor was unable to create the fence file");
            // send request to the appropriate handler with fencingfailed set to true and return
            if (InvokeHandler(message, key, platform, true))
            {
                delete message;
            }
        }
        else
        {
            // if we are here it means that we were able to create the fence file, we will try to delete it now with a fixed number of retries
            DeleteFenceFile_Retry(fenceID, fenceFileName, key, message, platform, g_RetriesForFenceFile);
        }
        return;
    }

    // If we are here it indicates that the request does not require fencing, we either call the required bus or invoke the handler directly
    if (InvokeHandler(message, key, platform))
    {
        delete message;
    }
    return;
}

void AssetRequestHandler::RequestReady(NetworkRequestID networkRequestId, BaseAssetProcessorMessage* message, QString platform,  bool fencingFailed)
{
    using namespace AzFramework::AssetSystem;

    if (message->GetMessageType() == RequestAssetStatus::MessageType())
    {
        ProcessAssetRequest(networkRequestId, message, platform, fencingFailed);
    }

    delete message;
}

void AssetRequestHandler::OnFenceFileDetected(unsigned int fenceId)
{
    auto fenceRequestFound = m_pendingFenceRequestMap.find(fenceId);
    if (fenceRequestFound == m_pendingFenceRequestMap.end())
    {
        AZ_TracePrintf(AssetProcessor::DebugChannel, "OnFenceFileDetected: Fence File Request not found, ignoring.\n");
        return;
    }

    if (InvokeHandler(fenceRequestFound.value().m_message, fenceRequestFound.value().m_requestId, fenceRequestFound.value().m_platform))
    {
        delete fenceRequestFound.value().m_message;
    }
    m_pendingFenceRequestMap.erase(fenceRequestFound);
}


#include <native/AssetManager/AssetRequestHandler.moc>
