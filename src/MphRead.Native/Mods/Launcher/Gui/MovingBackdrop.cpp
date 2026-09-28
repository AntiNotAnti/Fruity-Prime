#include "MovingBackdrop.hpp"

#include <algorithm>
#include <cmath>
#include <memory>
#include <numbers>

namespace MphRead::Mods::Launcher::Gui
{
    using namespace ::MphRead::NativeRuntime::Avalonia;

    MovingBackdrop::MovingBackdrop()
        : _timer(Threading::TimeSpan(FrameSeconds), Threading::DispatcherPriority::Render,
              [this] { Tick(); })
    {
        IsHitTestVisible(false);
    }

    MovingBackdrop::~MovingBackdrop()
    {
        std::erase(_live, this);
    }

    void MovingBackdrop::Suspended(bool value)
    {
        if (_suspended == value)
        {
            return;
        }
        _suspended = value;
        Arbitrate();
    }

    void MovingBackdrop::OnAttachedToVisualTree()
    {
        Control::OnAttachedToVisualTree();
        _live.push_back(this);
        Threading::Dispatcher::UIThread().Post([this] { Tick(); }, Threading::DispatcherPriority::Loaded);
        Arbitrate();
    }

    void MovingBackdrop::OnDetachedFromVisualTree()
    {
        std::erase(_live, this);
        _timer.Stop();
        Arbitrate();
        Control::OnDetachedFromVisualTree();
    }

    void MovingBackdrop::Arbitrate()
    {
        for (std::size_t i = 0; i < _live.size(); i++)
        {
            MovingBackdrop* layer = _live[i];
            if (i == _live.size() - 1 && !Deck::Still() && !_suspended)
            {
                layer->_timer.Start();
            }
            else
            {
                layer->_timer.Stop();
            }
        }
    }

    void MovingBackdrop::Tick()
    {
        if (_suspended)
        {
            return;
        }
        if (!Deck::Still())
        {
            _phase = std::fmod(_phase + PhaseStep, 1.0);
        }
        InvalidateVisual();
    }

    void MovingBackdrop::Render(Media::DrawingContext& context)
    {
        const double w = Bounds().Width;
        const double h = Bounds().Height;
        if (w <= 0 || h <= 0)
        {
            return;
        }

        // The old implementation rebuilt an RGBA noise bitmap on the CPU at
        // 30 Hz. These animated washes are vector primitives, so a live
        // TopLevel keeps the entire effect on Ganesh/OpenGL.
        const double angle = _phase * 2.0 * std::numbers::pi;
        const double warmX = 0.34 + std::sin(angle) * 0.12;
        const double warmY = 0.42 + std::cos(angle * 0.83) * 0.10;
        const double coolX = 0.70 + std::cos(angle * 0.71) * 0.13;
        const double coolY = 0.62 + std::sin(angle * 0.91) * 0.11;

        auto warm = std::make_shared<Media::RadialGradientBrush>();
        warm->Center = RelativePoint(warmX, warmY, RelativeUnit::Relative);
        warm->GradientOrigin = warm->Center;
        warm->RadiusX = RelativeScalar(0.58, RelativeUnit::Relative);
        warm->RadiusY = RelativeScalar(0.68, RelativeUnit::Relative);
        warm->GradientStops = {
            Media::GradientStop(Media::Color::FromArgb(50, 0xc4, 0x60, 0x58), 0),
            Media::GradientStop(Media::Color::FromArgb(0, 0xc4, 0x60, 0x58), 1)
        };

        auto cool = std::make_shared<Media::RadialGradientBrush>();
        cool->Center = RelativePoint(coolX, coolY, RelativeUnit::Relative);
        cool->GradientOrigin = cool->Center;
        cool->RadiusX = RelativeScalar(0.62, RelativeUnit::Relative);
        cool->RadiusY = RelativeScalar(0.72, RelativeUnit::Relative);
        cool->GradientStops = {
            Media::GradientStop(Media::Color::FromArgb(58, 0x30, 0x70, 0xba), 0),
            Media::GradientStop(Media::Color::FromArgb(0, 0x30, 0x70, 0xba), 1)
        };

        const Rect box(0, 0, w, h);
        context.FillRectangle(warm, box);
        context.FillRectangle(cool, box);
    }
}
