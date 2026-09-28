#pragma once

#if !defined(__ANDROID__)
#error "AndroidThumbnails is only valid for the Android native target."
#endif

#include <jni.h>

#include "../MphRead.Native/Mods/ThumbnailHost.hpp"

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace MphRead::Droid
{
    class AndroidThumbnailHost final : public MphRead::Mods::IThumbnailHost
    {
    public:
        AndroidThumbnailHost() = default;
        ~AndroidThumbnailHost() override = default;

        AndroidThumbnailHost(const AndroidThumbnailHost&) = delete;
        AndroidThumbnailHost& operator=(const AndroidThumbnailHost&) = delete;
        AndroidThumbnailHost(AndroidThumbnailHost&&) = delete;
        AndroidThumbnailHost& operator=(AndroidThumbnailHost&&) = delete;

        [[nodiscard]] std::shared_future<int> RenderAsync(
            std::vector<std::string> rooms,
            std::function<void(const std::string&)> report
        ) override;

    };

    class PreviewWorkers final
    {
    public:
        [[nodiscard]] static std::int32_t Count(
            JNIEnv* env,
            jobject context
        );

        [[nodiscard]] static std::int32_t Run(
            JNIEnv* env,
            jobject context,
            const std::vector<std::string>& rooms,
            std::int32_t width,
            std::int32_t height,
            const std::function<void(const std::string&)>& report
        );

    private:
        static void Watch(
            const std::vector<std::string>& rooms,
            const std::vector<std::string>& markers,
            const std::function<void(const std::string&)>& report
        );

        [[nodiscard]] static std::int32_t CountWritten(
            const std::vector<std::string>& rooms
        );

        static void TryDelete(const std::string& path) noexcept;

        PreviewWorkers() = delete;
        ~PreviewWorkers() = delete;
        PreviewWorkers(const PreviewWorkers&) = delete;
        PreviewWorkers& operator=(const PreviewWorkers&) = delete;
        PreviewWorkers(PreviewWorkers&&) = delete;
        PreviewWorkers& operator=(PreviewWorkers&&) = delete;
    };
}
