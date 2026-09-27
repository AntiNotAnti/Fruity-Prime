#include "DeckSide.hpp"

#include "../../../NativeRuntime/System/Managed.hpp"

#include <algorithm>
#include <cmath>
#include <memory>

namespace MphRead::Mods::Launcher::Gui
{
    using namespace ::MphRead::NativeRuntime::Avalonia;
    using ::MphRead::NativeRuntime::MathClamp;
    using ::MphRead::NativeRuntime::MathMin;
    using ::MphRead::NativeRuntime::RoundToEven;

    DeckSide::DeckSide()
    {
        static const bool registered = []
        {
            AffectsMeasure<DeckSide>(Deck::EmProperty);
            AffectsMeasure<DeckSide>(Deck::FrameWidthProperty);
            return true;
        }();
        (void)registered;
        HorizontalAlignment(Layout::HorizontalAlignment::Right);
        VerticalAlignment(Layout::VerticalAlignment::Stretch);
        IsVisible(false);
    }

    void DeckSide::Open(bool value)
    {
        if (_open == value)
        {
            return;
        }
        _open = value;
        if (value)
        {
            IsVisible(true);
        }
        if (Deck::Still())
        {
            _at = value ? 1 : 0;
            IsVisible(value);
            Place();
            return;
        }
        _clock.Restart();
        Ask();
    }

    Size DeckSide::MeasureOverride(Size availableSize)
    {
        const double em = Em();
        const Controls::ControlPtr child = Child();
        if (Upright())
        {
            // A bottom sheet is wide and short where the side panel is tall.
            HorizontalAlignment(Layout::HorizontalAlignment::Stretch);
            VerticalAlignment(Layout::VerticalAlignment::Bottom);
            const double cap = std::isinf(availableSize.Height) ? em * 31
                                                                : MathMin(em * 31, availableSize.Height * 0.68);
            if (child != nullptr)
            {
                child->Measure({availableSize.Width, cap});
            }
            return {availableSize.Width, cap};
        }
        HorizontalAlignment(Layout::HorizontalAlignment::Right);
        VerticalAlignment(Layout::VerticalAlignment::Stretch);
        const double width = RoundToEven(em * WidthEms);
        if (child != nullptr)
        {
            child->Measure({width, availableSize.Height});
        }
        return {width, std::isinf(availableSize.Height) ? (child != nullptr ? child->DesiredSize().Height : 0.0)
                                                        : availableSize.Height};
    }

    Size DeckSide::ArrangeOverride(Size finalSize)
    {
        if (const Controls::ControlPtr child = Child())
        {
            child->Arrange(Rect(finalSize));
        }
        Place();
        return finalSize;
    }

    void DeckSide::Ask()
    {
        if (_framePending || Deck::Still())
        {
            return;
        }
        _framePending = true;
        const std::shared_ptr<DeckSide> self = std::static_pointer_cast<DeckSide>(shared_from_this());
        Deck::NextFrame(*this, [self] { self->Tick(); });
    }

    void DeckSide::Tick()
    {
        _framePending = false;
        if (!_clock.IsRunning())
        {
            return;
        }
        const double t = MathClamp(_clock.Elapsed().TotalSeconds() / SlideSeconds, 0.0, 1.0);
        const double eased = Deck::Spring(t);
        _at = _open ? eased : 1 - eased;
        Place();
        if (t >= 1)
        {
            _clock.Reset();
            // A panel that has finished leaving takes itself out of the tree,
            // or it would swallow clicks meant for the cards under it.
            if (!_open)
            {
                IsVisible(false);
            }
            return;
        }
        Ask();
    }

    void DeckSide::Place()
    {
        const double em = Em();
        const double hidden = 1 - _at;
        if (Upright())
        {
            const double park = Bounds().Height + em * ParkEms;
            RenderTransform(std::make_shared<Media::TranslateTransform>(0, park * hidden));
        }
        else
        {
            const double park = Bounds().Width + em * ParkEms;
            RenderTransform(std::make_shared<Media::TranslateTransform>(park * hidden, 0));
        }
    }
}
