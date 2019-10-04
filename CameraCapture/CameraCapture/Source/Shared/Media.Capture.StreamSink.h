﻿// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

#pragma once

#include "Media.Capture.StreamSink.g.h"

#include <mfapi.h>
#include <mfidl.h>
#include <mferror.h>

#define MAX_SAMPLE_REQUESTS 2

namespace winrt::CameraCapture::Media::Capture::implementation
{
    struct StreamSink : StreamSinkT<StreamSink, IMFStreamSink, IMFMediaEventGenerator, IMFMediaTypeHandler>
    {
        StreamSink() = delete;
        StreamSink(
            uint8_t index, 
            Windows::Media::MediaProperties::IMediaEncodingProperties const& encodingProperties, 
            Capture::Sink const& parent);
        StreamSink(
            uint8_t index, 
            IMFMediaType* pMediaType, 
            Capture::Sink const& parent);
        ~StreamSink() { Shutdown(); }

        void SetProperties(Windows::Foundation::Collections::IPropertySet const& configuration);

        // IMFStreamSink
        STDOVERRIDEMETHODIMP GetMediaSink(
            _COM_Outptr_ IMFMediaSink **ppMediaSink);
        STDOVERRIDEMETHODIMP GetIdentifier(
            _Out_ DWORD *pdwIdentifier);
        STDOVERRIDEMETHODIMP GetMediaTypeHandler(
            _COM_Outptr_ IMFMediaTypeHandler **ppHandler);
        STDOVERRIDEMETHODIMP Flush();
        STDOVERRIDEMETHODIMP ProcessSample(
            _In_opt_ IMFSample *pSample);
        STDOVERRIDEMETHODIMP PlaceMarker(
            _In_ MFSTREAMSINK_MARKER_TYPE eMarkerType,
            _In_ const PROPVARIANT * pvarMarkerValue,
            _In_ const PROPVARIANT * pvarContextValue);

        // IMFMediaEventGenerator
        STDOVERRIDEMETHODIMP BeginGetEvent(
            _In_ IMFAsyncCallback *pCallback,
            _In_opt_ IUnknown *punkState);
        STDOVERRIDEMETHODIMP EndGetEvent(
            _In_ IMFAsyncResult *pResult,
            _COM_Outptr_ IMFMediaEvent **ppEvent);
        STDOVERRIDEMETHODIMP GetEvent(
            _In_ DWORD dwFlags,
            _COM_Outptr_ IMFMediaEvent **ppEvent);
        STDOVERRIDEMETHODIMP QueueEvent(
            _In_ MediaEventType met,
            _In_ REFGUID guidExtendedType,
            _In_ HRESULT hrStatus,
            _In_opt_ const PROPVARIANT *pvValue);

        // IMFMediaTypeHandler
        STDOVERRIDEMETHODIMP GetCurrentMediaType(
            _COM_Outptr_ IMFMediaType **ppMediaType);
        STDOVERRIDEMETHODIMP GetMajorType(
            _Out_ GUID *pguidMajorType);
        STDOVERRIDEMETHODIMP GetMediaTypeByIndex(
            _In_ DWORD dwIndex,
            _COM_Outptr_ IMFMediaType **ppMediaType);
        STDOVERRIDEMETHODIMP GetMediaTypeCount(
            _Out_ DWORD *pdwTypeCount);
        STDOVERRIDEMETHODIMP IsMediaTypeSupported(
            _In_ IMFMediaType *pMediaType,
            _COM_Outptr_ IMFMediaType **ppMediaType);
        STDOVERRIDEMETHODIMP SetCurrentMediaType(
            _In_ IMFMediaType *pMediaType);

        // StreamSink
        HRESULT Start(int64_t systemTime, int64_t clockStartOffset);
        HRESULT Stop();
        HRESULT Shutdown();

        Capture::State State() { return m_currentState; }
        void State(Capture::State const& value) { m_currentState = value; }

    private:
        STDMETHODIMP CheckShutdown()
        {
            auto guard = m_cs.Guard();

            if (m_currentState == State::Shutdown)
            {
                return MF_E_SHUTDOWN;
            }

            return S_OK;
        }

        STDMETHODIMP VerifyMediaType(
            _In_ IMFMediaType* pMediaType) const;

        void SetStartTimeOffset(
            _In_ LONGLONG firstSampleTime)
        {
            auto guard = m_cs.Guard();

            if (firstSampleTime < m_clockStartOffset) // do not process samples predating the clock start offset;
            {
                return;
            }

            if (m_startTimeOffset == PRESENTATION_CURRENT_POSITION)
            {
                m_startTimeOffset = firstSampleTime - m_clockStartOffset;
            }
        }

        STDMETHODIMP ShouldDropSample(_In_ IMFSample* pSample, _Outptr_ bool *pDrop);
        STDMETHODIMP NotifyStarted();
        STDMETHODIMP NotifyStopped();
        STDMETHODIMP NotifyMarker(const PROPVARIANT *pVarContextValue);
        STDMETHODIMP NotifyRequestSample();

    private:
        CriticalSection m_cs;
        CriticalSection m_eventCS;

        std::atomic<Capture::State> m_currentState;
        LONGLONG m_clockStartOffset;    // Presentation time when the clock started.
        LONGLONG m_startTimeOffset;     // amount to subtract from a timestamp
        
        uint8_t m_streamIndex;
        Windows::Media::MediaProperties::IMediaEncodingProperties m_encodingProperties;
        com_ptr<IMFMediaType> m_mediaType;
        GUID m_guidMajorType;
        GUID m_guidSubType;

        CameraCapture::Media::Capture::Sink m_parentSink;
        com_ptr<IMFMediaEventQueue> m_eventQueue;

        bool m_setDiscontinuity;
        bool m_enableSampleRequests;
        uint8_t m_sampleRequests;
        LONGLONG m_lastTimestamp;
        LONGLONG m_lastDecodeTime;

        static const uint8_t m_cMaxSampleRequests = MAX_SAMPLE_REQUESTS;
    };
}

namespace winrt::CameraCapture::Media::Capture::factory_implementation
{
    struct StreamSink : StreamSinkT<StreamSink, implementation::StreamSink>
    {
    };
}
