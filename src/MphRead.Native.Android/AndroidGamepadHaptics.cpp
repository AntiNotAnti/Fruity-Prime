#include "AndroidGamepadHaptics.hpp"

#if !defined(__ANDROID__)
#error "AndroidGamepadHaptics is only valid for the Android native target."
#endif

#include <algorithm>
#include <cstdint>
#include <stdexcept>

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

    [[noreturn]] void ThrowJavaException(JNIEnv* env)
    {
        LocalRef<jthrowable> exception(env, env->ExceptionOccurred());
        env->ExceptionClear();
        (void)exception;
        throw std::runtime_error("Android gamepad vibration failed");
    }

    void CheckJavaException(JNIEnv* env)
    {
        if (env->ExceptionCheck())
        {
            ThrowJavaException(env);
        }
    }

    class ScopedJniEnv final
    {
    public:
        explicit ScopedJniEnv(JavaVM* vm)
            : _vm(vm)
        {
            if (_vm == nullptr)
            {
                throw std::runtime_error("Android Java VM is not available");
            }

            const jint result = _vm->GetEnv(
                reinterpret_cast<void**>(&_env), JNI_VERSION_1_6);
            if (result == JNI_EDETACHED)
            {
                if (_vm->AttachCurrentThread(&_env, nullptr) != JNI_OK)
                {
                    throw std::runtime_error(
                        "could not attach the current thread to the Android Java VM");
                }
                _attached = true;
            }
            else if (result != JNI_OK || _env == nullptr)
            {
                throw std::runtime_error(
                    "could not obtain the Android JNI environment");
            }
        }

        ~ScopedJniEnv()
        {
            if (_attached)
            {
                _vm->DetachCurrentThread();
            }
        }

        ScopedJniEnv(const ScopedJniEnv&) = delete;
        ScopedJniEnv& operator=(const ScopedJniEnv&) = delete;

        [[nodiscard]] JNIEnv* Get() const noexcept { return _env; }

    private:
        JavaVM* _vm = nullptr;
        JNIEnv* _env = nullptr;
        bool _attached = false;
    };

    template <typename Action>
    bool WithDeviceVibrator(
        JNIEnv* env,
        std::int32_t deviceId,
        bool requireHardware,
        Action&& action)
    {
        LocalRef<jclass> inputDeviceClass(
            env, env->FindClass("android/view/InputDevice"));
        CheckJavaException(env);
        if (!inputDeviceClass)
        {
            throw std::runtime_error("Android InputDevice class is unavailable");
        }

        const jmethodID getDevice = env->GetStaticMethodID(
            inputDeviceClass.Get(), "getDevice", "(I)Landroid/view/InputDevice;");
        CheckJavaException(env);
        if (getDevice == nullptr)
        {
            throw std::runtime_error("Android InputDevice.getDevice is unavailable");
        }

        LocalRef<jobject> device(env, env->CallStaticObjectMethod(
            inputDeviceClass.Get(), getDevice, static_cast<jint>(deviceId)));
        CheckJavaException(env);
        if (!device)
        {
            return false;
        }

        const jmethodID getVibrator = env->GetMethodID(
            inputDeviceClass.Get(), "getVibrator", "()Landroid/os/Vibrator;");
        CheckJavaException(env);
        if (getVibrator == nullptr)
        {
            throw std::runtime_error("Android InputDevice.getVibrator is unavailable");
        }

        LocalRef<jobject> vibrator(env, env->CallObjectMethod(device.Get(), getVibrator));
        CheckJavaException(env);
        if (!vibrator)
        {
            return false;
        }

        const jclass vibratorClass = env->FindClass("android/os/Vibrator");
        CheckJavaException(env);
        LocalRef<jclass> vibratorType(env, vibratorClass);
        if (!vibratorType)
        {
            throw std::runtime_error("Android Vibrator class is unavailable");
        }

        if (requireHardware)
        {
            const jmethodID hasVibrator = env->GetMethodID(
                vibratorType.Get(), "hasVibrator", "()Z");
            CheckJavaException(env);
            if (hasVibrator == nullptr)
            {
                throw std::runtime_error("Android Vibrator.hasVibrator is unavailable");
            }

            const jboolean available = env->CallBooleanMethod(vibrator.Get(), hasVibrator);
            CheckJavaException(env);
            if (available == JNI_FALSE)
            {
                return false;
            }
        }

        return action(vibrator.Get(), vibratorType.Get());
    }

    [[nodiscard]] std::int32_t AndroidApiLevel(JNIEnv* env)
    {
        LocalRef<jclass> versionClass(
            env, env->FindClass("android/os/Build$VERSION"));
        CheckJavaException(env);
        if (!versionClass)
        {
            throw std::runtime_error("Android Build.VERSION class is unavailable");
        }

        const jfieldID sdkInt = env->GetStaticFieldID(
            versionClass.Get(), "SDK_INT", "I");
        CheckJavaException(env);
        if (sdkInt == nullptr)
        {
            throw std::runtime_error("Android SDK_INT is unavailable");
        }

        const jint apiLevel = env->GetStaticIntField(versionClass.Get(), sdkInt);
        CheckJavaException(env);
        return static_cast<std::int32_t>(apiLevel);
    }
}

