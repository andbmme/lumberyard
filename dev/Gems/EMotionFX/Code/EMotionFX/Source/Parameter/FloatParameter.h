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

#include "RangedValueParameter.h"

namespace EMotionFX
{
    class FloatParameter 
        : public RangedValueParameter<float, FloatParameter>
    {
        using BaseType = RangedValueParameter<float, FloatParameter>;
    public:
        AZ_RTTI(FloatParameter, "{0F0B8531-0B07-4D9B-A8AC-3A32D15E8762}", ValueParameter);
        AZ_CLASS_ALLOCATOR_DECL

        FloatParameter()
            : BaseType(0.0f, 0.0f, 1.0f)
        {
        }

        explicit FloatParameter(AZStd::string name, AZStd::string description = {})
            : BaseType(0.0f, 0.0f, 1.0f, true, true, AZStd::move(name), AZStd::move(description))
        {
        }

        static void Reflect(AZ::ReflectContext* context);

        MCore::Attribute* ConstructDefaultValueAsAttribute() const override;
        uint32 GetType() const override;
        bool AssignDefaultValueToAttribute(MCore::Attribute* attribute) const override;

        bool SetDefaultValueFromAttribute(MCore::Attribute* attribute) override;
        bool SetMinValueFromAttribute(MCore::Attribute* attribute) override;
        bool SetMaxValueFromAttribute(MCore::Attribute* attribute) override;

        static float GetUnboundedMinValue();
        static float GetUnboundedMaxValue();
    };
} 
