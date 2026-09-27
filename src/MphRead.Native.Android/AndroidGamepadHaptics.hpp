#pragma once

#include "../MphRead.Native/Mods/Input/GamepadHaptics.hpp"

#include <jni.h>

#include <chrono>
#include <cstdint>
#include <memory>

namespace MphRead::Droid
{
    class AndroidGamepadHaptics final
        : public MphRead::Mods::Input::IGamepadHaptics
    {
    public:
        [[nodiscard]] static std::shared_ptr<AndroidGamepadHaptics> Create(
            JNIEnv* env,
            std::int32_t deviceId);

        [[nodiscard]] bool Available();
        void Rumble(
            float lowFrequency,
            float highFrequency,
            std::chrono::milliseconds duration) override;
        void Stop() override;

    private:
        AndroidGamepadHaptics(JNIEnv* env, std::int32_t deviceId);

        JavaVM* _vm = nullptr;
        std::int32_t _deviceId = 0;
    };
}
