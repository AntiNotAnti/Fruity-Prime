#include "GamepadBridge.hpp"
#include "AndroidGamepadProfile.hpp"
#include "AndroidGamepadHaptics.hpp"

#if !defined(__ANDROID__)
#error "GamepadBridge is only valid for the Android native target."
#endif

#include "../MphRead.Native/Mods/Input/GamepadAnalog.hpp"
#include "../MphRead.Native/Mods/Input/GamepadGlyphs.hpp"
#include "../MphRead.Native/Mods/Input/GamepadHaptics.hpp"
#include "../MphRead.Native/Mods/Input/GamepadManager.hpp"
#include "../MphRead.Native/Mods/Input/GamepadState.hpp"

#include <android/input.h>

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace
{
    using MphRead::Mods::Input::GamepadButtons;
    using MphRead::Mods::Input::GamepadCapabilities;
    using MphRead::Mods::Input::GamepadEventState;
    using MphRead::Mods::Input::GamepadFamily;
    using MphRead::Mods::Input::GamepadGlyphs;
    using MphRead::Mods::Input::GamepadHaptics;
    using MphRead::Mods::Input::GamepadManager;
    using MphRead::Mods::Input::GamepadState;

    constexpr float HatPress = 0.5F;

    struct Pad final
    {
        std::string Id;
        std::string Name;
        MphRead::Droid::AndroidGamepadProfile Profile;
        GamepadFamily Family = GamepadFamily::Unknown;
        GamepadEventState Input;
    };

    struct BridgeState final
    {
        std::map<std::int32_t, Pad> Pads;
        std::int32_t Generation = 0;
    };

    BridgeState& State()
    {
        static BridgeState state;
        return state;
    }

    void ClearPendingException(JNIEnv* env) noexcept
    {
        if (env != nullptr && env->ExceptionCheck())
        {
            env->ExceptionClear();
        }
    }

    template <typename T>
    class LocalRef final
    {
    public:
        LocalRef(JNIEnv* env, T value) noexcept
            : _env(env), _value(value)
        {
        }

        ~LocalRef()
        {
            if (_env != nullptr && _value != nullptr)
            {
                _env->DeleteLocalRef(_value);
            }
        }

        LocalRef(const LocalRef&) = delete;
        LocalRef& operator=(const LocalRef&) = delete;

        [[nodiscard]] T Get() const noexcept { return _value; }
        [[nodiscard]] explicit operator bool() const noexcept { return _value != nullptr; }

    private:
        JNIEnv* _env;
        T _value;
    };

    std::int32_t Bits(GamepadButtons value) noexcept
    {
        return static_cast<std::int32_t>(value);
    }

    bool IsGamepad(std::int32_t source) noexcept
    {
        return (source & AINPUT_SOURCE_GAMEPAD) == AINPUT_SOURCE_GAMEPAD
            || (source & AINPUT_SOURCE_JOYSTICK) == AINPUT_SOURCE_JOYSTICK;
    }

    bool TryGetMethod(
        JNIEnv* env,
        jobject object,
        const char* name,
        const char* signature,
        jmethodID& method
    )
    {
        jclass type = env->GetObjectClass(object);
        if (type == nullptr || env->ExceptionCheck())
        {
            if (type != nullptr)
            {
                env->DeleteLocalRef(type);
            }
            ClearPendingException(env);
            return false;
        }

        method = env->GetMethodID(type, name, signature);
        env->DeleteLocalRef(type);
        if (method == nullptr || env->ExceptionCheck())
        {
            ClearPendingException(env);
            return false;
        }
        return true;
    }

    bool TryGetInt(
        JNIEnv* env,
        jobject event,
        const char* name,
        std::int32_t& value
    )
    {
        jmethodID method = nullptr;
        if (!TryGetMethod(env, event, name, "()I", method))
        {
            return false;
        }

        const jint result = env->CallIntMethod(event, method);
        if (env->ExceptionCheck())
        {
            ClearPendingException(env);
            return false;
        }

        value = static_cast<std::int32_t>(result);
        return true;
    }

    bool TryGetAxisValue(
        JNIEnv* env,
        jobject event,
        jmethodID method,
        std::int32_t axis,
        float& value
    )
    {
        const jfloat result = env->CallFloatMethod(
            event, method, static_cast<jint>(axis)
        );
        if (env->ExceptionCheck())
        {
            ClearPendingException(env);
            return false;
        }

        value = static_cast<float>(result);
        return true;
    }

    bool TryReadProfileAxis(
        JNIEnv* env,
        jobject event,
        jmethodID getAxisValue,
        const std::optional<std::int32_t>& axis,
        float& value)
    {
        if (MphRead::Droid::AndroidGamepadProfile::Read(
                env, event, getAxisValue, axis, value))
        {
            return true;
        }
        ClearPendingException(env);
        return false;
    }

    GamepadButtons Map(std::int32_t code) noexcept
    {
        switch (code)
        {
        case AKEYCODE_BUTTON_A:
            return GamepadButtons::A;
        case AKEYCODE_BUTTON_B:
            return GamepadButtons::B;
        case AKEYCODE_BUTTON_X:
            return GamepadButtons::X;
        case AKEYCODE_BUTTON_Y:
            return GamepadButtons::Y;
        case AKEYCODE_BUTTON_L1:
            return GamepadButtons::LeftBumper;
        case AKEYCODE_BUTTON_R1:
            return GamepadButtons::RightBumper;
        case AKEYCODE_BUTTON_L2:
            return GamepadButtons::LeftTrigger;
        case AKEYCODE_BUTTON_R2:
            return GamepadButtons::RightTrigger;
        case AKEYCODE_BUTTON_SELECT:
            return GamepadButtons::Back;
        case AKEYCODE_BUTTON_START:
            return GamepadButtons::Start;
        case AKEYCODE_BUTTON_THUMBL:
            return GamepadButtons::LeftThumb;
        case AKEYCODE_BUTTON_THUMBR:
            return GamepadButtons::RightThumb;
        case AKEYCODE_DPAD_UP:
            return GamepadButtons::DpadUp;
        case AKEYCODE_DPAD_DOWN:
            return GamepadButtons::DpadDown;
        case AKEYCODE_DPAD_LEFT:
            return GamepadButtons::DpadLeft;
        case AKEYCODE_DPAD_RIGHT:
            return GamepadButtons::DpadRight;
        default:
            return GamepadButtons::None;
        }
    }

    bool TryGetDeviceString(
        JNIEnv* env,
        jobject device,
        const char* name,
        std::optional<std::string>& value)
    {
        jmethodID method = nullptr;
        if (!TryGetMethod(env, device, name, "()Ljava/lang/String;", method))
        {
            return false;
        }

        LocalRef<jstring> result(env, static_cast<jstring>(
            env->CallObjectMethod(device, method)));
        if (env->ExceptionCheck())
        {
            ClearPendingException(env);
            return false;
        }
        if (!result)
        {
            value = std::nullopt;
            return true;
        }

        const char* chars = env->GetStringUTFChars(result.Get(), nullptr);
        if (chars == nullptr || env->ExceptionCheck())
        {
            ClearPendingException(env);
            return false;
        }
        value = std::string(chars);
        env->ReleaseStringUTFChars(result.Get(), chars);
        return true;
    }

    bool TryGetInputDevice(
        JNIEnv* env,
        std::int32_t deviceId,
        jobject& device)
    {
        LocalRef<jclass> inputDeviceClass(
            env, env->FindClass("android/view/InputDevice"));
        if (!inputDeviceClass || env->ExceptionCheck())
        {
            ClearPendingException(env);
            return false;
        }

        const jmethodID getDevice = env->GetStaticMethodID(
            inputDeviceClass.Get(), "getDevice", "(I)Landroid/view/InputDevice;");
        if (getDevice == nullptr || env->ExceptionCheck())
        {
            ClearPendingException(env);
            return false;
        }

        device = env->CallStaticObjectMethod(
            inputDeviceClass.Get(), getDevice, static_cast<jint>(deviceId));
        if (env->ExceptionCheck())
        {
            if (device != nullptr)
            {
                env->DeleteLocalRef(device);
                device = nullptr;
            }
            ClearPendingException(env);
            return false;
        }
        return device != nullptr;
    }

    bool TryGetDeviceIds(JNIEnv* env, std::vector<std::int32_t>& ids)
    {
        LocalRef<jclass> inputDeviceClass(
            env, env->FindClass("android/view/InputDevice"));
        if (!inputDeviceClass || env->ExceptionCheck())
        {
            ClearPendingException(env);
            return false;
        }
        const jmethodID getDeviceIds = env->GetStaticMethodID(
            inputDeviceClass.Get(), "getDeviceIds", "()[I");
        if (getDeviceIds == nullptr || env->ExceptionCheck())
        {
            ClearPendingException(env);
            return false;
        }
        LocalRef<jintArray> array(env, static_cast<jintArray>(
            env->CallStaticObjectMethod(inputDeviceClass.Get(), getDeviceIds)));
        if (env->ExceptionCheck())
        {
            ClearPendingException(env);
            return false;
        }
        if (!array)
        {
            return true;
        }

        const jsize length = env->GetArrayLength(array.Get());
        if (env->ExceptionCheck())
        {
            ClearPendingException(env);
            return false;
        }
        std::vector<jint> values(static_cast<std::size_t>(length));
        if (length > 0)
        {
            env->GetIntArrayRegion(array.Get(), 0, length, values.data());
            if (env->ExceptionCheck())
            {
                ClearPendingException(env);
                return false;
            }
        }
        ids.assign(values.begin(), values.end());
        return true;
    }

    void Publish(Pad& pad)
    {
        GamepadState state = pad.Input.Snapshot();
        state.Connected = true;
        state.Name = pad.Name;

        GamepadCapabilities capabilities = GamepadCapabilities::None;
        if (pad.Profile.LeftTrigger().has_value()
            && pad.Profile.RightTrigger().has_value())
        {
            capabilities = capabilities | GamepadCapabilities::AnalogTriggers;
        }
        if (pad.Profile.HasLeftStick())
        {
            capabilities = capabilities | GamepadCapabilities::AnalogLeftStick;
        }
        if (pad.Profile.RightX().has_value() && pad.Profile.RightY().has_value())
        {
            capabilities = capabilities | GamepadCapabilities::AnalogRightStick;
        }
        if (GamepadHaptics::Available(pad.Id))
        {
            capabilities = capabilities | GamepadCapabilities::Rumble;
        }

        GamepadManager::UpdateDevice(
            pad.Id, state, true, pad.Family, capabilities);
    }

    Pad* Ensure(JNIEnv* env, std::int32_t deviceId)
    {
        if (env == nullptr)
        {
            return nullptr;
        }
        BridgeState& state = State();
        const auto existing = state.Pads.find(deviceId);
        if (existing != state.Pads.end())
        {
            return &existing->second;
        }

        jobject rawDevice = nullptr;
        if (!TryGetInputDevice(env, deviceId, rawDevice))
        {
            return nullptr;
        }
        LocalRef<jobject> device(env, rawDevice);

        std::int32_t sources = 0;
        std::int32_t vendorId = 0;
        std::optional<std::string> name;
        std::optional<std::string> descriptor;
        if (!TryGetInt(env, device.Get(), "getSources", sources)
            || !IsGamepad(sources)
            || !TryGetDeviceString(env, device.Get(), "getName", name)
            || !TryGetDeviceString(env, device.Get(), "getDescriptor", descriptor)
            || !TryGetInt(env, device.Get(), "getVendorId", vendorId))
        {
            return nullptr;
        }

        std::optional<MphRead::Droid::AndroidGamepadProfile> profile
            = MphRead::Droid::AndroidGamepadProfile::ForDevice(env, deviceId);
        if (env->ExceptionCheck())
        {
            ClearPendingException(env);
        }
        if (!profile)
        {
            return nullptr;
        }

        Pad pad{};
        pad.Name = name.value_or("gamepad");
        pad.Id = "android:" + descriptor.value_or("") + ":"
            + std::to_string(deviceId) + ":" + std::to_string(++state.Generation);
        pad.Profile = *profile;
        pad.Family = GamepadGlyphs::Detect(name.value_or(""), std::nullopt, vendorId);
        auto [found, inserted] = state.Pads.emplace(deviceId, std::move(pad));
        if (!inserted)
        {
            return &found->second;
        }

        std::shared_ptr<MphRead::Droid::AndroidGamepadHaptics> haptics
            = MphRead::Droid::AndroidGamepadHaptics::Create(env, deviceId);
        if (haptics)
        {
            GamepadHaptics::Register(found->second.Id, std::move(haptics));
        }
        Publish(found->second);
        return &found->second;
    }

    void Remove(std::int32_t deviceId)
    {
        BridgeState& state = State();
        const auto found = state.Pads.find(deviceId);
        if (found == state.Pads.end())
        {
            return;
        }
        found->second.Input.Clear();
        GamepadHaptics::Unregister(found->second.Id);
        GamepadManager::RemoveDevice(found->second.Id);
        state.Pads.erase(found);
    }
}

