// SoundFlow's MiniAudio backend, as the managed build uses it: an engine, one
// playback device with a master mixer, and a player that pulls float samples
// from a RawDataProvider.
//
// The device is miniaudio's, as it is in the managed build (SoundFlow ships
// miniaudio and plays through WASAPI on Windows). This used to be OpenAL,
// because the program already links it for its sound effects -- which put the
// music on a second OpenAL device of its own, the one audio path that differed
// from the managed build. What SoundFlow is asked for in this program is
// exactly one thing -- keep pulling from the provider and play what comes
// back -- and that is what this does; nothing of SoundFlow's wider API is
// reproduced.

#include "../../Sound/Music.hpp"

#define MA_NO_DECODING
#define MA_NO_ENCODING
#define MA_NO_GENERATION
#define MA_NO_RESOURCE_MANAGER
#define MA_NO_NODE_GRAPH
#define MA_NO_ENGINE
#define MINIAUDIO_IMPLEMENTATION
#include "../../ThirdParty/miniaudio/miniaudio.h"

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstring>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <vector>

namespace SoundFlow::Components
{
    struct SoundPlayer::Impl final
    {
        Structs::AudioFormat Format{};
        std::shared_ptr<Providers::RawDataProvider> Provider;
        std::atomic<Enums::PlaybackState> State{Enums::PlaybackState::Stopped};
        std::atomic<bool> Disposed{false};
        // Only touched from the device's callback, which is one thread.
        std::vector<std::uint8_t> Scratch;
    };

    SoundPlayer::SoundPlayer(
        const std::shared_ptr<Backends::MiniAudio::MiniAudioEngine>& engine,
        const Structs::AudioFormat& format,
        const std::shared_ptr<Providers::RawDataProvider>& provider)
        : _impl(std::make_shared<Impl>())
    {
        (void)engine;
        _impl->Format = format;
        _impl->Provider = provider;
    }

    SoundPlayer::~SoundPlayer()
    {
        Dispose();
    }

    void SoundPlayer::Play()
    {
        _impl->State.store(Enums::PlaybackState::Playing, std::memory_order_release);
    }

    void SoundPlayer::Pause()
    {
        _impl->State.store(Enums::PlaybackState::Paused, std::memory_order_release);
    }

    void SoundPlayer::Stop()
    {
        _impl->State.store(Enums::PlaybackState::Stopped, std::memory_order_release);
    }

    void SoundPlayer::Dispose() noexcept
    {
        if (_impl != nullptr)
        {
            _impl->State.store(Enums::PlaybackState::Stopped, std::memory_order_release);
            _impl->Disposed.store(true, std::memory_order_release);
        }
    }

    Enums::PlaybackState SoundPlayer::State() const noexcept
    {
        return _impl->State.load(std::memory_order_acquire);
    }

    void SoundPlayer::MixInto(float* output, std::uint32_t frames, std::int32_t channels)
    {
        Impl& impl = *_impl;
        if (impl.Disposed.load(std::memory_order_acquire)
            || impl.State.load(std::memory_order_acquire) != Enums::PlaybackState::Playing
            || impl.Provider == nullptr)
        {
            return;
        }
        const std::size_t samples = static_cast<std::size_t>(frames) * static_cast<std::size_t>(channels);
        const std::size_t bytes = samples * sizeof(float);
        if (impl.Scratch.size() < bytes)
        {
            impl.Scratch.resize(bytes);
        }
        const std::int32_t read = impl.Provider->Read(
            std::span<std::uint8_t>(impl.Scratch.data(), bytes), 0, static_cast<std::int32_t>(bytes));
        if (read <= 0)
        {
            return;
        }
        const std::size_t got = std::min(samples, static_cast<std::size_t>(read) / sizeof(float));
        float value = 0;
        for (std::size_t i = 0; i < got; ++i)
        {
            std::memcpy(&value, impl.Scratch.data() + i * sizeof(float), sizeof(float));
            output[i] += value;
        }
    }

