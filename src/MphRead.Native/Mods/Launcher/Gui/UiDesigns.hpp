#pragma once

#include "UiCapture.hpp"
#include "../../../NativeRuntime/Avalonia/Avalonia.hpp"

#include <functional>
#include <optional>
#include <string>
#include <string_view>

namespace MphRead::Mods::Launcher::Gui
{
    namespace Av = ::MphRead::NativeRuntime::Avalonia;

    class UiDesignsAdapter
    {
    public:
        virtual ~UiDesignsAdapter() = default;

        // The C# calls GuiLauncher.EnsureSetup before touching the output path.
        [[nodiscard]] virtual bool EnsureSetup() = 0;
        virtual void CreateDirectory(std::string_view directory) = 0;
        virtual void InvokeUiThread(const std::function<void()>& action) = 0;
        [[nodiscard]] virtual bool Capture(const Av::Controls::ControlPtr& view,
            std::string_view path, UiCaptureSize size) = 0;
        virtual void ConsoleWriteLine(std::string_view text) = 0;
    };

    namespace Detail
    {
        // Avalonia, dispatcher and render-target boundary supplied by the GUI host.
        [[nodiscard]] UiDesignsAdapter& UiDesignsAdapterInstance();
    }

    // The six non-shipping information-architecture studies behind `-uidesign`.
    class UiDesigns final
    {
    public:
        UiDesigns() = delete;
        UiDesigns(const UiDesigns&) = delete;
        UiDesigns& operator=(const UiDesigns&) = delete;

        [[nodiscard]] static std::int32_t Run(
            UiDesignsAdapter& adapter, std::optional<std::string> directory);
        [[nodiscard]] static std::int32_t Run(std::optional<std::string> directory);
    };
}
