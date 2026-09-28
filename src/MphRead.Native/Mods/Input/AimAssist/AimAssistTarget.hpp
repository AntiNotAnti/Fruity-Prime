#pragma once

#include "../../../NativeRuntime/System/Numerics.hpp"

#include <cmath>
#include <cstdint>
#include <string>

namespace MphRead::Mods::Input::AimAssist
{
    enum class AimAssistPointType : std::int32_t { CenterMass, UpperChest, Head };

    // AimAssistPointType.ToString().
    [[nodiscard]] std::string ToString(AimAssistPointType value);

    class AimAssistTarget final
    {
    public:
        AimAssistTarget() noexcept = default;
        constexpr AimAssistTarget(std::int32_t slot, std::int64_t life, System::Numerics::Vector2 bodyError,
            System::Numerics::Vector2 headError, float distance, bool bodyVisible, bool headVisible,
            bool eligible = true, AimAssistPointType bodyPointType = AimAssistPointType::UpperChest) noexcept
            : _slot(slot), _life(life), _bodyError(bodyError), _headError(headError), _distance(distance),
              _bodyVisible(bodyVisible), _headVisible(headVisible), _eligible(eligible), _bodyPointType(bodyPointType)
        {
        }

        [[nodiscard]] constexpr std::int32_t Slot() const noexcept { return _slot; }
        [[nodiscard]] constexpr std::int64_t Life() const noexcept { return _life; }
        [[nodiscard]] constexpr System::Numerics::Vector2 BodyError() const noexcept { return _bodyError; }
        [[nodiscard]] constexpr System::Numerics::Vector2 HeadError() const noexcept { return _headError; }
        [[nodiscard]] constexpr float Distance() const noexcept { return _distance; }
        [[nodiscard]] constexpr bool BodyVisible() const noexcept { return _bodyVisible; }
        [[nodiscard]] constexpr bool HeadVisible() const noexcept { return _headVisible; }
        [[nodiscard]] constexpr bool Eligible() const noexcept { return _eligible; }
        [[nodiscard]] constexpr AimAssistPointType BodyPointType() const noexcept { return _bodyPointType; }

        friend bool operator==(const AimAssistTarget& left, const AimAssistTarget& right) noexcept
        {
            return left._slot == right._slot && left._life == right._life
                && Equal(left._bodyError, right._bodyError) && Equal(left._headError, right._headError)
                && Equal(left._distance, right._distance) && left._bodyVisible == right._bodyVisible
                && left._headVisible == right._headVisible && left._eligible == right._eligible
                && left._bodyPointType == right._bodyPointType;
        }

    private:
        std::int32_t _slot = 0;
        std::int64_t _life = 0;
        System::Numerics::Vector2 _bodyError{};
        System::Numerics::Vector2 _headError{};
        float _distance = 0;
        bool _bodyVisible = false;
        bool _headVisible = false;
        bool _eligible = false;
        AimAssistPointType _bodyPointType = AimAssistPointType::CenterMass;

        [[nodiscard]] static bool Equal(float left, float right) noexcept
        {
            return left == right || (std::isnan(left) && std::isnan(right));
        }

        [[nodiscard]] static bool Equal(System::Numerics::Vector2 left, System::Numerics::Vector2 right) noexcept
        {
            return Equal(left.X, right.X) && Equal(left.Y, right.Y);
        }
    };

    class AimAssistResult final
    {
    public:
        AimAssistResult() noexcept = default;
        constexpr AimAssistResult(float x, float y, std::int32_t targetSlot = -1, float friction = 1,
            float rotationStrength = 0, AimAssistPointType pointType = AimAssistPointType::UpperChest,
            float headBlend = 0, float score = 0) noexcept
            : _x(x), _y(y), _targetSlot(targetSlot), _friction(friction), _rotationStrength(rotationStrength),
              _pointType(pointType), _headBlend(headBlend), _score(score)
        {
        }

        [[nodiscard]] constexpr float X() const noexcept { return _x; }
        [[nodiscard]] constexpr float Y() const noexcept { return _y; }
        [[nodiscard]] constexpr std::int32_t TargetSlot() const noexcept { return _targetSlot; }
        [[nodiscard]] constexpr float Friction() const noexcept { return _friction; }
        [[nodiscard]] constexpr float RotationStrength() const noexcept { return _rotationStrength; }
        [[nodiscard]] constexpr AimAssistPointType PointType() const noexcept { return _pointType; }
        [[nodiscard]] constexpr float HeadBlend() const noexcept { return _headBlend; }
        [[nodiscard]] constexpr float Score() const noexcept { return _score; }

        friend bool operator==(const AimAssistResult& left, const AimAssistResult& right) noexcept
        {
            return Equal(left._x, right._x) && Equal(left._y, right._y) && left._targetSlot == right._targetSlot
                && Equal(left._friction, right._friction) && Equal(left._rotationStrength, right._rotationStrength)
                && left._pointType == right._pointType && Equal(left._headBlend, right._headBlend)
                && Equal(left._score, right._score);
        }

    private:
        float _x = 0;
        float _y = 0;
        std::int32_t _targetSlot = 0;
        float _friction = 0;
        float _rotationStrength = 0;
        AimAssistPointType _pointType = AimAssistPointType::CenterMass;
        float _headBlend = 0;
        float _score = 0;

        [[nodiscard]] static bool Equal(float left, float right) noexcept
        {
            return left == right || (std::isnan(left) && std::isnan(right));
        }
    };
}
