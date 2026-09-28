#pragma once

#include <cstdint>

namespace MphRead::Mods::Render
{
    // Texture names for the scene's own render targets, chosen rather than
    // asked for. The engine counts its textures (Scene::_textureCount) and
    // binds the number, so a name from glGenTextures is one some scene will
    // count onto: with the launcher's side scene in the session, the match's
    // screen texture took a name the HUD later counted onto, and the
    // scoreboard's portraits came out as the frame upside down, or black.
    // These live above the counter and above UiOverlay's own chosen name.
    class GlNames final
    {
    public:
        GlNames() = delete;

        static std::int32_t NextTexture() noexcept;
    };
}
