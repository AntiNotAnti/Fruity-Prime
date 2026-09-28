#pragma once

#include "../../../NativeRuntime/Avalonia/Avalonia.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace MphRead::Mods::Launcher::Gui
{
    namespace Av = ::MphRead::NativeRuntime::Avalonia;

    struct UiCaptureSize final
    {
        double Width;
        double Height;

        friend constexpr bool operator==(
            const UiCaptureSize&, const UiCaptureSize&) noexcept = default;
    };

    // Headless captures of the launcher's actual controls. The PNG and bounds
    // JSON use the same in-tree Avalonia implementation as the live surface.
    class UiCapture final
    {
    public:
        UiCapture() = delete;
        UiCapture(const UiCapture&) = delete;
        UiCapture& operator=(const UiCapture&) = delete;

        [[nodiscard]] static std::int32_t Run(std::optional<std::string> directory);
        [[nodiscard]] static bool Capture(const Av::Controls::ControlPtr& view,
            std::string_view path, UiCaptureSize size);
    };
}
