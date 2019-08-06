﻿// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

#pragma once

#include "Plugin.CaptureEngine.g.h"
#include "Plugin.Module.h"
#include "Media.Capture.Sink.h"
#include "Media.Capture.PayloadHandler.h"
#include "Media.SharedTexture.h"

#include <mfapi.h>
#include <winrt/windows.media.h>
#include <winrt/Windows.Media.Capture.h>

namespace winrt::CameraCapture::Plugin::implementation
{
    struct CaptureEngine : CaptureEngineT<CaptureEngine, Module>
    {
        static Plugin::IModule Create(
            _In_ std::weak_ptr<IUnityDeviceResource> const& unityDevice,
            _In_ StateChangedCallback fnCallback,
            _In_ void* pCallbackObject);

        CaptureEngine();

        virtual void Shutdown();

        HRESULT StartPreview(uint32_t width, uint32_t height, bool enableAudio, bool enableMrc);
        HRESULT StopPreview();

        Windows::Perception::Spatial::SpatialCoordinateSystem AppCoordinateSystem()
        {
            return m_appCoordinateSystem;
        }
        void AppCoordinateSystem(Windows::Perception::Spatial::SpatialCoordinateSystem const& value)
        {
            m_appCoordinateSystem = value;
        }

    private:
        HRESULT CreateDeviceResources();
        void ReleaseDeviceResources();

        Windows::Foundation::IAsyncAction StartPreviewCoroutine(uint32_t const width, uint32_t const height, boolean const enableAudio, boolean const enableMrc);
        Windows::Foundation::IAsyncAction StopPreviewCoroutine();

        Windows::Foundation::IAsyncAction CreateMediaCaptureAsync(boolean const enableAudio, Windows::Media::Capture::MediaCategory const category);
        Windows::Foundation::IAsyncAction ReleaseMediaCaptureAsync();

        Windows::Foundation::IAsyncAction AddMrcEffectsAsync(boolean const enableAudio);
        Windows::Foundation::IAsyncAction RemoveMrcEffectsAsync();

    private:
        slim_mutex m_mutex;
        std::atomic<boolean> m_isShutdown;

        Windows::Foundation::IAsyncAction m_startPreviewOp;
        Windows::Foundation::IAsyncAction m_stopPreviewOp;

        com_ptr<ID3D11Device> m_d3dDevice;
        uint32_t m_resetToken;
        com_ptr<IMFDXGIDeviceManager> m_dxgiDeviceManager;

        // media capture
		Windows::Media::Capture::MediaCategory m_category;
		Windows::Media::Capture::MediaStreamType m_streamType;
		Windows::Media::Capture::KnownVideoProfile m_videoProfile;
        Windows::Media::Capture::MediaCapture m_mediaCapture;
		Windows::Media::Capture::MediaCaptureInitializationSettings m_initSettings;

        Windows::Media::IMediaExtension m_mrcAudioEffect;
        Windows::Media::IMediaExtension m_mrcVideoEffect;
        Windows::Media::IMediaExtension m_mrcPreviewEffect;

        // IMFMediaSink
        Media::Capture::Sink m_mediaSink;
        Media::Capture::PayloadHandler m_payloadHandler;
        event_token m_sampleEventToken;

        // buffers
        com_ptr<IMFSample> m_audioSample;
        com_ptr<SharedTexture> m_videoBuffer;

        Windows::Perception::Spatial::SpatialCoordinateSystem m_appCoordinateSystem;
    };
}

namespace winrt::CameraCapture::Plugin::factory_implementation
{
    struct CaptureEngine : CaptureEngineT<CaptureEngine, implementation::CaptureEngine>
    {
    };
}
