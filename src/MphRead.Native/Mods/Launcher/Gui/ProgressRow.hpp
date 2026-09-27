#pragma once

#include "GuiTheme.hpp"

#include <string>

namespace MphRead::Mods::Launcher::Gui
{
    // A bar, what is happening, and a percentage.
    class ProgressRow final : public Av::Controls::Control
    {
    public:
        ProgressRow();
        void Set(double fraction, std::string stage);
        void Render(Av::Media::DrawingContext& context) override;

    private:
        double _fraction = 0;
        std::string _stage;
    };
}
