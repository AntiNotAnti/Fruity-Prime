#include "Tap.hpp"

#include <cmath>

namespace MphRead::Mods::Launcher::Gui
{
    bool Tap::Drags(const Av::Input::PointerEventArgs& e)
    {
        return e.Pointer->Type == Av::Input::PointerType::Touch || e.Pointer->Type == Av::Input::PointerType::Pen;
    }

    void Tap::Press(const Av::Input::PointerPressedEventArgs& e, const Av::Visual& over)
    {
        Press(e.Pointer, e.GetPosition(&over), Drags(e));
    }

    bool Tap::Moved(const Av::Input::PointerEventArgs& e, const Av::Visual& over)
    {
        return Moved(e.Pointer, e.GetPosition(&over));
    }

    bool Tap::Release(const Av::Input::PointerReleasedEventArgs& e, const Av::Visual& over)
    {
        return Release(e.Pointer, e.GetPosition(&over), over.Bounds().GetSize());
    }

    void Tap::Press(const void* pointer, Av::Point origin, bool drags) noexcept
    {
        _pointer = pointer;
        _origin = origin;
        _drags = drags;
    }

    bool Tap::Moved(const void* pointer, Av::Point p) noexcept
    {
        if (_pointer == nullptr || pointer != _pointer || !_drags)
        {
            return false;
        }
        const Av::Vector travel = p - _origin;
        if (std::abs(travel.X) <= Slop && std::abs(travel.Y) <= Slop)
        {
            return false;
        }
        _pointer = nullptr;
        return true;
    }

    bool Tap::Release(const void* pointer, Av::Point p, Av::Size size) noexcept
    {
        if (_pointer == nullptr || pointer != _pointer)
        {
            // Somebody else's pointer, or one that was cancelled on the way.
            return false;
        }
        _pointer = nullptr;
        // Where the release landed, not IsPointerOver: a finger hovers nothing.
        return p.X >= 0 && p.Y >= 0 && p.X <= size.Width && p.Y <= size.Height;
    }

    bool Tap::Sideways(Av::Vector travel) noexcept
    {
        return std::abs(travel.X) > Slop && std::abs(travel.X) >= std::abs(travel.Y);
    }

    bool Tap::Cancel() noexcept
    {
        const bool was = _pointer != nullptr;
        _pointer = nullptr;
        return was;
    }
}
