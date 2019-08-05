// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

#pragma once

#include "Unity/IUnityGraphics.h"

struct IUnityDeviceResource
{
    virtual void __stdcall ProcessDeviceEvent(_In_ UnityGfxDeviceEventType type, _In_ IUnityInterfaces* interfaces) = 0;
    virtual bool __stdcall GetUsesReverseZ() = 0;
};

// Create a graphics API implementation instance for the given API type.
std::shared_ptr<IUnityDeviceResource> CreateDeviceResource(_In_ UnityGfxRenderer apiType);