namespace MphRead::Droid
{
    void GamepadBridge::Start(
        JNIEnv* env,
        const std::function<void()>& registerListener)
    {
        Stop();
        if (registerListener)
        {
            registerListener();
        }
        if (env == nullptr)
        {
            return;
        }
        std::vector<std::int32_t> ids;
        if (!TryGetDeviceIds(env, ids))
        {
            return;
        }
        for (const std::int32_t id : ids)
        {
            static_cast<void>(Ensure(env, id));
        }
    }

    void GamepadBridge::Stop()
    {
        BridgeState& state = State();
        for (const auto& [deviceId, pad] : state.Pads)
        {
            static_cast<void>(deviceId);
            GamepadHaptics::Unregister(pad.Id);
            GamepadManager::RemoveDevice(pad.Id);
        }
        state.Pads.clear();
    }

    void GamepadBridge::Clear()
    {
        BridgeState& state = State();
        for (auto& [deviceId, pad] : state.Pads)
        {
            static_cast<void>(deviceId);
            pad.Input.Clear();
            GamepadManager::ClearDevice(pad.Id);
        }
        GamepadHaptics::Stop();
    }

    void GamepadBridge::DeviceAdded(JNIEnv* env, std::int32_t deviceId)
    {
        static_cast<void>(Ensure(env, deviceId));
    }

