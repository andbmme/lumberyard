/*
* All or portions of this file Copyright (c) Amazon.com, Inc. or its affiliates, or 
* a third party where indicated.
*
* For complete copyright and license terms please see the LICENSE at the root of this
* distribution (the "License"). All use of this software is governed by the License,  
* or, if provided, by the license below or the license accompanying this file. Do not
* remove or modify any license notices. This file is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  
*
*/
#include "StdAfx.h"

#include "ReflectedVar.h"
#include <AzToolsFramework/UI/PropertyEditor/ReflectedPropertyEditor.hxx>
#include <AzCore/Component/ComponentApplicationBus.h>

bool ReflectedVarInit::s_reflectionDone = false;

void ReflectedVarInit::setupReflection(AZ::SerializeContext* serializeContext)
{
    if (!serializeContext)
        return;

    if (s_reflectionDone)
        return;

    s_reflectionDone = true;

    serializeContext->Class< CReflectedVar>()
        ->Version(1)
        ->Field("description", &CReflectedVar::m_description)
        ->Field("varName", &CReflectedVar::m_varName);

    serializeContext->Class <CReflectedVarAnimation, CReflectedVar >()
        ->Version(1)
        ->Field("animation", &CReflectedVarAnimation::m_animation)
        ->Field("entityID", &CReflectedVarAnimation::m_entityID)
        ;

    serializeContext->Class <CReflectedVarResource, CReflectedVar >()
        ->Version(1)
        ->Field("path", &CReflectedVarResource::m_path)
        ->Field("propertyType", &CReflectedVarResource::m_propertyType)
        ;

    serializeContext->Class< CReflectedVarColor, CReflectedVar>()
        ->Version(1)
        ->Field("color", &CReflectedVarColor::m_color);

    serializeContext->Class< CReflectedVarUser, CReflectedVar>()
        ->Version(1)
        ->Field("value", &CReflectedVarUser::m_value)
        ->Field("enableEdit", &CReflectedVarUser::m_enableEdit)
        ->Field("title", &CReflectedVarUser::m_dialogTitle)
        ->Field("useTree", &CReflectedVarUser::m_useTree)
        ->Field("treeSeparator", &CReflectedVarUser::m_treeSeparator)
        ->Field("itemNames", &CReflectedVarUser::m_itemNames)
        ->Field("itemDescriptions", &CReflectedVarUser::m_itemDescriptions);

    serializeContext->Class <CReflectedVarSpline, CReflectedVar >()
        ->Version(1)
        ->Field("spline", &CReflectedVarSpline::m_spline)
        ->Field("propertyType", &CReflectedVarSpline::m_propertyType)
        ;

    serializeContext->Class< CPropertyContainer, CReflectedVar>()
        ->Version(1)
        ->Field("properties", &CPropertyContainer::m_properties);

    serializeContext->Class <CReflectedVarMotion, CReflectedVar >()
        ->Version(1)
        ->Field("motion", &CReflectedVarMotion::m_motion)
        ->Field("assetId", &CReflectedVarMotion::m_assetId)
        ;

    AZ::EditContext* ec = serializeContext->GetEditContext();
    if (ec)
    {
        ec->Class< CReflectedVarAnimation >("VarAnimation", "Animation")
            ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
            ->Attribute(AZ::Edit::Attributes::NameLabelOverride, &CReflectedVarAnimation::varName)
            ->Attribute(AZ::Edit::Attributes::DescriptionTextOverride, &CReflectedVarAnimation::description)
            ;

        ec->Class< CReflectedVarResource >("VarResource", "Resource")
            ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
            ->Attribute(AZ::Edit::Attributes::NameLabelOverride, &CReflectedVarResource::varName)
            ->Attribute(AZ::Edit::Attributes::DescriptionTextOverride, &CReflectedVarResource::description)
            ;

        ec->Class< CReflectedVarUser >("VarUser", "")
            ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
            ->Attribute(AZ::Edit::Attributes::NameLabelOverride, &CReflectedVarUser::varName)
            ->Attribute(AZ::Edit::Attributes::Handler, AZ_CRC("ePropertyUser", 0x65b972c0))
            ;

        ec->Class< CReflectedVarColor >("VarColor", "")
            ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
            ->Attribute(AZ::Edit::Attributes::Visibility, AZ_CRC("PropertyVisibility_ShowChildrenOnly", 0xef428f20))
            ->DataElement(AZ::Edit::UIHandlers::Color, &CReflectedVarColor::m_color, "Color", "")
            ->Attribute(AZ::Edit::Attributes::NameLabelOverride, &CReflectedVarColor::varName)
            ->Attribute(AZ::Edit::Attributes::DescriptionTextOverride, &CReflectedVarColor::description)
            ;

        ec->Class< CReflectedVarSpline >("VarSpline", "")
            ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
            ->Attribute(AZ::Edit::Attributes::NameLabelOverride, &CReflectedVarSpline::varName)
            ->Attribute(AZ::Edit::Attributes::Handler, &CReflectedVarSpline::handler)
            ;

        ec->Class< CPropertyContainer >("PropertyContainer", "")
            ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
            ->Attribute(AZ::Edit::Attributes::Visibility, AZ_CRC("PropertyVisibility_ShowChildrenOnly", 0xef428f20))
            ->DataElement(AZ::Edit::UIHandlers::Default, &CPropertyContainer::m_properties, "Properties", "")
            ->Attribute(AZ::Edit::Attributes::ContainerCanBeModified, false)
            ->Attribute(AZ::Edit::Attributes::NameLabelOverride, &CPropertyContainer::varName)
            ->Attribute(AZ::Edit::Attributes::DescriptionTextOverride, &CPropertyContainer::description)
            ->Attribute(AZ::Edit::Attributes::Visibility, &CPropertyContainer::GetVisibility)
            ->Attribute(AZ::Edit::Attributes::AutoExpand, &CPropertyContainer::m_autoExpand)
            ->Attribute(AZ::Edit::Attributes::ValueText, &CPropertyContainer::m_valueText) //will be ignored if blank
            ;

        ec->Class< CReflectedVarMotion >("VarMotion", "Motion")
            ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
            ->Attribute(AZ::Edit::Attributes::NameLabelOverride, &CReflectedVarMotion::varName)
            ->Attribute(AZ::Edit::Attributes::DescriptionTextOverride, &CReflectedVarMotion::description)
            ;
    }
    CReflectedVarString::reflect(serializeContext);
    CReflectedVarBool::reflect(serializeContext);
    CReflectedVarFloat::reflect(serializeContext);
    CReflectedVarInt::reflect(serializeContext);
    CReflectedVarVector2::reflect(serializeContext);
    CReflectedVarVector3::reflect(serializeContext);
    CReflectedVarVector4::reflect(serializeContext);
    CReflectedVarAny<AZStd::vector<AZStd::string>>::reflect(serializeContext);
    CReflectedVarEnum<int>::reflect(serializeContext);
    CReflectedVarEnum<AZStd::string>::reflect(serializeContext);
    CReflectedVarGenericProperty::reflect(serializeContext);
}


