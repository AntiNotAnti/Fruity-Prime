#include "HapticScheduler.hpp"

#include "GamepadHaptics.hpp"
#include "../../NativeRuntime/System/Exceptions.hpp"

#include <limits>

namespace MphRead::Mods::Input
{
    HapticScheduler::HapticScheduler()
    {
        Reset();
    }

    void HapticScheduler::Reset() noexcept
    {
        _last.fill(std::numeric_limits<std::int64_t>::min() / 2);
        _until = 0;
        _priority = 0;
    }

    bool HapticScheduler::Accept(GamepadFeedback feedback, std::int64_t now, std::int32_t duration)
    {
        const std::int32_t priority = feedback == GamepadFeedback::Death || feedback == GamepadFeedback::Explosion
            || feedback == GamepadFeedback::Damage ? 3
            : feedback == GamepadFeedback::Fire ? 1 : 2;
        const std::int32_t cooldown = feedback == GamepadFeedback::Fire ? 65 : 40;
        const std::int32_t index = static_cast<std::int32_t>(feedback);
        if (index < 0 || index >= static_cast<std::int32_t>(_last.size()))
        {
            throw ::System::IndexOutOfRangeException();
        }
        std::int64_t& last = _last[static_cast<std::size_t>(index)];
        if (now - last < cooldown || (now < _until && priority < _priority))
        {
            return false;
        }
        last = now;
        _until = now + duration;
        _priority = priority;
        return true;
    }
}
