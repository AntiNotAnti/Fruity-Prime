#pragma once

#include "GuiTheme.hpp"
#include "../../Render/Crosshair.hpp"

namespace MphRead::Mods::Launcher::Gui
{
    // The crosshair as it will actually be drawn, in the settings row that
    // picks it: one point here is one pixel in the game.
    class CrosshairPreview final
    {
    public:
        CrosshairPreview() = delete;
        static void Draw(Av::Media::DrawingContext& context, Av::Rect area, ::MphRead::Mods::Render::CrosshairStyle style,
            ::MphRead::Mods::Render::CrosshairSize size);
    };
}