template<class T>
void CReflectedVarAny<T>::reflect(AZ::SerializeContext* serializeContext)
{
    static bool reflected = false;
    if (reflected)
        return;
    reflected = true;

    serializeContext->Class< CReflectedVarAny<T>, CReflectedVar>()
        ->Version(1)
        ->Field("value", &CReflectedVarAny<T>::m_value);

    AZ::EditContext* ec = serializeContext->GetEditContext();
    if (ec)
    {
        ec->Class< CReflectedVarAny<T> >("VarAny", "")
            ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
            ->Attribute(AZ::Edit::Attributes::Visibility, AZ_CRC("PropertyVisibility_ShowChildrenOnly", 0xef428f20))
            ->DataElement(AZ::Edit::UIHandlers::Default, &CReflectedVarAny<T>::m_value, "Value", "")
            ->Attribute(AZ::Edit::Attributes::NameLabelOverride, &CReflectedVarAny<T>::varName)
            ->Attribute(AZ::Edit::Attributes::DescriptionTextOverride, &CReflectedVarAny<T>::description)
            ;
    }
}


template<class T, class R>
void CReflectedVarRanged<T, R>::reflect(AZ::SerializeContext* serializeContext)
{
    static bool reflected = false;
    if (reflected)
        return;
    reflected = true;

    serializeContext->Class< CReflectedVarRanged<T, R>, CReflectedVar>()
        ->Version(1)
        ->Field("value", &CReflectedVarRanged<T, R>::m_value)
        ->Field("min", &CReflectedVarRanged<T, R>::m_minVal)
        ->Field("max", &CReflectedVarRanged<T, R>::m_maxVal)
        ->Field("step", &CReflectedVarRanged<T, R>::m_stepSize)
        ->Field("softMin", &CReflectedVarRanged<T, R>::m_softMinVal)
        ->Field("softMax", &CReflectedVarRanged<T, R>::m_softMaxVal)
        ;

    AZ::EditContext* ec = serializeContext->GetEditContext();
    if (ec)
    {
        ec->Class< CReflectedVarRanged<T, R> >("VarAny", "")
            ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
            ->Attribute(AZ::Edit::Attributes::Visibility, AZ_CRC("PropertyVisibility_ShowChildrenOnly", 0xef428f20))
            ->DataElement(AZ::Edit::UIHandlers::Slider, &CReflectedVarRanged<T, R>::m_value, "Value", "")
            ->Attribute(AZ::Edit::Attributes::NameLabelOverride, &CReflectedVarRanged<T, R>::varName)
            ->Attribute(AZ::Edit::Attributes::DescriptionTextOverride, &CReflectedVarRanged<T, R>::description)
            ->Attribute(AZ::Edit::Attributes::Min, &CReflectedVarRanged<T, R>::minValue)
            ->Attribute(AZ::Edit::Attributes::Max, &CReflectedVarRanged<T, R>::maxValue)
            ->Attribute(AZ::Edit::Attributes::Step, &CReflectedVarRanged<T, R>::stepSize)
            ->Attribute(AZ::Edit::Attributes::SoftMin, &CReflectedVarRanged<T, R>::softMinVal)
            ->Attribute(AZ::Edit::Attributes::SoftMax, &CReflectedVarRanged<T, R>::softMaxVal)
            ;
    }

}


