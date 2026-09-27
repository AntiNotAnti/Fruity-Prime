#pragma once

#if !defined(__ANDROID__)
#error "AndroidUiSurface is only valid for the Android native target."
#endif

#include "../MphRead.Native/Mods/Launcher/Gui/UiTopLevel.hpp"
#include "../MphRead.Native/NativeRuntime/Avalonia/Avalonia.hpp"

#include <cstdint>
#include <memory>
#include <mutex>
#include <vector>

namespace MphRead::Droid
{
    namespace Av = ::MphRead::NativeRuntime::Avalonia;

    class AndroidUiSurface final
    {
    public:
        [[nodiscard]] static std::shared_ptr<AndroidUiSurface> Current() noexcept;
        [[nodiscard]] static std::shared_ptr<AndroidUiSurface> Ensure();

        [[nodiscard]] bool Visible() const noexcept;
        void Resize(std::int32_t width, std::int32_t height);
        void Show(const Av::Controls::ControlPtr& view);
        void Hide();
        void Tick();

        [[nodiscard]] bool TakeFrame(
            std::vector<std::uint8_t>& into,
            std::int32_t& version,
            std::int32_t& width,
            std::int32_t& height
        );

        void TouchDown(double x, double y);
        void TouchMove(double x, double y);
        void TouchUp(double x, double y);

    private:
        AndroidUiSurface();

        void ApplyScale();
        void Painted();

        std::unique_ptr<Mods::Launcher::Gui::UiTopLevelImpl> _impl;
        std::shared_ptr<Av::Controls::LayoutTransformControl> _host;

        mutable std::mutex _gate;
        std::vector<std::uint8_t> _frame;
        std::int32_t _frameWidth = 0;
        std::int32_t _frameHeight = 0;
        std::int32_t _version = 0;

        std::int32_t _width = 1280;
        std::int32_t _height = 720;
        Av::Controls::ControlPtr _view;
        std::int64_t _touchId = 0;
    };
}
