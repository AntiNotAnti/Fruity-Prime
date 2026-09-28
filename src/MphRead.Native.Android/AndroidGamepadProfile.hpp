#pragma once

#include <jni.h>

#include <cstdint>
#include <optional>

namespace MphRead::Droid
{
    class AndroidGamepadProfile final
    {
    public:
        [[nodiscard]] static std::optional<AndroidGamepadProfile> ForDevice(
            JNIEnv* env,
            std::int32_t deviceId);

        [[nodiscard]] bool HasLeftStick() const noexcept;
        [[nodiscard]] const std::optional<std::int32_t>& RightX() const noexcept;
        [[nodiscard]] const std::optional<std::int32_t>& RightY() const noexcept;
        [[nodiscard]] const std::optional<std::int32_t>& LeftTrigger() const noexcept;
        [[nodiscard]] const std::optional<std::int32_t>& RightTrigger() const noexcept;

        [[nodiscard]] static bool Read(
            JNIEnv* env,
            jobject event,
            jmethodID getAxisValue,
            const std::optional<std::int32_t>& axis,
            float& value);

    private:
        bool _hasLeftStick = false;
        std::optional<std::int32_t> _rightX{};
        std::optional<std::int32_t> _rightY{};
        std::optional<std::int32_t> _leftTrigger{};
        std::optional<std::int32_t> _rightTrigger{};
    };
}