template<class T>
void CReflectedVarEnum<T>::reflect(AZ::SerializeContext* serializeContext)
{
    static bool reflected = false;
    if (reflected)
        return;
    reflected = true;

    serializeContext->Class< CReflectedVarEnum<T>, CReflectedVar>()
        ->Version(1)
        ->Field("value", &CReflectedVarEnum<T>::m_value)
        ->Field("selectedName", &CReflectedVarEnum<T>::m_selectedEnumName)
        ->Field("availableValues", &CReflectedVarEnum<T>::m_enums)
        ;

    AZ::EditContext* ec = serializeContext->GetEditContext();
    if (ec)
    {
        ec->Class< CReflectedVarEnum<T> >("Enum Variable", "")
            ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
            ->Attribute(AZ::Edit::Attributes::Visibility, AZ_CRC("PropertyVisibility_ShowChildrenOnly", 0xef428f20))
            ->DataElement(AZ::Edit::UIHandlers::ComboBox, &CReflectedVarEnum<T>::m_selectedEnumName, "Value", "")
            ->Attribute(AZ::Edit::Attributes::StringList, &CReflectedVarEnum<T>::GetEnums)
            ->Attribute(AZ::Edit::Attributes::ChangeNotify, &CReflectedVarEnum<T>::OnEnumChanged)
            ->Attribute(AZ::Edit::Attributes::NameLabelOverride, &CReflectedVarEnum<T>::varName)
            ->Attribute(AZ::Edit::Attributes::DescriptionTextOverride, &CReflectedVarEnum<T>::description)
            ;
    }
}


void CReflectedVarGenericProperty::reflect(AZ::SerializeContext* serializeContext)
{
    static bool reflected = false;
    if (reflected)
        return;
    reflected = true;

    serializeContext->Class< CReflectedVarGenericProperty, CReflectedVar>()
        ->Version(1)
        ->Field("value", &CReflectedVarGenericProperty::m_value)
        ->Field("propertyType", &CReflectedVarGenericProperty::m_propertyType)
        ;

    AZ::EditContext* ec = serializeContext->GetEditContext();
    if (ec)
    {
        ec->Class< CReflectedVarGenericProperty >("GenericProperty", "GenericProperty")
            ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
            ->Attribute(AZ::Edit::Attributes::NameLabelOverride, &CReflectedVarGenericProperty::varName)
            ->Attribute(AZ::Edit::Attributes::DescriptionTextOverride, &CReflectedVarGenericProperty::description)
            ->Attribute(AZ::Edit::Attributes::Handler, &CReflectedVarGenericProperty::handler)
            ;
    }

}