namespace MphRead::Droid
{
    AndroidGamepadHaptics::AndroidGamepadHaptics(
        JNIEnv* env,
        std::int32_t deviceId)
        : _deviceId(deviceId)
    {
        if (env == nullptr || env->GetJavaVM(&_vm) != JNI_OK)
        {
            throw std::runtime_error("could not obtain the Android Java VM");
        }
    }

    std::shared_ptr<AndroidGamepadHaptics> AndroidGamepadHaptics::Create(
        JNIEnv* env,
        std::int32_t deviceId)
    {
        std::shared_ptr<AndroidGamepadHaptics> backend(
            new AndroidGamepadHaptics(env, deviceId));
        if (!backend->Available())
        {
            return nullptr;
        }
        return backend;
    }

    bool AndroidGamepadHaptics::Available()
    {
        ScopedJniEnv scoped(_vm);
        return WithDeviceVibrator(scoped.Get(), _deviceId, true,
            [](jobject, jclass) { return true; });
    }

    void AndroidGamepadHaptics::Rumble(
        float lowFrequency,
        float highFrequency,
        std::chrono::milliseconds duration)
    {
        const std::int64_t durationMs = std::clamp<std::int64_t>(
            duration.count(), 1, 500);
        ScopedJniEnv scoped(_vm);
        JNIEnv* env = scoped.Get();
        WithDeviceVibrator(env, _deviceId, true,
            [&](jobject vibrator, jclass vibratorClass)
            {
                if (AndroidApiLevel(env) >= 26)
                {
                    const jmethodID hasAmplitudeControl = env->GetMethodID(
                        vibratorClass, "hasAmplitudeControl", "()Z");
                    CheckJavaException(env);
                    if (hasAmplitudeControl == nullptr)
                    {
                        throw std::runtime_error(
                            "Android Vibrator.hasAmplitudeControl is unavailable");
                    }

                    const jboolean supportsAmplitude = env->CallBooleanMethod(
                        vibrator, hasAmplitudeControl);
                    CheckJavaException(env);
                    std::int32_t amplitude = -1;
                    if (supportsAmplitude != JNI_FALSE)
                    {
                        amplitude = std::clamp(
                            static_cast<std::int32_t>(
                                std::max(lowFrequency, highFrequency) * 255.0F),
                            1, 255);
                    }

                    LocalRef<jclass> effectClass(
                        env, env->FindClass("android/os/VibrationEffect"));
                    CheckJavaException(env);
                    if (!effectClass)
                    {
                        throw std::runtime_error(
                            "Android VibrationEffect class is unavailable");
                    }
                    const jmethodID createOneShot = env->GetStaticMethodID(
                        effectClass.Get(), "createOneShot",
                        "(JI)Landroid/os/VibrationEffect;");
                    CheckJavaException(env);
                    if (createOneShot == nullptr)
                    {
                        throw std::runtime_error(
                            "Android VibrationEffect.createOneShot is unavailable");
                    }

                    LocalRef<jobject> effect(env, env->CallStaticObjectMethod(
                        effectClass.Get(), createOneShot,
                        static_cast<jlong>(durationMs), static_cast<jint>(amplitude)));
                    CheckJavaException(env);
                    if (!effect)
                    {
                        throw std::runtime_error(
                            "Android VibrationEffect.createOneShot returned null");
                    }

                    const jmethodID vibrate = env->GetMethodID(
                        vibratorClass, "vibrate", "(Landroid/os/VibrationEffect;)V");
                    CheckJavaException(env);
                    if (vibrate == nullptr)
                    {
                        throw std::runtime_error("Android Vibrator.vibrate is unavailable");
                    }
                    env->CallVoidMethod(vibrator, vibrate, effect.Get());
                    CheckJavaException(env);
                }
                else
                {
                    const jmethodID vibrate = env->GetMethodID(
                        vibratorClass, "vibrate", "(J)V");
                    CheckJavaException(env);
                    if (vibrate == nullptr)
                    {
                        throw std::runtime_error("Android Vibrator.vibrate is unavailable");
                    }
                    env->CallVoidMethod(vibrator, vibrate,
                        static_cast<jlong>(durationMs));
                    CheckJavaException(env);
                }
                return true;
            });
    }

    void AndroidGamepadHaptics::Stop()
    {
        ScopedJniEnv scoped(_vm);
        JNIEnv* env = scoped.Get();
        WithDeviceVibrator(env, _deviceId, false,
            [env](jobject vibrator, jclass vibratorClass)
            {
                const jmethodID cancel = env->GetMethodID(
                    vibratorClass, "cancel", "()V");
                CheckJavaException(env);
                if (cancel == nullptr)
                {
                    throw std::runtime_error("Android Vibrator.cancel is unavailable");
                }
                env->CallVoidMethod(vibrator, cancel);
                CheckJavaException(env);
                return true;
            });
    }
}
