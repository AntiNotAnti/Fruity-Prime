#pragma once

#include "GuiTheme.hpp"

#include <string>

namespace MphRead::Mods::Launcher::Gui
{
    // A flag, drawn on the same pixel grid as everything else, out of the
    // shapes flags are made of. Everything else falls back to its code.
    class Flags final
    {
    public:
        Flags() = delete;

        // 1.5em by 1.05em of a row's own em.
        static constexpr double Width = 20;
        static constexpr double Height = 14;

        [[nodiscard]] static bool Known(const std::string& code);
        static void Draw(Av::Media::DrawingContext& context, const std::string& code, double x, double y);

    private:
        static void DrawUnion(Av::Media::DrawingContext& context, double x, double y, double w, double h);
        static void DrawCode(Av::Media::DrawingContext& context, const std::string& code, double x, double y);
    };
}
