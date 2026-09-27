#include "AL.hpp"

#if !defined(__ANDROID__)

#include <vector>

#if defined(__APPLE__)
#include <OpenAL/al.h>
#include <OpenAL/alc.h>
#else
#include <AL/al.h>
#include <AL/alc.h>
#endif

#if defined(__APPLE__)
#include "../../Mods/Platform/AppPaths.hpp"
#include "../System/NativeLibrary.hpp"
#include "../System/Runtime.hpp"

namespace
{
    void* BundledOpenAL()
    {
        static void* const library = MphRead::NativeRuntime::NativeLibraryLoad(
            MphRead::NativeRuntime::PathCombine(
                MphRead::Mods::Platform::AppPaths::ExecutableDirectory(),
                "libopenal.1.dylib"
            )
        );
        return library;
    }

    template <typename Function>
    Function OpenALFunction(const char* name)
    {
        return reinterpret_cast<Function>(
            MphRead::NativeRuntime::NativeLibraryGetExport(BundledOpenAL(), name)
        );
    }
}

#define OPENAL_FUNCTION(name) \
    ([]() \
    { \
        static const auto function = OpenALFunction<decltype(&::name)>(#name); \
        return function; \
    }())
#else
#define OPENAL_FUNCTION(name) ::name
#endif

namespace OpenTK::Audio::OpenAL
{
    const ALDevice ALDevice::Null{};
    const ALContext ALContext::Null{};

    ALContext ALC::GetCurrentContext()
    {
        return ALContext(reinterpret_cast<std::intptr_t>(
            OPENAL_FUNCTION(alcGetCurrentContext)()));
    }

    void* ALC::GetProcAddress(ALDevice device, const std::string& name)
    {
        return reinterpret_cast<void*>(OPENAL_FUNCTION(alcGetProcAddress)(
            reinterpret_cast<ALCdevice*>(device.Handle), name.c_str()));
    }
}

namespace OpenTK::Audio::OpenAL::AL
{
    std::int32_t GenSource()
    {
        ALuint name = 0;
        OPENAL_FUNCTION(alGenSources)(1, &name);
        return static_cast<std::int32_t>(name);
    }

    void GenBuffers(std::span<std::int32_t> buffers)
    {
        if (buffers.empty())
        {
            return;
        }
        std::vector<ALuint> names(buffers.size());
        OPENAL_FUNCTION(alGenBuffers)(static_cast<ALsizei>(names.size()), names.data());
        for (std::size_t i = 0; i < buffers.size(); ++i)
        {
            buffers[i] = static_cast<std::int32_t>(names[i]);
        }
    }

    void DeleteSource(std::int32_t source)
    {
        const ALuint name = static_cast<ALuint>(source);
        OPENAL_FUNCTION(alDeleteSources)(1, &name);
    }

    void DeleteBuffers(std::span<const std::int32_t> buffers)
    {
        if (buffers.empty())
        {
            return;
        }
        std::vector<ALuint> names(buffers.begin(), buffers.end());
        OPENAL_FUNCTION(alDeleteBuffers)(static_cast<ALsizei>(names.size()), names.data());
    }

    void Source(std::int32_t source, ALSourcef param, float value)
    {
        OPENAL_FUNCTION(alSourcef)(static_cast<ALuint>(source), static_cast<ALenum>(param), value);
    }

    std::int32_t GetSource(std::int32_t source, ALGetSourcei param)
    {
        ALint value = 0;
        OPENAL_FUNCTION(alGetSourcei)(static_cast<ALuint>(source), static_cast<ALenum>(param), &value);
        return static_cast<std::int32_t>(value);
    }

    void SourcePlay(std::int32_t source)
    {
        OPENAL_FUNCTION(alSourcePlay)(static_cast<ALuint>(source));
    }

    void SourceStop(std::int32_t source)
    {
        OPENAL_FUNCTION(alSourceStop)(static_cast<ALuint>(source));
    }

    void SourceQueueBuffers(std::int32_t source, std::span<const std::int32_t> buffers)
    {
        if (buffers.empty())
        {
            return;
        }
        std::vector<ALuint> names(buffers.begin(), buffers.end());
        OPENAL_FUNCTION(alSourceQueueBuffers)(static_cast<ALuint>(source),
            static_cast<ALsizei>(names.size()), names.data());
    }

    void SourceUnqueueBuffers(std::int32_t source, std::span<std::int32_t> buffers)
    {
        if (buffers.empty())
        {
            return;
        }
        std::vector<ALuint> names(buffers.size());
        OPENAL_FUNCTION(alSourceUnqueueBuffers)(static_cast<ALuint>(source),
            static_cast<ALsizei>(names.size()), names.data());
        for (std::size_t i = 0; i < buffers.size(); ++i)
        {
            buffers[i] = static_cast<std::int32_t>(names[i]);
        }
    }

    void BufferData(std::int32_t buffer, ALFormat format,
        std::span<const std::int16_t> data, std::int32_t frequency)
    {
        OPENAL_FUNCTION(alBufferData)(static_cast<ALuint>(buffer), static_cast<ALenum>(format),
            data.data(), static_cast<ALsizei>(data.size() * sizeof(std::int16_t)),
            frequency);
    }
}

#undef OPENAL_FUNCTION

#endif
