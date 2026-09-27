#include "DeckSheet.hpp"

#include "../../../NativeRuntime/System/Managed.hpp"

#include <algorithm>
#include <memory>

namespace MphRead::Mods::Launcher::Gui
{
    using namespace ::MphRead::NativeRuntime::Avalonia;
    using ::MphRead::NativeRuntime::MathClamp;
    using ::MphRead::NativeRuntime::MathMax;

    DeckSheet::DeckSheet()
    {
        // The panel is the one that scales, not the scrim.
        ClipToBounds(false);
    }

    void DeckSheet::OnAttachedToVisualTree()
    {
        Panel::OnAttachedToVisualTree();
        if (Deck::Still())
        {
            Apply(1);
            return;
        }
        Apply(0);
        _clock.Restart();
        Ask();
    }

    void DeckSheet::OnDetachedFromVisualTree()
    {
        _clock.Reset();
        Panel::OnDetachedFromVisualTree();
    }

    void DeckSheet::Tick()
    {
        _framePending = false;
        if (!_clock.IsRunning())
        {
            return;
        }
        const double t = _clock.Elapsed().TotalSeconds();
        Apply(t);
        if (t >= MathMax(FadeSeconds, RiseSeconds))
        {
            _clock.Reset();
            return;
        }
        Ask();
    }

    void DeckSheet::Ask()
    {
        if (_framePending)
        {
            return;
        }
        _framePending = true;
        const std::shared_ptr<DeckSheet> self = std::static_pointer_cast<DeckSheet>(shared_from_this());
        Deck::NextFrame(*this, [self] { self->Tick(); });
    }

    void DeckSheet::Apply(double seconds)
    {
        const double fade = FadeSeconds <= 0 ? 1 : MathClamp(seconds / FadeSeconds, 0.0, 1.0);
        // --settle: no overshoot, which is right for an opacity.
        Opacity(Deck::Bezier(fade, 0.3, 0.8, 0.4, 1));

        const double rise = RiseSeconds <= 0 ? 1 : MathClamp(seconds / RiseSeconds, 0.0, 1.0);
        const double amount = Deck::Spring(rise);
        const double scale = FromScale + (1 - FromScale) * amount;
        const double lift = FromLift * (1 - amount);
        for (std::size_t i = 1; i < Children.Count(); i++)
        {
            const Controls::ControlPtr& child = Children[i];
            child->RenderTransformOrigin(RelativePoint(0.5, 0.5, RelativeUnit::Relative));
            auto group = std::make_shared<Media::TransformGroup>();
            group->Children.push_back(std::make_shared<Media::ScaleTransform>(scale, scale));
            group->Children.push_back(std::make_shared<Media::TranslateTransform>(0, lift));
            child->RenderTransform(group);
        }
    }
}
