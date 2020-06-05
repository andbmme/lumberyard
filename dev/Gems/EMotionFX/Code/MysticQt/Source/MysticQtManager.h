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

#ifndef __MYSTICQT_MANAGER_H
#define __MYSTICQT_MANAGER_H

// include required files
#include <MCore/Source/StandardHeaders.h>
#include <MCore/Source/Array.h>
#include "MysticQtConfig.h"
#include <QWidget>


namespace MysticQt
{
    // the initializer
    class MYSTICQT_API Initializer
    {
    public:
        static bool MCORE_CDECL Init(const char* appDir, const char* dataDir = "");
        static void MCORE_CDECL Shutdown();
    };


    // the MysticQt manager class
    class MYSTICQT_API MysticQtManager
    {
        friend class Initializer;
        MCORE_MEMORYOBJECTCATEGORY(MysticQtManager, MCore::MCORE_DEFAULT_ALIGNMENT, MEMCATEGORY_MYSTICQT);

    public:
        MCORE_INLINE QWidget* GetMainWindow() const                         { return mMainWindow; }
        MCORE_INLINE void SetMainWindow(QWidget* mainWindow)                { mMainWindow = mainWindow; }

        MCORE_INLINE void SetAppDir(const char* appDir)
        {
            mAppDir = appDir;
            if (mDataDir.size() == 0)
            {
                mDataDir = appDir;
            }
        }
        MCORE_INLINE const AZStd::string& GetAppDir() const                 { return mAppDir; }

        MCORE_INLINE void SetDataDir(const char* dataDir)
        {
            mDataDir = dataDir;
            if (mAppDir.size() == 0)
            {
                mAppDir = dataDir;
            }
        }
        MCORE_INLINE const AZStd::string& GetDataDir() const                { return mDataDir; }

        const QIcon& FindIcon(const char* filename);

    private:
        struct MYSTICQT_API IconData
        {
            MCORE_MEMORYOBJECTCATEGORY(MysticQtManager::IconData, MCore::MCORE_DEFAULT_ALIGNMENT, MEMCATEGORY_MYSTICQT)

            IconData(const char* filename);
            ~IconData();

            QIcon*          mIcon;
            AZStd::string   mFileName;
        };

        QWidget*                            mMainWindow;
        MCore::Array<IconData*>             mIcons;
        AZStd::string                       mAppDir;
        AZStd::string                       mDataDir;

        MysticQtManager();
        ~MysticQtManager();
    };


    // the global
    extern MYSTICQT_API MysticQtManager* gMysticQtManager;

    // shortcuts
    MCORE_INLINE MysticQtManager*           GetMysticQt()                   { return MysticQt::gMysticQtManager; }
    MCORE_INLINE const AZStd::string&       GetAppDir()                     { return gMysticQtManager->GetAppDir(); }
    MCORE_INLINE const AZStd::string&       GetDataDir()                    { return gMysticQtManager->GetDataDir(); }
}   // namespace MysticQt

#endif
