#pragma once

#include "../../Renderer.hpp"

namespace MphRead::Mods::Render
{
    class AppIcon final
    {
    public:
        AppIcon() = delete;

        [[nodiscard]] static const ::MphRead::RendererPlatform::WindowIcon* Load();

    private:
        [[nodiscard]] static ::MphRead::RendererPlatform::WindowIconImage Scaled(
            const std::vector<std::uint8_t>& source, std::int32_t width, std::int32_t height,
            std::int32_t size);

        static std::optional<::MphRead::RendererPlatform::WindowIcon> _icon;
        static bool _tried;
    };
}
