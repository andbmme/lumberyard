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

#include <Editor/PropertyWidgets/ActorGoalNodeHandler.h>
#include <Editor/ActorEditorBus.h>
#include <EMotionFX/Source/Attachment.h>
#include <EMotionFX/Tools/EMotionStudio/EMStudioSDK/Source/EMStudioManager.h>
#include <EMotionFX/Tools/EMotionStudio/EMStudioSDK/Source/NodeSelectionWindow.h>
#include <Editor/AnimGraphEditorBus.h>
#include <QHBoxLayout>
#include <QMessageBox>


namespace EMotionFX
{
    AZ_CLASS_ALLOCATOR_IMPL(ActorGoalNodePicker, EditorAllocator, 0)
    AZ_CLASS_ALLOCATOR_IMPL(ActorGoalNodeHandler, EditorAllocator, 0)

    ActorGoalNodePicker::ActorGoalNodePicker(QWidget* parent)
        : QWidget(parent)
    {
        QHBoxLayout* hLayout = new QHBoxLayout();
        hLayout->setMargin(0);

        m_pickButton = new QPushButton(this);
        connect(m_pickButton, &QPushButton::clicked, this, &ActorGoalNodePicker::OnPickClicked);
        hLayout->addWidget(m_pickButton);

        m_resetButton = new QPushButton(this);
        EMStudio::EMStudioManager::MakeTransparentButton(m_resetButton, "/Images/Icons/Clear.png", "Reset selection");
        connect(m_resetButton, &QPushButton::clicked, this, &ActorGoalNodePicker::OnResetClicked);
        hLayout->addWidget(m_resetButton);

        setLayout(hLayout);
    }


    void ActorGoalNodePicker::OnResetClicked()
    {
        if (m_goalNode.first.empty() && m_goalNode.second == 0)
        {
            return;
        }

        SetGoalNode(AZStd::make_pair(AZStd::string(), 0));
        emit SelectionChanged();
    }


    void ActorGoalNodePicker::UpdateInterface()
    {
        if (m_goalNode.first.empty())
        {
            m_pickButton->setText("Select node");
            m_resetButton->setVisible(false);
        }
        else
        {
            m_pickButton->setText(m_goalNode.first.c_str());
            m_resetButton->setVisible(true);
        }
    }


    void ActorGoalNodePicker::SetGoalNode(const AZStd::pair<AZStd::string, int>& goalNode)
    {
        m_goalNode = goalNode;
        UpdateInterface();
    }


    AZStd::pair<AZStd::string, int> ActorGoalNodePicker::GetGoalNode() const
    {
        return m_goalNode;
    }


    void ActorGoalNodePicker::OnPickClicked()
    {
        EMotionFX::ActorInstance* actorInstance = nullptr;
        ActorEditorRequestBus::BroadcastResult(actorInstance, &ActorEditorRequests::GetSelectedActorInstance);
        if (!actorInstance)
        {
            QMessageBox::warning(this, "No Actor Instance", "Cannot open node selection window. No valid actor instance selected.");
            return;
        }
        EMotionFX::Actor* actor = actorInstance->GetActor();

        // Create and show the node picker window
        EMStudio::NodeSelectionWindow nodeSelectionWindow(this, true);
        nodeSelectionWindow.GetNodeHierarchyWidget()->SetSelectionMode(true);

        CommandSystem::SelectionList prevSelection;
        EMotionFX::Node* node = actor->GetSkeleton()->FindNodeByName(m_goalNode.first.c_str());
        if (node)
        {
            prevSelection.AddNode(node);
        }


        MCore::Array<uint32> actorInstanceIDs;

        // Add the current actor instance and all the ones it is attached to
        EMotionFX::ActorInstance* currentInstance = actorInstance;
        while (currentInstance)
        {
            actorInstanceIDs.Add(currentInstance->GetID());
            EMotionFX::Attachment* attachment = currentInstance->GetSelfAttachment();
            if (attachment)
            {
                currentInstance = attachment->GetAttachToActorInstance();
            }
            else
            {
                currentInstance = nullptr;
            }
        }


        nodeSelectionWindow.Update(actorInstanceIDs, &prevSelection);
        nodeSelectionWindow.setModal(true);

        if (nodeSelectionWindow.exec() != QDialog::Rejected)
        {
            AZStd::vector<SelectionItem>& newSelection = nodeSelectionWindow.GetNodeHierarchyWidget()->GetSelectedItems();
            if (newSelection.size() == 1)
            {
                AZStd::string selectedNodeName = newSelection[0].GetNodeName();
                AZ::u32 selectedActorInstanceId = newSelection[0].mActorInstanceID;

                uint32 parentDepth = actorInstanceIDs.Find(selectedActorInstanceId);
                AZ_Assert(parentDepth != MCORE_INVALIDINDEX32, "Cannot get parent depth. The selected actor instance was not shown in the selection window.");

                m_goalNode = AZStd::make_pair<AZStd::string, int>(selectedNodeName, parentDepth);

                UpdateInterface();
                emit SelectionChanged();
            }
        }
    }

    //---------------------------------------------------------------------------------------------------------------------------------------------------------

    AZ::u32 ActorGoalNodeHandler::GetHandlerName() const
    {
        return AZ_CRC("ActorGoalNode", 0xaf1e8a3a);
    }


    QWidget* ActorGoalNodeHandler::CreateGUI(QWidget* parent)
    {
        ActorGoalNodePicker* picker = aznew ActorGoalNodePicker(parent);

        connect(picker, &ActorGoalNodePicker::SelectionChanged, this, [picker]()
        {
            EBUS_EVENT(AzToolsFramework::PropertyEditorGUIMessages::Bus, RequestWrite, picker);
        });

        return picker;
    }


    void ActorGoalNodeHandler::ConsumeAttribute(ActorGoalNodePicker* GUI, AZ::u32 attrib, AzToolsFramework::PropertyAttributeReader* attrValue, const char* debugName)
    {
        if (attrib == AZ::Edit::Attributes::ReadOnly)
        {
            bool value;
            if (attrValue->Read<bool>(value))
            {
                GUI->setEnabled(!value);
            }
        }
    }


    void ActorGoalNodeHandler::WriteGUIValuesIntoProperty(size_t index, ActorGoalNodePicker* GUI, property_t& instance, AzToolsFramework::InstanceDataNode* node)
    {
        instance = GUI->GetGoalNode();
    }


    bool ActorGoalNodeHandler::ReadValuesIntoGUI(size_t index, ActorGoalNodePicker* GUI, const property_t& instance, AzToolsFramework::InstanceDataNode* node)
    {
        QSignalBlocker signalBlocker(GUI);
        GUI->SetGoalNode(instance);
        return true;
    }
} // namespace EMotionFX

#include <Source/Editor/PropertyWidgets/ActorGoalNodeHandler.moc>