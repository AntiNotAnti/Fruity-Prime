#include "Deck.hpp"

#include "../../DebugLog.hpp"
#include "../../../NativeRuntime/System/Globalization.hpp"
#include "../../../NativeRuntime/System/Managed.hpp"

#if !defined(__ANDROID__)
#include "UiSurface.hpp"
#endif

#include <algorithm>
#include <cmath>

namespace MphRead::Mods::Launcher::Gui
{
    using namespace ::MphRead::NativeRuntime::Avalonia;

    Av::AttachedProperty<double>& Deck::EmProperty = Av::RegisterAttached<Deck, double>("Em", 10.81, true);
    Av::AttachedProperty<double>& Deck::FrameWidthProperty = Av::RegisterAttached<Deck, double>("FrameWidth", 940.0, true);

    Media::Color Deck::Fade(std::int32_t hex, double alpha)
    {
        const double alphaByte = ::MphRead::NativeRuntime::MathClamp(
            ::MphRead::NativeRuntime::RoundToEven(alpha * 255), 0.0, 255.0);
        return Media::Color::FromArgb(static_cast<std::uint8_t>(alphaByte),
            static_cast<std::uint8_t>(hex >> 16), static_cast<std::uint8_t>((hex >> 8) & 0xff),
            static_cast<std::uint8_t>(hex & 0xff));
    }

    Media::BoxShadow Deck::Shadow(double x, double y, double blur, double spread, Media::Color colour, bool inset)
    {
        Media::BoxShadow shadow;
        shadow.OffsetX = x;
        shadow.OffsetY = y;
        shadow.Blur = blur;
        shadow.Spread = spread;
        shadow.Color = colour;
        shadow.IsInset = inset;
        return shadow;
    }

    Media::Typeface Deck::Body(bool bold)
    {
        return Media::Typeface(bold ? MonoBold : Mono, Media::FontStyle::Normal, Media::FontWeight::Normal);
    }

    Media::Typeface Deck::Label(bool strong)
    {
        return Media::Typeface(strong ? GuiTheme::PixelSemi : GuiTheme::Pixel, Media::FontStyle::Normal,
            Media::FontWeight::Normal);
    }

    void Deck::NextFrame(Visual& asker, std::function<void()> step, bool idling)
    {
#if !defined(__ANDROID__)
        (void)asker;
        UiSurface::RequestFrame(std::move(step), idling);
#else
        (void)asker;
        (void)idling;
        // Not in a tree yet: no clock to ask, and the step still has to land
        // between passes rather than inside one.
        Threading::Dispatcher::UIThread().Post(std::move(step), Threading::DispatcherPriority::Render);
#endif
    }

    double Deck::Spring(double progress)
    {
        return Bezier(progress, 0.18, 1.55, 0.35, 1);
    }

    double Deck::Bezier(double progress, double x1, double y1, double x2, double y2)
    {
        if (progress <= 0)
        {
            return 0;
        }
        if (progress >= 1)
        {
            return 1;
        }
        double t = progress;
        for (int i = 0; i < 4; i++)
        {
            const double slope = Slope(t, x1, x2);
            if (std::abs(slope) < 1e-6)
            {
                break;
            }
            t -= (Curve(t, x1, x2) - progress) / slope;
        t = ::MphRead::NativeRuntime::MathClamp(t, 0.0, 1.0);
        }
        return Curve(t, y1, y2);
    }

    double Deck::Curve(double t, double a, double b)
    {
        const double u = 1 - t;
        return 3 * u * u * t * a + 3 * u * t * t * b + t * t * t;
    }

    double Deck::Slope(double t, double a, double b)
    {
        const double u = 1 - t;
        return 3 * u * u * a + 6 * u * t * (b - a) + 3 * t * t * (1 - b);
    }

    double Deck::EmFor(double width, double height)
    {
        if (width <= 0)
        {
            return 9;
        }
        if (_phone)
        {
            const bool landscape = width > height;
            return ::MphRead::NativeRuntime::MathClamp(width * (landscape ? 0.0155 : 0.0315), 9.0, 13.0);
        }
        return ::MphRead::NativeRuntime::MathClamp(width * 0.0115, 9.0, 15.0);
    }

    Size DeckStage::MeasureOverride(Size availableSize)
    {
        const double w = availableSize.Width;
        const double h = availableSize.Height;
        if (!std::isinf(w) && !std::isinf(h) && w > 0 && h > 0)
        {
            const double em = Deck::EmFor(w, h);
            if (std::abs(em - _em) > 0.001)
            {
                _em = em;
                Deck::SetEm(*this, em);
                // One line when it moves: the number every "the phone lays it
                // out as if it were a monitor" report is really about.
                DebugLog::Line("deck", "stage em " + ::MphRead::NativeRuntime::ToString(em, "0.###") + " at "
                    + ::MphRead::NativeRuntime::ToString(w, "0") + "x" + ::MphRead::NativeRuntime::ToString(h, "0"));
            }
            if (std::abs(w - _width) > 0.001)
            {
                _width = w;
                Deck::SetFrameWidth(*this, w);
            }
        }
        return Panel::MeasureOverride(availableSize);
    }
}
