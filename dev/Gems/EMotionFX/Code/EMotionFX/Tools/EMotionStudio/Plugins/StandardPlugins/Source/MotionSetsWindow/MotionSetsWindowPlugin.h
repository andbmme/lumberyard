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

#pragma once

#include "../StandardPluginsConfig.h"
#include "../../../../EMStudioSDK/Source/DockWidgetPlugin.h"
#include <MysticQt/Source/DialogStack.h>
#include <EMotionFX/Source/MotionSet.h>
#include <EMotionFX/Source/MotionManager.h>
#include <EMotionFX/CommandSystem/Source/CommandManager.h>
#include <EMotionFX/CommandSystem/Source/MotionSetCommands.h>
#include <EMotionFX/Source/EventHandler.h>
#include "MotionSetWindow.h"
#include "MotionSetManagementWindow.h"
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QTableWidget>
#include <QTreeWidget>
#include <QLineEdit>
#include <QDialog>


namespace EMStudio
{
    // forward declarations
    class SaveDirtyMotionSetFilesCallback;


    class MotionSetsWindowPlugin
        : public EMStudio::DockWidgetPlugin
    {
        Q_OBJECT
        MCORE_MEMORYOBJECTCATEGORY(MotionSetsWindowPlugin, MCore::MCORE_DEFAULT_ALIGNMENT, MEMCATEGORY_STANDARDPLUGINS);

    public:
        enum
        {
            CLASS_ID = 0x00000234
        };

        MotionSetsWindowPlugin();
        ~MotionSetsWindowPlugin();

        // overloaded
        const char* GetCompileDate() const override     { return MCORE_DATE; }
        const char* GetName() const override            { return "Motion Sets"; }
        uint32 GetClassID() const override              { return MotionSetsWindowPlugin::CLASS_ID; }
        const char* GetCreatorName() const override     { return "MysticGD"; }
        float GetVersion() const override               { return 1.0f;  }
        bool GetIsClosable() const override             { return true;  }
        bool GetIsFloatable() const override            { return true;  }
        bool GetIsVertical() const override             { return false; }

        // overloaded main init function
        bool Init() override;
        EMStudioPlugin* Clone() override;

        void ReInit();

        EMotionFX::MotionSet* GetSelectedSet() const;

        void SetSelectedSet(EMotionFX::MotionSet* motionSet);
        int SaveDirtyMotionSet(EMotionFX::MotionSet* motionSet, MCore::CommandGroup* commandGroup, bool askBeforeSaving, bool showCancelButton = true);

        MotionSetManagementWindow*  GetManagementWindow()                                                       { return mMotionSetManagementWindow; }
        MotionSetWindow*            GetMotionSetWindow()                                                        { return mMotionSetWindow; }

        int OnSaveDirtyMotionSets();
        void LoadMotionSet(AZStd::string filename);

        static bool GetMotionSetCommandInfo(MCore::Command* command, const MCore::CommandLine& parameters,EMotionFX::MotionSet** outMotionSet, MotionSetsWindowPlugin** outPlugin);

    public slots:
        void WindowReInit(bool visible);

    private:
        // declare the callbacks
        MCORE_DEFINECOMMANDCALLBACK(CommandCreateMotionSetCallback);
        MCORE_DEFINECOMMANDCALLBACK(CommandReinitCallback);
        MCORE_DEFINECOMMANDCALLBACK(CommandAdjustMotionSetCallback);
        MCORE_DEFINECOMMANDCALLBACK(CommandMotionSetAddMotionCallback);
        MCORE_DEFINECOMMANDCALLBACK(CommandMotionSetRemoveMotionCallback);
        MCORE_DEFINECOMMANDCALLBACK(CommandMotionSetAdjustMotionCallback);
        MCORE_DEFINECOMMANDCALLBACK(CommandLoadMotionSetCallback);

        CommandCreateMotionSetCallback*         mCreateMotionSetCallback;
        CommandReinitCallback*                  m_reinitCallback;
        CommandAdjustMotionSetCallback*         mAdjustMotionSetCallback;
        CommandMotionSetAddMotionCallback*      mMotionSetAddMotionCallback;
        CommandMotionSetRemoveMotionCallback*   mMotionSetRemoveMotionCallback;
        CommandMotionSetAdjustMotionCallback*   mMotionSetAdjustMotionCallback;
        CommandLoadMotionSetCallback*           mLoadMotionSetCallback;

        MotionSetManagementWindow*              mMotionSetManagementWindow;
        MotionSetWindow*                        mMotionSetWindow;

        MysticQt::DialogStack*                  mDialogStack;
        //MotionSetStringIDWindow*              mStringIDWindow;

        EMotionFX::MotionSet*                   mSelectedSet;

        SaveDirtyMotionSetFilesCallback*        mDirtyFilesCallback;
    };
} // namespace EMStudio