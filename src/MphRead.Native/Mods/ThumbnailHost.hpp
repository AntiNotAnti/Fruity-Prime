#pragma once

#include "../NativeRuntime/System/AtomicSharedPtr.hpp"

#include <functional>
#include <future>
#include <string>
#include <vector>

namespace MphRead::Mods
{
    // Platform renderer used when a process-per-room worker cannot run.
    class IThumbnailHost
    {
    public:
        virtual ~IThumbnailHost() = default;

        [[nodiscard]] virtual std::shared_future<int> RenderAsync(
            std::vector<std::string> rooms,
            std::function<void(const std::string&)> report) = 0;
    };

    class ThumbnailHost final
    {
    public:
        ThumbnailHost() = delete;

        [[nodiscard]] static std::shared_ptr<IThumbnailHost> Current() noexcept;
        static void Current(std::shared_ptr<IThumbnailHost> value) noexcept;

        [[nodiscard]] static bool CanRender();
        [[nodiscard]] static std::shared_future<int> RenderMissingAsync(
            const std::function<void(const std::string&)>& report);

    private:
        static ::MphRead::NativeRuntime::AtomicSharedPtr<IThumbnailHost> _current;
    };
}
