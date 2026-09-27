#include "UiScaleHost.hpp"

#include "UiLayout.hpp"
#include "../../DebugLog.hpp"
#include "../../../NativeRuntime/System/Number.hpp"

#if defined(__ANDROID__)
#include "../../../../MphRead.Native.Android/MainActivity.hpp"
#endif

#include <cmath>
#include <limits>
#include <memory>
#include <string>

namespace MphRead::Mods::Launcher::Gui
{
    UiScaleHost::UiScaleHost(const Av::Controls::ControlPtr& screen)
        : _host(std::make_shared<Av::Controls::LayoutTransformControl>())
    {
        _host->LayoutTransform(std::make_shared<Av::Media::ScaleTransform>(1.0, 1.0));
        _host->HorizontalAlignment(Av::Layout::HorizontalAlignment::Stretch);
        _host->VerticalAlignment(Av::Layout::VerticalAlignment::Stretch);
        _host->Child(screen);
        Child(_host);
    }

    double UiScaleHost::FactorFor(double widthDips, double heightDips)
    {
        if (std::isinf(widthDips) || std::isinf(heightDips)
            || widthDips <= 0.0 || heightDips <= 0.0)
        {
            return 1.0;
        }
        if (std::isnan(widthDips) || std::isnan(heightDips))
        {
            // .NET Math.Min/Max and Math.Clamp propagate NaN; the C++ standard
            // min/max do not agree when only their second argument is NaN.
            return std::numeric_limits<double>::quiet_NaN();
        }
        return UiLayout::Factor(widthDips / DipsPerPoint, heightDips / DipsPerPoint)
            * DipsPerPoint;
    }

    Av::Controls::ControlPtr UiScaleHost::Screen() const
    {
        return _host->Child();
    }

    void UiScaleHost::Screen(Av::Controls::ControlPtr value)
    {
        _host->Child(std::move(value));
    }

    Av::Size UiScaleHost::MeasureOverride(Av::Size availableSize)
    {
        Apply(availableSize);
        return Decorator::MeasureOverride(availableSize);
    }

    void UiScaleHost::Apply(Av::Size size)
    {
        if (std::isinf(size.Width) || std::isinf(size.Height)
            || size.Width <= 0.0 || size.Height <= 0.0)
        {
            return;
        }
        const double factor = FactorFor(size.Width, size.Height);
        const Av::TopLevel* topLevel = Av::TopLevel::GetTopLevel(this);
        double density = topLevel != nullptr ? topLevel->RenderScaling() : 1.0;
#if defined(__ANDROID__)
        // The native toolkit TopLevel is a pixel-buffer host and defaults to
        // 1.0; the Android view's render scaling is its display density.
        if (topLevel != nullptr)
        {
            if (const ::MphRead::Droid::MainActivity* activity =
                ::MphRead::Droid::MainActivity::Instance(); activity != nullptr)
            {
                density = activity->DisplayDensity();
                if (!std::isfinite(density) || density <= 0.0)
                {
                    density = 1.0;
                }
            }
        }
#endif
        UiLayout::BakeScale = factor * density;
        if (std::abs(factor - _factor) < 0.0001)
        {
            return;
        }
        _factor = factor;
        _host->LayoutTransform(std::make_shared<Av::Media::ScaleTransform>(factor, factor));
        const std::string message = "screens at "
            + ::MphRead::NativeRuntime::ToString(factor, "0.###") + "x ("
            + ::MphRead::NativeRuntime::ToString(size.Width, "0") + "x"
            + ::MphRead::NativeRuntime::ToString(size.Height, "0") + " points)";
        ::MphRead::Mods::DebugLog::Line("ui", message);
    }
}
