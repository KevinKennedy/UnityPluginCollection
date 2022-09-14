﻿#include "pch.h"
#include "Plugin.PlaybackManager.h"
#include "Plugin.PlaybackManager.g.cpp"

#include <winrt/Windows.Media.Core.h>
#include <winrt/Windows.Graphics.DirectX.Direct3D11.h>
#include <Windows.Graphics.DirectX.Direct3D11.h>

using namespace winrt;
using namespace VideoPlayer::Plugin::implementation;
using namespace winrt::Windows::Foundation;

using winrtPlaybackManager = VideoPlayer::Plugin::PlaybackManager;

VideoPlayer::Plugin::IModule PlaybackManager::Create(
    _In_ std::weak_ptr<IUnityDeviceResource> const& unityDevice,
    _In_ StateChangedCallback fnCallback,
    void* pCallbackObject)
{
    auto player = make<PlaybackManager>();

    if (SUCCEEDED(player.as<IModulePriv>()->Initialize(unityDevice, fnCallback, pCallbackObject)))
    {
        return player;
    }

    return nullptr;
}

PlaybackManager::PlaybackManager()
    : m_d3dDevice(nullptr)
    , m_mediaPlayer(nullptr)
    , m_mediaPlaybackSession(nullptr)
    , m_primaryBuffer(std::make_shared<SharedTextureBuffer>())
{
}

void PlaybackManager::Shutdown()
{
    m_primaryBuffer.reset();

    ReleaseMediaPlayer();

    ReleaseResources();

    Module::Shutdown();
}

event_token PlaybackManager::Closed(Windows::Foundation::EventHandler<winrtPlaybackManager> const& handler)
{
    return m_closedEvent.add(handler);
}

void PlaybackManager::Closed(event_token const& token)
{
    m_closedEvent.remove(token);
}

_Use_decl_annotations_
HRESULT PlaybackManager::CreatePlaybackTexture(
    UINT32 width,
    UINT32 height,
    void** ppvTexture)
{
    NULL_CHK_HR(ppvTexture, E_INVALIDARG);

    if (width < 1 || height < 1)
    {
        IFR(E_INVALIDARG);
    }

    *ppvTexture = nullptr;

    m_primaryBuffer->Reset();

    auto resources = m_d3d11DeviceResources.lock();
    NULL_CHK_HR(resources, E_POINTER);

    // make sure we have created our own d3d device
    IFR(CreateResources(resources->GetDevice()));

    IFR(SharedTextureBuffer::Create(resources->GetDevice().get(), m_dxgiDeviceManager.get(), width, height, m_primaryBuffer));

    com_ptr<ID3D11ShaderResourceView> spSRV = nullptr;
    m_primaryBuffer->frameTextureSRV.copy_to(spSRV.put());

    *ppvTexture = spSRV.detach();

    return S_OK;
}

_Use_decl_annotations_
HRESULT PlaybackManager::LoadContent(
    hstring const& contentLocation)
{
    if (contentLocation.empty())
    {
        IFR(E_INVALIDARG);
    }

    if (m_mediaPlayer == nullptr)
    {
        IFR(CreateMediaPlayer());
    }

    HRESULT hr = S_OK;

    try
    {
        auto uri = Windows::Foundation::Uri(contentLocation);

        auto mediaSource = Windows::Media::Core::MediaSource::CreateFromUri(uri);

        m_mediaPlayer.Source(mediaSource);
    }
    catch (hresult_error const & e)
    {
        hr = e.code();
    }

    return hr;
}

_Use_decl_annotations_
HRESULT PlaybackManager::Play()
{
    NULL_CHK_HR(m_mediaPlayer, MF_E_NOT_INITIALIZED);

    HRESULT hr = S_OK;

    try
    {
        m_mediaPlayer.Play();
    }
    catch (hresult_error const & e)
    {
        hr = e.code();
    }

    return hr;
}

_Use_decl_annotations_
HRESULT PlaybackManager::Pause()
{
    NULL_CHK_HR(m_mediaPlayer, MF_E_NOT_INITIALIZED);

    HRESULT hr = S_OK;

    try
    {
        m_mediaPlayer.Pause();
    }
    catch (hresult_error const & e)
    {
        hr = e.code();
    }

    return hr;
}

_Use_decl_annotations_
HRESULT PlaybackManager::Stop()
{
    NULL_CHK_HR(m_mediaPlayer, MF_E_NOT_INITIALIZED);

    HRESULT hr = S_OK;

    try
    {
        m_mediaPlayer.Source(nullptr);
    }
    catch (hresult_error const & e)
    {
        hr = e.code();
    }

    return hr;
}

