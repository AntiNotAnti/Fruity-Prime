#include "SpectatorInput.hpp"

#include "GamepadAnalog.hpp"
#include "GamepadInput.hpp"
#include "GamepadOptions.hpp"
#include "GamepadUiRouter.hpp"
#include "../SpectatorMode.hpp"

namespace MphRead::Mods::Input
{
    SpectatorInput SpectatorInput::ReadController(bool replay)
    {
        static_cast<void>(replay);
        if (!GamepadContexts::Focused() || GamepadContexts::Current() != GamepadContext::Gameplay)
        {
            return {};
        }
        const GamepadState pad = GamepadInput::State();
        const auto move = GamepadAnalog::ApplyRadialDeadZone(pad.LeftX, pad.LeftY, GamepadOptions::LeftInner(), GamepadOptions::LeftOuter());
        SpectatorInput input{};
        input._nextPlayer = GamepadInput::TakePress(GamepadButtons::RightBumper);
        input._previousPlayer = GamepadInput::TakePress(GamepadButtons::LeftBumper);
        input._toggleView = GamepadInput::TakePress(GamepadButtons::Y);
        input._scoreboard = pad.Down(GamepadButtons::Back);
        input._openMenu = GamepadInput::TakePress(GamepadButtons::Start);
        input._moveX = move.first;
        input._moveY = move.second;
        input._lookX = GamepadInput::AimDeltaX();
        input._lookY = GamepadInput::AimDeltaY();
        input._ascend = pad.RightTrigger;
        input._descend = pad.LeftTrigger;
        return input;
    }

    void SpectatorInput::ApplyView(bool replay) const
    {
        static_cast<void>(replay);
        if (_nextPlayer)
        {
            SpectatorMode::CycleNext();
        }
        if (_previousPlayer)
        {
            SpectatorMode::CyclePrevious();
        }
        if (_toggleView)
        {
            SpectatorMode::ToggleView();
        }
    }
}
