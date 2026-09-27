#pragma once

#if !defined(__ANDROID__)
#error "AndroidUiOverlay is only valid for the Android native target."
#endif

#include <cstdint>
#include <span>

namespace MphRead::Droid
{
    class AndroidUiOverlay final
    {
    public:
        static void Visible(bool value) noexcept;
        [[nodiscard]] static bool Visible() noexcept;

        static void Upload(
            std::span<const std::uint8_t> pixels,
            std::int32_t width,
            std::int32_t height
        );
        static void Draw(std::int32_t width, std::int32_t height);
        static void Release();

    private:
        static bool Ensure();
        static std::int32_t Link();
        static std::int32_t Compile(
            std::int32_t type,
            const char* source
        );

        static constexpr std::int32_t CullFace = 0x0B44;

        static std::int32_t _program;
        static std::int32_t _vao;
        static std::int32_t _buffer;
        static std::int32_t _texture;
        static std::int32_t _width;
        static std::int32_t _height;
        static bool _hasFrame;
        static bool _failed;
        static bool _visible;
    };
}