    void GamepadBridge::DeviceChanged(JNIEnv* env, std::int32_t deviceId)
    {
        Remove(deviceId);
        static_cast<void>(Ensure(env, deviceId));
    }

    void GamepadBridge::DeviceRemoved(std::int32_t deviceId)
    {
        Remove(deviceId);
    }

    bool GamepadBridge::HandleKey(
        std::int32_t keyCode,
        jobject event,
        bool down,
        JNIEnv* env
    )
    {
        if (event == nullptr || env == nullptr)
        {
            return false;
        }

        std::int32_t deviceId = 0;
        if (!TryGetInt(env, event, "getDeviceId", deviceId))
        {
            return false;
        }
        Pad* pad = Ensure(env, deviceId);
        if (pad == nullptr)
        {
            return false;
        }

        const GamepadButtons button = Map(keyCode);
        if (button == GamepadButtons::None)
        {
            return false;
        }

        if (down)
        {
            std::int32_t repeatCount = 0;
            if (!TryGetInt(env, event, "getRepeatCount", repeatCount))
            {
                return false;
            }
            if (repeatCount > 0)
            {
                return true;
            }
        }

        pad->Input.Key(button, down);
        Publish(*pad);
        return true;
    }

    bool GamepadBridge::HandleMotion(
        jobject event,
        JNIEnv* env
    )
    {
        if (event == nullptr || env == nullptr)
        {
            return false;
        }

        std::int32_t source = 0;
        if (!TryGetInt(env, event, "getSource", source)
            || !IsGamepad(source))
        {
            return false;
        }

        std::int32_t action = 0;
        if (!TryGetInt(env, event, "getAction", action)
            || action != AMOTION_EVENT_ACTION_MOVE)
        {
            return false;
        }

        std::int32_t deviceId = 0;
        if (!TryGetInt(env, event, "getDeviceId", deviceId))
        {
            return false;
        }
        Pad* pad = Ensure(env, deviceId);
        if (pad == nullptr)
        {
            return false;
        }

        jmethodID getAxisValue = nullptr;
        if (!TryGetMethod(
                env, event, "getAxisValue", "(I)F", getAxisValue
            ))
        {
            return false;
        }

        GamepadState state{};
        state.Connected = true;
        state.Name = pad->Name;

        if (!TryGetAxisValue(
                env, event, getAxisValue, AMOTION_EVENT_AXIS_X, state.LeftX
            ))
        {
            return false;
        }

        float value = 0.0F;
        if (!TryGetAxisValue(
                env, event, getAxisValue, AMOTION_EVENT_AXIS_Y, value
            ))
        {
            return false;
        }
        state.LeftY = -value;

        if (!TryReadProfileAxis(
                env, event, getAxisValue, pad->Profile.RightX(), state.RightX))
        {
            return false;
        }

        if (!TryReadProfileAxis(
                env, event, getAxisValue, pad->Profile.RightY(), value))
        {
            return false;
        }
        state.RightY = -value;

        if (!TryReadProfileAxis(
                env, event, getAxisValue, pad->Profile.LeftTrigger(), state.LeftTrigger))
        {
            return false;
        }

        if (!TryReadProfileAxis(
                env, event, getAxisValue, pad->Profile.RightTrigger(), state.RightTrigger))
        {
            return false;
        }

        GamepadButtons buttons = GamepadButtons::None;

        float hatX = 0.0F;
        if (!TryGetAxisValue(
                env, event, getAxisValue, AMOTION_EVENT_AXIS_HAT_X, hatX
            ))
        {
            return false;
        }

        float hatY = 0.0F;
        if (!TryGetAxisValue(
                env, event, getAxisValue, AMOTION_EVENT_AXIS_HAT_Y, hatY
            ))
        {
            return false;
        }

        if (hatX < -HatPress)
        {
            buttons = static_cast<GamepadButtons>(
                Bits(buttons) | Bits(GamepadButtons::DpadLeft)
            );
        }
        else if (hatX > HatPress)
        {
            buttons = static_cast<GamepadButtons>(
                Bits(buttons) | Bits(GamepadButtons::DpadRight)
            );
        }

        if (hatY < -HatPress)
        {
            buttons = static_cast<GamepadButtons>(
                Bits(buttons) | Bits(GamepadButtons::DpadUp)
            );
        }
        else if (hatY > HatPress)
        {
            buttons = static_cast<GamepadButtons>(
                Bits(buttons) | Bits(GamepadButtons::DpadDown)
            );
        }

        state.Buttons = buttons;
        pad->Input.Motion = state;
        Publish(*pad);
        return true;
    }
}
