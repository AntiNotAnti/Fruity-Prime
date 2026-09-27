#pragma once

#include "GamepadState.hpp"
#include "../../NativeRuntime/System/Numerics.hpp"

#include <cmath>
#include <cstdint>
#include <string>
#include <utility>

namespace MphRead::Mods::Input
{
    enum class GamepadFamily : std::int32_t;
    enum class GamepadCapabilities : std::int32_t;

    namespace GamepadDeviceSnapshotEquality
    {
        [[nodiscard]] inline bool Single(float left, float right) noexcept
        {
            return left == right || (std::isnan(left) && std::isnan(right));
        }

        [[nodiscard]] inline bool State(const GamepadState& left, const GamepadState& right) noexcept
        {
            return left.Connected == right.Connected
                && Single(left.LeftX, right.LeftX) && Single(left.LeftY, right.LeftY)
                && Single(left.RightX, right.RightX) && Single(left.RightY, right.RightY)
                && Single(left.LeftTrigger, right.LeftTrigger) && Single(left.RightTrigger, right.RightTrigger)
                && left.Buttons == right.Buttons && left.Name == right.Name;
        }
    }

    struct GamepadAuxState
    {
    private:
        System::Numerics::Vector3 _gyro{};
        System::Numerics::Vector3 _accelerometer{};
        System::Numerics::Vector2 _touch0{};
        System::Numerics::Vector2 _touch1{};
        bool _touchpadPressed = false;

    public:
        GamepadAuxState(System::Numerics::Vector3 gyro = {}, System::Numerics::Vector3 accelerometer = {},
            System::Numerics::Vector2 touch0 = {}, System::Numerics::Vector2 touch1 = {}, bool touchpadPressed = false)
            : _gyro(gyro), _accelerometer(accelerometer), _touch0(touch0), _touch1(touch1), _touchpadPressed(touchpadPressed)
        {
        }

        [[nodiscard]] const System::Numerics::Vector3& Gyro() const noexcept { return _gyro; }
        [[nodiscard]] const System::Numerics::Vector3& Accelerometer() const noexcept { return _accelerometer; }
        [[nodiscard]] const System::Numerics::Vector2& Touch0() const noexcept { return _touch0; }
        [[nodiscard]] const System::Numerics::Vector2& Touch1() const noexcept { return _touch1; }
        [[nodiscard]] bool TouchpadPressed() const noexcept { return _touchpadPressed; }

        friend bool operator==(const GamepadAuxState& left, const GamepadAuxState& right) noexcept
        {
            using GamepadDeviceSnapshotEquality::Single;
            return Single(left._gyro.X, right._gyro.X) && Single(left._gyro.Y, right._gyro.Y)
                && Single(left._gyro.Z, right._gyro.Z)
                && Single(left._accelerometer.X, right._accelerometer.X)
                && Single(left._accelerometer.Y, right._accelerometer.Y)
                && Single(left._accelerometer.Z, right._accelerometer.Z)
                && Single(left._touch0.X, right._touch0.X) && Single(left._touch0.Y, right._touch0.Y)
                && Single(left._touch1.X, right._touch1.X) && Single(left._touch1.Y, right._touch1.Y)
                && left._touchpadPressed == right._touchpadPressed;
        }
    };

    struct GamepadDeviceSnapshot
    {
    private:
        std::string _deviceId{};
        std::string _name{};
        std::string _profileKey{};
        GamepadFamily _family{};
        GamepadCapabilities _capabilities{};
        bool _isMapped = false;
        std::string _mapping{};
        GamepadState _state{};
        GamepadState _rawState{};
        GamepadAuxState _auxState{};
        std::int64_t _revision = 0;

    public:
        GamepadDeviceSnapshot(std::string deviceId = {}, std::string name = {}, std::string profileKey = {},
            GamepadFamily family = {}, GamepadCapabilities capabilities = {}, bool isMapped = false,
            std::string mapping = {}, GamepadState state = {}, GamepadState rawState = {},
            GamepadAuxState auxState = {}, std::int64_t revision = 0)
            : _deviceId(std::move(deviceId)), _name(std::move(name)), _profileKey(std::move(profileKey)),
              _family(family), _capabilities(capabilities), _isMapped(isMapped), _mapping(std::move(mapping)),
              _state(std::move(state)), _rawState(std::move(rawState)), _auxState(std::move(auxState)), _revision(revision)
        {
        }

        [[nodiscard]] const std::string& DeviceId() const noexcept { return _deviceId; }
        [[nodiscard]] const std::string& Name() const noexcept { return _name; }
        [[nodiscard]] const std::string& ProfileKey() const noexcept { return _profileKey; }
        [[nodiscard]] GamepadFamily Family() const noexcept { return _family; }
        [[nodiscard]] GamepadCapabilities Capabilities() const noexcept { return _capabilities; }
        [[nodiscard]] bool IsMapped() const noexcept { return _isMapped; }
        [[nodiscard]] const std::string& Mapping() const noexcept { return _mapping; }
        [[nodiscard]] const GamepadState& State() const noexcept { return _state; }
        [[nodiscard]] const GamepadState& RawState() const noexcept { return _rawState; }
        [[nodiscard]] const GamepadAuxState& AuxState() const noexcept { return _auxState; }
        [[nodiscard]] std::int64_t Revision() const noexcept { return _revision; }

        friend bool operator==(const GamepadDeviceSnapshot& left, const GamepadDeviceSnapshot& right) noexcept
        {
            return left._deviceId == right._deviceId && left._name == right._name && left._profileKey == right._profileKey
                && left._family == right._family && left._capabilities == right._capabilities
                && left._isMapped == right._isMapped && left._mapping == right._mapping
                && GamepadDeviceSnapshotEquality::State(left._state, right._state)
                && GamepadDeviceSnapshotEquality::State(left._rawState, right._rawState)
                && left._auxState == right._auxState && left._revision == right._revision;
        }
    };
}