_Use_decl_annotations_
HRESULT PlaybackManager::CreateMediaPlayer()
{
    if (m_mediaPlayer != nullptr)
    {
        ReleaseMediaPlayer();
    }

    m_mediaPlayer = Windows::Media::Playback::MediaPlayer();

    m_endedToken = m_mediaPlayer.MediaEnded([=](Windows::Media::Playback::MediaPlayer const& sender, Windows::Foundation::IInspectable const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        CALLBACK_STATE state{};
        ZeroMemory(&state, sizeof(CALLBACK_STATE));

        state.type = CallbackType::VideoPlayer;

        ZeroMemory(&state.value.playbackState, sizeof(PLAYBACK_STATE));
        state.value.playbackState.state = MediaPlayerState::Ended;

        Callback(state);
    });

    m_failedToken = m_mediaPlayer.MediaFailed([=](Windows::Media::Playback::MediaPlayer const& sender, Windows::Media::Playback::MediaPlayerFailedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        CALLBACK_STATE state{};
        ZeroMemory(&state, sizeof(CALLBACK_STATE));

        state.type = CallbackType::Failed;

        ZeroMemory(&state.value.failedState, sizeof(FAILED_STATE));

        state.value.failedState.hresult = args.ExtendedErrorCode();

        Callback(state);
    });

    m_openedToken = m_mediaPlayer.MediaOpened([=](Windows::Media::Playback::MediaPlayer const& sender, Windows::Foundation::IInspectable const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        if (nullptr == m_mediaPlaybackSession)
        {
            return;
        }

        CALLBACK_STATE state{};
        ZeroMemory(&state, sizeof(CALLBACK_STATE));

        state.type = CallbackType::VideoPlayer;

        FillPlaybackState(state.value.playbackState);

        Callback(state);
    });

    // set frameserver mode for video
    m_mediaPlayer.IsVideoFrameServerEnabled(true);
    m_videoFrameAvailableToken = m_mediaPlayer.VideoFrameAvailable([=](Windows::Media::Playback::MediaPlayer const& sender, Windows::Foundation::IInspectable const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        if (nullptr != m_primaryBuffer->mediaSurface)
        {
            m_mediaPlayer.CopyFrameToVideoSurface(m_primaryBuffer->mediaSurface);
        }
    });

    m_mediaPlaybackSession = m_mediaPlayer.PlaybackSession();
    m_stateChangedEventToken = m_mediaPlaybackSession.PlaybackStateChanged([=](Windows::Media::Playback::MediaPlaybackSession const& sender, Windows::Foundation::IInspectable const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        if (nullptr == m_mediaPlaybackSession)
        {
            return;
        }

        CALLBACK_STATE state{};
        ZeroMemory(&state, sizeof(CALLBACK_STATE));

        state.type = CallbackType::VideoPlayer;

        FillPlaybackState(state.value.playbackState);

        Callback(state);
    });

    return S_OK;
}

_Use_decl_annotations_
void PlaybackManager::FillPlaybackState(PLAYBACK_STATE& playbackState)
{
    ZeroMemory(&playbackState, sizeof(PLAYBACK_STATE));

    try
    {
        playbackState.state = static_cast<MediaPlayerState>(m_mediaPlaybackSession.PlaybackState());
        playbackState.width = static_cast<int32_t>(m_mediaPlaybackSession.NaturalVideoWidth());
        playbackState.height = static_cast<int32_t>(m_mediaPlaybackSession.NaturalVideoHeight());
        playbackState.canSeek = static_cast<boolean>(m_mediaPlaybackSession.CanSeek());
        playbackState.duration = m_mediaPlaybackSession.NaturalDuration().count();
    }
    catch (hresult_error const& e)
    {
        // In Miracast use, the accessors above will fail.  Ignore
        // the failures and set everytihng to zero.
        ZeroMemory(&playbackState, sizeof(PLAYBACK_STATE));
    }
}


_Use_decl_annotations_
void PlaybackManager::ReleaseMediaPlayer()
{
    if (m_mediaPlaybackSession != nullptr)
    {
        m_mediaPlaybackSession.PlaybackStateChanged(m_stateChangedEventToken);

        m_mediaPlaybackSession = nullptr;
    }

    if (m_mediaPlayer != nullptr)
    {
        m_mediaPlayer.MediaEnded(m_endedToken);
        m_mediaPlayer.MediaFailed(m_failedToken);
        m_mediaPlayer.MediaOpened(m_openedToken);
        m_mediaPlayer.VideoFrameAvailable(m_videoFrameAvailableToken);

        m_mediaPlayer = nullptr;
    }
}

_Use_decl_annotations_
HRESULT PlaybackManager::CreateResources(com_ptr<ID3D11Device> const& unityDevice)
{
    if (m_d3dDevice != nullptr)
    {
        return S_OK;
    }

    NULL_CHK_HR(unityDevice, E_INVALIDARG);

    auto sdxgiDevice = unityDevice.as<IDXGIDevice>();
    NULL_CHK_HR(sdxgiDevice, E_POINTER);

    com_ptr<IDXGIAdapter> dxgiAdapter = nullptr;
    IFR(sdxgiDevice->GetAdapter(dxgiAdapter.put()));

    // create dx device for media pipeline
    com_ptr<ID3D11Device> d3dDevice = nullptr;
    IFR(CreateMediaDevice(dxgiAdapter.get(), d3dDevice.put()));

    // lock the shared dxgi device manager
    // will keep lock open for the life of object
    //     call MFUnlockDXGIDeviceManager when unloading
    uint32_t resetToken;
    com_ptr<IMFDXGIDeviceManager> dxgiDeviceManager;
    IFR(MFLockDXGIDeviceManager(&resetToken, dxgiDeviceManager.put()));

    // associtate the device with the manager
    IFR(dxgiDeviceManager->ResetDevice(d3dDevice.get(), resetToken));
    
    m_d3dDevice.attach(d3dDevice.detach());
    m_resetToken = resetToken;
    m_dxgiDeviceManager = dxgiDeviceManager;

    return S_OK;
}

_Use_decl_annotations_
void PlaybackManager::ReleaseResources()
{
    if (m_dxgiDeviceManager != nullptr)
    {
        m_dxgiDeviceManager->ResetDevice(m_d3dDevice.get(), m_resetToken);

        // release the default media foundation dxgi manager
        MFUnlockDXGIDeviceManager();

        m_dxgiDeviceManager = nullptr;
    }

    if (m_d3dDevice != nullptr)
    {
        m_d3dDevice = nullptr;
    }

    if (m_closedEvent)
    {
        m_closedEvent(nullptr, *this);
    }
}
