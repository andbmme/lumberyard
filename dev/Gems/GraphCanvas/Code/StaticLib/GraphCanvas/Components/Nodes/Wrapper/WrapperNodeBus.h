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

#include <qstring.h>

#include <AzCore/Component/EntityId.h>
#include <AzCore/EBus/EBus.h>
#include <AzCore/Math/Vector2.h>

class QGraphicsLayoutItem;
class QMimeData;

namespace GraphCanvas
{
    static const AZ::Crc32 WrapperNodeLayoutServiceCrc = AZ_CRC("GraphCanvas_WrapperNodeLayoutService", 0x4033e2f5);

    // WrappedNodeConfiguration
    // This contains the configuration required to display the Wrapped node inside of the WrapperNode.
    // This enable nodes to either be forced into a certain ordering, or to just show up in whatever
    // order they were added.
    //
    // Manaul Node Ordering(say from a user dragging a node around inside of a wrapper node), is currently unsupported.
    struct WrappedNodeConfiguration
    {
        friend class WrapperNodeLayoutComponent;

        AZ_TYPE_INFO(WrappedNodeConfiguration, "{55C674CA-2AB3-4D60-A687-D4DBC98F1E95}");
        AZ_CLASS_ALLOCATOR(WrappedNodeConfiguration, AZ::SystemAllocator, 0);

        WrappedNodeConfiguration()
            : m_layoutOrder(-1)
            , m_elementOrdering(-1)
        {
        }

        WrappedNodeConfiguration(AZ::u32 layoutOrder)
            : m_layoutOrder(layoutOrder)
            , m_elementOrdering(-1)
        {
        }

        bool operator<(const WrappedNodeConfiguration& other) const
        {
            if (m_layoutOrder == other.m_layoutOrder)
            {
                return m_elementOrdering < other.m_elementOrdering;
            }

            return m_layoutOrder < other.m_layoutOrder;
        }

        // Controls the order that the node will be displayed
        AZ::u32 m_layoutOrder;

    private:
        AZ::u32 m_elementOrdering;
    };

    //! WrapperNodeLayoutRequests
    //! Requests that are serviced by a Wrapper Node Layout implementation.
    class WrapperNodeRequests : public AZ::EBusTraits
    {
    public:
        static const AZ::EBusAddressPolicy AddressPolicy = AZ::EBusAddressPolicy::ById;
        using BusIdType = AZ::EntityId;

        virtual void SetActionString(const QString& actionString) = 0;

        virtual AZStd::vector< AZ::EntityId > GetWrappedNodeIds() const = 0;

        virtual void WrapNode(const AZ::EntityId& nodeId, const WrappedNodeConfiguration& nodeConfiguration) = 0;
        virtual void UnwrapNode(const AZ::EntityId& nodeId) = 0;

        virtual void SetWrapperType(const AZ::Crc32& wrapperType) = 0;
        virtual AZ::Crc32 GetWrapperType() const = 0;
    };

    using WrapperNodeRequestBus = AZ::EBus<WrapperNodeRequests>;

    //! WrapperNodeNotificationBus
    //! Notifications that are generated by a WrapperNode.
    class WrapperNodeNotifications : public AZ::EBusTraits
    {
    public:
        static const AZ::EBusAddressPolicy AddressPolicy = AZ::EBusAddressPolicy::ById;
        using BusIdType = AZ::EntityId;

        virtual void OnWrappedNode(const AZ::EntityId& /*wrappedNode*/) {};
        virtual void OnUnwrappedNode(const AZ::EntityId& /*removedNode*/) {};
    };

    using WrapperNodeNotificationBus = AZ::EBus<WrapperNodeNotifications>;

    //! WrapperNodeConfigurationRequestBus
    //! Requests 
    class WrapperNodeConfigurationRequests : public AZ::EBusTraits
    {
    public:
        static const AZ::EBusAddressPolicy AddressPolicy = AZ::EBusAddressPolicy::ById;
        using BusIdType = AZ::EntityId;

        virtual WrappedNodeConfiguration GetWrappedNodeConfiguration(const AZ::EntityId& wrappedNodeId) const = 0;
    };

    using WrapperNodeConfigurationRequestBus = AZ::EBus<WrapperNodeConfigurationRequests>;

    class ForcedWrappedNodeRequests : public AZ::EBusTraits
    {
    public:
        static const AZ::EBusAddressPolicy AddressPolicy = AZ::EBusAddressPolicy::ById;
        using BusIdType = AZ::EntityId;

        virtual AZ::Crc32 GetWrapperType() const = 0;
        virtual AZ::Crc32 GetIdentifier() const = 0;

        virtual AZ::EntityId CreateWrapperNode(const AZ::EntityId& sceneId, const AZ::Vector2& nodePosition) = 0;
    };

    using ForcedWrappedNodeRequestBus = AZ::EBus<ForcedWrappedNodeRequests>;
}