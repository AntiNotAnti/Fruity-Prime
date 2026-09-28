#pragma once

#include "../../../NativeRuntime/Avalonia/Avalonia.hpp"

namespace MphRead::Mods::Launcher::Gui
{
    namespace Av = ::MphRead::NativeRuntime::Avalonia;

    class UiScaleHost final : public Av::Controls::Decorator
    {
    public:
        explicit UiScaleHost(const Av::Controls::ControlPtr& screen);

        [[nodiscard]] static double FactorFor(double widthDips, double heightDips);
        [[nodiscard]] Av::Controls::ControlPtr Screen() const;
        void Screen(Av::Controls::ControlPtr value);

    protected:
        Av::Size MeasureOverride(Av::Size availableSize) override;

    private:
        void Apply(Av::Size size);

        static constexpr double DipsPerPoint = 160.0 / 96.0;
        std::shared_ptr<Av::Controls::LayoutTransformControl> _host;
        double _factor = -1.0;
    };
}
