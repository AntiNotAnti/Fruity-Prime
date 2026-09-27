#pragma once

#include "Deck.hpp"
#include "../../../NativeRuntime/System/Stopwatch.hpp"

namespace MphRead::Mods::Launcher::Gui
{
    // The room you picked, and what you are taking into it: the reference's
    // .side, sliding in from off the edge on the spring (a bottom sheet on a
    // phone held upright).
    class DeckSide final : public Av::Controls::Decorator
    {
    public:
        DeckSide();

        // Is the panel showing? Setting it starts the slide either way.
        [[nodiscard]] bool Open() const noexcept { return _open; }
        void Open(bool value);

    protected:
        Av::Size MeasureOverride(Av::Size availableSize) override;
        Av::Size ArrangeOverride(Av::Size finalSize) override;

    private:
        [[nodiscard]] double Em() const { return Deck::GetEm(*this); }
        [[nodiscard]] bool Upright() const { return Deck::Phone() && Deck::GetFrameWidth(*this) <= Deck::NarrowFrame; }
        void Ask();
        void Tick();
        void Place();

        static constexpr double WidthEms = 17.5;
        static constexpr double ParkEms = 1.4;
        static constexpr double SlideSeconds = 0.34;

        ::MphRead::NativeRuntime::Stopwatch _clock;
        bool _open = false;
        bool _framePending = false;
        double _at = 0;
    };
}
