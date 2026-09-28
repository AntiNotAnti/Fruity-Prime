#pragma once

#include "../../../NativeRuntime/Avalonia/Avalonia.hpp"

namespace MphRead::Mods::Launcher::Gui
{
    namespace Av = ::MphRead::NativeRuntime::Avalonia;

    // Whether a press and the release after it were a tap, or the beginning
    // of a scroll that happened to start on this row. The press decides
    // nothing; a finger that travels more than Slop has stopped meaning the
    // row; the release acts, and only inside the control. The distance test
    // is for a finger or a stylus only -- a mouse keeps what it had.
    //
    // The methods come in pairs: one taking Avalonia's event, one taking
    // plain values. The plain ones are the rule itself, and are what
    // -tapcheck drives without a display.
    class Tap final
    {
    public:
        // How far a finger may wander and still mean the row it started on.
        static constexpr double Slop = 8;

        // A press is still live: it has not been cancelled or let go.
        [[nodiscard]] bool Down() const noexcept { return _pointer != nullptr; }

        // True for the pointers that scroll by dragging.
        [[nodiscard]] static bool Drags(const Av::Input::PointerEventArgs& e);

        void Press(const Av::Input::PointerPressedEventArgs& e, const Av::Visual& over);
        // True when this move is what cancelled the tap.
        bool Moved(const Av::Input::PointerEventArgs& e, const Av::Visual& over);
        // True when the press and this release were a tap.
        bool Release(const Av::Input::PointerReleasedEventArgs& e, const Av::Visual& over);

        // Where the pointer has got to since the press, or nothing if there
        // is no press.
        [[nodiscard]] Av::Vector Travel(Av::Point p) const noexcept
        {
            return _pointer == nullptr ? Av::Vector{} : p - _origin;
        }

        // ------------------------------------------------------- the rule

        void Press(const void* pointer, Av::Point origin, bool drags) noexcept;
        bool Moved(const void* pointer, Av::Point p) noexcept;
        bool Release(const void* pointer, Av::Point p, Av::Size size) noexcept;

        // Whether a travel is along a row rather than down the page. Ties go
        // to the row.
        [[nodiscard]] static bool Sideways(Av::Vector travel) noexcept;

        // Give the gesture up. True if there was one.
        bool Cancel() noexcept;

    private:
        const void* _pointer = nullptr;
        Av::Point _origin{};
        bool _drags = false;
    };
}
