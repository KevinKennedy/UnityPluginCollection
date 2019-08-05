﻿// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

#include "pch.h"
#include "Media.Capture.MrcAudioEffect.h"
#include "Media.Capture.MrcAudioEffect.g.cpp"

using namespace winrt;
using namespace CameraCapture::Media::Capture::implementation;

using winrtAudioMixerMode = CameraCapture::Media::Capture::AudioMixerMode;

MrcAudioEffect::MrcAudioEffect()
    : m_propertySet() 
{
}

hstring MrcAudioEffect::ActivatableClassId()
{
    return GetRuntimeClassName();
}

Windows::Foundation::Collections::IPropertySet MrcAudioEffect::Properties()
{
    return m_propertySet;
}

winrtAudioMixerMode MrcAudioEffect::MixerMode()
{
    AudioMixerMode result = DefaultAudioMixerMode;
    if (m_propertySet.HasKey(PROPERTY_MIXERMODE))
    {
        result = unbox_value<winrtAudioMixerMode>((m_propertySet.Lookup(PROPERTY_MIXERMODE)));
    }

    return result;
}

void MrcAudioEffect::MixerMode(winrtAudioMixerMode const& value)
{
    m_propertySet.Insert(PROPERTY_MIXERMODE, box_value<winrtAudioMixerMode, uint32_t>(value));
}
