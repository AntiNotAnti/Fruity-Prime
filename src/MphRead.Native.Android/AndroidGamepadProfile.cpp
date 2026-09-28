#include "AndroidGamepadProfile.hpp"

#if !defined(__ANDROID__)
#error "AndroidGamepadProfile is only valid for the Android native target."
#endif

#include <android/input.h>

namespace
{
    template <typename T>
    class LocalRef final
    {
    public:
        LocalRef(JNIEnv* env, T value) noexcept
            : _env(env),
              _value(value)
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

    [[nodiscard]] bool HasMotionRange(
        JNIEnv* env,
        jobject device,
        jmethodID getMotionRange,
        std::int32_t axis,
        bool& hasRange)
    {
        LocalRef<jobject> range(env, env->CallObjectMethod(
            device, getMotionRange, static_cast<jint>(axis)));
        if (env->ExceptionCheck())
        {
            return false;
        }
        hasRange = static_cast<bool>(range);
        return true;
    }

    [[nodiscard]] bool PickAxis(
        JNIEnv* env,
        jobject device,
        jmethodID getMotionRange,
        std::int32_t first,
        std::int32_t second,
        std::optional<std::int32_t>& result)
    {
        bool available = false;
        if (!HasMotionRange(env, device, getMotionRange, first, available))
        {
            return false;
        }
        if (available)
        {
            result = first;
            return true;
        }

        if (!HasMotionRange(env, device, getMotionRange, second, available))
        {
            return false;
        }
        if (available)
        {
            result = second;
        }
        return true;
    }
}

namespace MphRead::Droid
{
    std::optional<AndroidGamepadProfile> AndroidGamepadProfile::ForDevice(
        JNIEnv* env,
        std::int32_t deviceId)
    {
        if (env == nullptr || env->ExceptionCheck())
        {
            return std::nullopt;
        }

        LocalRef<jclass> inputDeviceClass(
            env, env->FindClass("android/view/InputDevice"));
        if (!inputDeviceClass || env->ExceptionCheck())
        {
            return std::nullopt;
        }

        const jmethodID getDevice = env->GetStaticMethodID(
            inputDeviceClass.Get(),
            "getDevice",
            "(I)Landroid/view/InputDevice;");
        if (getDevice == nullptr || env->ExceptionCheck())
        {
            return std::nullopt;
        }

        LocalRef<jobject> device(env, env->CallStaticObjectMethod(
            inputDeviceClass.Get(), getDevice, static_cast<jint>(deviceId)));
        if (!device || env->ExceptionCheck())
        {
            return std::nullopt;
        }

        const jmethodID getMotionRange = env->GetMethodID(
            inputDeviceClass.Get(),
            "getMotionRange",
            "(I)Landroid/view/InputDevice$MotionRange;");
        if (getMotionRange == nullptr || env->ExceptionCheck())
        {
            return std::nullopt;
        }

        AndroidGamepadProfile profile;
        bool hasX = false;
        bool hasY = false;
        if (!HasMotionRange(env, device.Get(), getMotionRange,
                AMOTION_EVENT_AXIS_X, hasX)
            || !HasMotionRange(env, device.Get(), getMotionRange,
                AMOTION_EVENT_AXIS_Y, hasY))
        {
            return std::nullopt;
        }
        profile._hasLeftStick = hasX && hasY;

        if (!PickAxis(env, device.Get(), getMotionRange,
                AMOTION_EVENT_AXIS_Z, AMOTION_EVENT_AXIS_RX, profile._rightX)
            || !PickAxis(env, device.Get(), getMotionRange,
                AMOTION_EVENT_AXIS_RZ, AMOTION_EVENT_AXIS_RY, profile._rightY)
            || !PickAxis(env, device.Get(), getMotionRange,
                AMOTION_EVENT_AXIS_LTRIGGER, AMOTION_EVENT_AXIS_BRAKE,
                profile._leftTrigger)
            || !PickAxis(env, device.Get(), getMotionRange,
                AMOTION_EVENT_AXIS_RTRIGGER, AMOTION_EVENT_AXIS_GAS,
                profile._rightTrigger))
        {
            return std::nullopt;
        }

        return profile;
    }

    bool AndroidGamepadProfile::HasLeftStick() const noexcept
    {
        return _hasLeftStick;
    }

    const std::optional<std::int32_t>& AndroidGamepadProfile::RightX() const noexcept
    {
        return _rightX;
    }

    const std::optional<std::int32_t>& AndroidGamepadProfile::RightY() const noexcept
    {
        return _rightY;
    }

    const std::optional<std::int32_t>& AndroidGamepadProfile::LeftTrigger() const noexcept
    {
        return _leftTrigger;
    }

    const std::optional<std::int32_t>& AndroidGamepadProfile::RightTrigger() const noexcept
    {
        return _rightTrigger;
    }

    bool AndroidGamepadProfile::Read(
        JNIEnv* env,
        jobject event,
        jmethodID getAxisValue,
        const std::optional<std::int32_t>& axis,
        float& value)
    {
        if (!axis)
        {
            value = 0.0F;
            return true;
        }
        if (env == nullptr || event == nullptr || getAxisValue == nullptr)
        {
            return false;
        }

        const jfloat result = env->CallFloatMethod(
            event, getAxisValue, static_cast<jint>(*axis));
        if (env->ExceptionCheck())
        {
            return false;
        }
        value = static_cast<float>(result);
        return true;
    }
}
