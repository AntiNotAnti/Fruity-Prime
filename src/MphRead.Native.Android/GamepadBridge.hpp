#pragma once

#include <cstdint>
#include <functional>
#include <jni.h>

namespace MphRead::Droid
{
    class GamepadBridge final
    {
    public:
        GamepadBridge() = delete;
        GamepadBridge(const GamepadBridge&) = delete;
        GamepadBridge& operator=(const GamepadBridge&) = delete;
        GamepadBridge(GamepadBridge&&) = delete;
        GamepadBridge& operator=(GamepadBridge&&) = delete;
        ~GamepadBridge() = delete;

        static void Start(JNIEnv* env, const std::function<void()>& registerListener);
        static void Stop();
        static void Clear();
        static void DeviceAdded(JNIEnv* env, std::int32_t deviceId);
        static void DeviceChanged(JNIEnv* env, std::int32_t deviceId);
        static void DeviceRemoved(std::int32_t deviceId);

        [[nodiscard]] static bool HandleKey(
            std::int32_t keyCode,
            jobject event,
            bool down,
            JNIEnv* env
        );

        [[nodiscard]] static bool HandleMotion(
            jobject event,
            JNIEnv* env
        );
    };
}