AZ::u32 CReflectedVarSpline::handler()
{
    switch (m_propertyType)
    {
    case ePropertyFloatCurve:
        return AZ_CRC("ePropertyFloatCurve", 0x7440ccce);
    case ePropertyColorCurve:
        return AZ_CRC("ePropertyColorCurve", 0xa30da4ec);
    default:
        AZ_Assert(false, "CReflectedVarSpline property type must be ePropertyFloatCurve or ePropertyColorCurve");
        return AZ::Edit::UIHandlers::Default;
    }
}


AZ::u32 CReflectedVarGenericProperty::handler()
{
    switch (m_propertyType)
    {
    case ePropertyShader:
        return AZ_CRC("ePropertyShader", 0xc40932f1);
    case ePropertyMaterial:
        return AZ_CRC("ePropertyMaterial", 0xf324dffa);
    case ePropertyAiBehavior:
        return AZ_CRC("ePropertyAiBehavior", 0xa780fd1a);
    case ePropertyAiAnchor:
        return AZ_CRC("ePropertyAiAnchor", 0x3e446ccb);
#ifdef USE_DEPRECATED_AI_CHARACTER_SYSTEM
    case ePropertyAiCharacter:
        return AZ_CRC("ePropertyAiCharacter", 0xa5e5d19f);
#endif
    case ePropertyAiPFPropertiesList:
        return AZ_CRC("ePropertyAiPFPropertiesList", 0x9b406f43);
    case ePropertyAiEntityClasses:
        return AZ_CRC("ePropertyAiEntityClasses", 0xd50f1b94);
    case ePropertySOClass:
        return AZ_CRC("ePropertySOClass", 0x6d13d619);
    case ePropertySOClasses:
        return AZ_CRC("ePropertySOClasses", 0x64ef1e71);
    case ePropertySOState:
        return AZ_CRC("ePropertySOState", 0x23cb1d7d);
    case ePropertySOStates:
        return AZ_CRC("ePropertySOStates", 0x35990997);
    case ePropertySOStatePattern:
        return AZ_CRC("ePropertySOStatePattern", 0xbd09853a);
    case ePropertySOAction:
        return AZ_CRC("ePropertySOAction", 0x4397f248);
    case ePropertySOHelper:
        return AZ_CRC("ePropertySOHelper", 0x836c056a);
    case ePropertySONavHelper:
        return AZ_CRC("ePropertySONavHelper", 0x1abfbd59);
    case ePropertySOAnimHelper:
        return AZ_CRC("ePropertySOAnimHelper", 0x139a4d89);
    case ePropertySOEvent:
        return AZ_CRC("ePropertySOEvent", 0xbbf6c521);
    case ePropertySOTemplate:
        return AZ_CRC("ePropertySOTemplate", 0x5b0a6a76);
    case ePropertyEquip:
        return AZ_CRC("ePropertyEquip", 0x66ffd290);
    case ePropertyReverbPreset:
        return AZ_CRC("ePropertyReverbPreset", 0x51469f38);
    case ePropertyDeprecated0:
        return AZ_CRC("ePropertyCustomAction", 0x4ffa5ba5);
    case ePropertyGameToken:
        return AZ_CRC("ePropertyGameToken", 0x34855b6f);
    case ePropertyMissionObj:
        return AZ_CRC("ePropertyMissionObj", 0x4a2d0dc8);
    case ePropertySequence:
        return AZ_CRC("ePropertySequence", 0xdd1c7d44);
    case ePropertySequenceId:
        return AZ_CRC("ePropertySequenceId", 0x05983dcc);
    case ePropertyLocalString:
        return AZ_CRC("ePropertyLocalString", 0x0cd9609a);
    case ePropertyLightAnimation:
        return AZ_CRC("ePropertyLightAnimation", 0x277097da);
    case ePropertyParticleName:
        return AZ_CRC("ePropertyParticleName", 0xf44c7133);
    case ePropertyFlare:
        return AZ_CRC("ePropertyFlare", 0x5ce803df);
    default:
        AZ_Assert(false, "No property handlers defined for the property type");
        return AZ_CRC("Default", 0xe35e00df);
    }
}


void CPropertyContainer::AddProperty(CReflectedVar *property)
{
    if (property)
        m_properties.push_back(property);
}

void CPropertyContainer::Clear()
{
    m_properties.clear();
}
