#pragma once

#include "Deck.hpp"
#include "../../../NativeRuntime/System/Stopwatch.hpp"

namespace MphRead::Mods::Launcher::Gui
{
    // The reference's .sheet arriving: the scrim and the panel on it fading
    // up together, with the panel springing out of scale(.9) translateY(14px).
    class DeckSheet final : public Av::Controls::Panel
    {
    public:
        DeckSheet();

    protected:
        void OnAttachedToVisualTree() override;
        void OnDetachedFromVisualTree() override;

    private:
        void Tick();
        void Ask();
        void Apply(double seconds);

        static constexpr double FadeSeconds = 0.22;
        static constexpr double RiseSeconds = 0.4;
        static constexpr double FromScale = 0.9;
        static constexpr double FromLift = 14;

        ::MphRead::NativeRuntime::Stopwatch _clock;
        bool _framePending = false;
    };
}
