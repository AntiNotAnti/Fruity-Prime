#pragma once

#include <cmath>

namespace MphRead::Mods::Input
{
    struct SpectatorInput
    {
    private:
        bool _nextPlayer = false;
        bool _previousPlayer = false;
        bool _toggleView = false;
        bool _scoreboard = false;
        bool _openMenu = false;
        float _moveX = 0;
        float _moveY = 0;
        float _lookX = 0;
        float _lookY = 0;
        float _ascend = 0;
        float _descend = 0;

    public:
        [[nodiscard]] bool NextPlayer() const noexcept { return _nextPlayer; }
        [[nodiscard]] bool PreviousPlayer() const noexcept { return _previousPlayer; }
        [[nodiscard]] bool ToggleView() const noexcept { return _toggleView; }
        [[nodiscard]] bool Scoreboard() const noexcept { return _scoreboard; }
        [[nodiscard]] bool OpenMenu() const noexcept { return _openMenu; }
        [[nodiscard]] float MoveX() const noexcept { return _moveX; }
        [[nodiscard]] float MoveY() const noexcept { return _moveY; }
        [[nodiscard]] float LookX() const noexcept { return _lookX; }
        [[nodiscard]] float LookY() const noexcept { return _lookY; }
        [[nodiscard]] float Ascend() const noexcept { return _ascend; }
        [[nodiscard]] float Descend() const noexcept { return _descend; }

        [[nodiscard]] static SpectatorInput ReadController(bool replay = false);
        void ApplyView(bool replay = false) const;

        friend bool operator==(const SpectatorInput& left, const SpectatorInput& right) noexcept
        {
            const auto equals = [](float a, float b) noexcept
            {
                return a == b || (std::isnan(a) && std::isnan(b));
            };
            return left._nextPlayer == right._nextPlayer && left._previousPlayer == right._previousPlayer
                && left._toggleView == right._toggleView && left._scoreboard == right._scoreboard
                && left._openMenu == right._openMenu && equals(left._moveX, right._moveX)
                && equals(left._moveY, right._moveY) && equals(left._lookX, right._lookX)
                && equals(left._lookY, right._lookY) && equals(left._ascend, right._ascend)
                && equals(left._descend, right._descend);
        }
    };
}