    void Mixer::Mix(float* output, std::uint32_t frames, std::int32_t channels)
    {
        std::lock_guard<std::mutex> guard(_mutex);
        for (const std::shared_ptr<SoundPlayer>& player : _components)
        {
            if (player != nullptr)
            {
                player->MixInto(output, frames, channels);
            }
        }
    }
}

namespace SoundFlow::Abstracts::Devices
{
    struct AudioPlaybackDevice::Impl final
    {
        ma_device Device{};
        bool Initialized = false;
        std::atomic<bool> Started{false};
        std::mutex StartLock;
    };

    namespace
    {
        void DataCallback(ma_device* device, void* output, const void* input, ma_uint32 frames)
        {
            (void)input;
            auto* const self = static_cast<AudioPlaybackDevice*>(device->pUserData);
            float* const samples = static_cast<float*>(output);
            const std::int32_t channels = static_cast<std::int32_t>(device->playback.channels);
            const std::size_t count = static_cast<std::size_t>(frames) * static_cast<std::size_t>(channels);
            std::memset(samples, 0, count * sizeof(float));
            try
            {
                self->MasterMixer.Mix(samples, frames, channels);
            }
            catch (...)
            {
                // A provider that throws is silence for this period, not a
                // dead audio thread.
            }
            for (std::size_t i = 0; i < count; ++i)
            {
                samples[i] = std::clamp(samples[i], -1.0F, 1.0F);
            }
        }
    }

    AudioPlaybackDevice::AudioPlaybackDevice()
        : _impl(std::make_shared<Impl>())
    {
    }

    AudioPlaybackDevice::~AudioPlaybackDevice()
    {
        if (_impl != nullptr && _impl->Initialized)
        {
            ma_device_uninit(&_impl->Device);
            _impl->Initialized = false;
        }
    }

    void AudioPlaybackDevice::Open(const Structs::AudioFormat& format)
    {
        ma_device_config config = ma_device_config_init(ma_device_type_playback);
        config.playback.format = ma_format_f32;
        config.playback.channels = static_cast<ma_uint32>(format.Channels > 0 ? format.Channels : 2);
        config.sampleRate = static_cast<ma_uint32>(format.SampleRate > 0 ? format.SampleRate : 48000);
        config.dataCallback = &DataCallback;
        config.pUserData = this;
        const ma_result result = ma_device_init(nullptr, &config, &_impl->Device);
        if (result != MA_SUCCESS)
        {
            throw std::runtime_error(std::string("miniaudio could not open a playback device: ")
                + ma_result_description(result));
        }
        _impl->Initialized = true;
    }

    void AudioPlaybackDevice::Start()
    {
        std::lock_guard<std::mutex> guard(_impl->StartLock);
        if (_impl->Initialized && !_impl->Started.load(std::memory_order_acquire)
            && ma_device_start(&_impl->Device) == MA_SUCCESS)
        {
            _impl->Started.store(true, std::memory_order_release);
        }
    }

    void AudioPlaybackDevice::Stop()
    {
        std::lock_guard<std::mutex> guard(_impl->StartLock);
        if (_impl->Initialized && _impl->Started.load(std::memory_order_acquire))
        {
            ma_device_stop(&_impl->Device);
            _impl->Started.store(false, std::memory_order_release);
        }
    }
}

namespace SoundFlow::Backends::MiniAudio
{
    struct MiniAudioEngine::Impl final
    {
        std::shared_ptr<Abstracts::Devices::AudioPlaybackDevice> Device;
    };

    MiniAudioEngine::MiniAudioEngine()
        : _impl(std::make_shared<Impl>())
    {
    }

    MiniAudioEngine::~MiniAudioEngine() = default;

    std::shared_ptr<Abstracts::Devices::AudioPlaybackDevice>
        MiniAudioEngine::InitializePlaybackDevice(
            const void* deviceInfo, const Structs::AudioFormat& format)
    {
        (void)deviceInfo;
        if (_impl->Device == nullptr)
        {
            auto device = std::make_shared<Abstracts::Devices::AudioPlaybackDevice>();
            device->Open(format);
            _impl->Device = std::move(device);
        }
        return _impl->Device;
    }
}
