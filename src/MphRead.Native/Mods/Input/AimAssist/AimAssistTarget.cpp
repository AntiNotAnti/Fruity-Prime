#include "AimAssistTarget.hpp"

#include <string>

namespace MphRead::Mods::Input::AimAssist
{
    std::string ToString(AimAssistPointType value)
    {
        switch (value)
        {
        case AimAssistPointType::CenterMass: return "CenterMass";
        case AimAssistPointType::UpperChest: return "UpperChest";
        case AimAssistPointType::Head: return "Head";
        }
        return std::to_string(static_cast<std::int32_t>(value));
    }
}
