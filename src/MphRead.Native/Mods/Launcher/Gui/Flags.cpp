#include "Flags.hpp"

#include "../../../NativeRuntime/System/Globalization.hpp"
#include "../../../NativeRuntime/System/Managed.hpp"

#include <map>
#include <vector>

namespace MphRead::Mods::Launcher::Gui
{
    using namespace ::MphRead::NativeRuntime::Avalonia;

    namespace
    {
        enum class Style
        {
            V, V2, H, H2, H3, Cross, Swiss, Disc, Union, Union2, Stars, Maple, Spain, Greece, Czech, Chile
        };

        struct Flag final
        {
            Style Style;
            std::vector<std::uint32_t> Colours;
        };

        [[nodiscard]] const std::map<std::string, Flag>& Table()
        {
            static const std::map<std::string, Flag> flags{
                {"AR", {Style::H, {0x6f9ec9U, 0xe2e0d8U, 0x6f9ec9U}}},
                {"AT", {Style::H, {0x9e2b2bU, 0xe2e0d8U, 0x9e2b2bU}}},
                {"AU", {Style::Union2, {}}},
                {"BE", {Style::V, {0x16181dU, 0xc9a227U, 0x9e2b2bU}}},
                {"BR", {Style::Disc, {0x2c7a4eU, 0xc9a227U}}},
                {"CA", {Style::Maple, {}}},
                {"CH", {Style::Swiss, {0x9e2b2bU, 0xe2e0d8U}}},
                {"CL", {Style::Chile, {}}},
                {"CO", {Style::H3, {0xc9a227U, 0x22457aU, 0x9e2b2bU}}},
                {"CZ", {Style::Czech, {}}},
                {"DE", {Style::H, {0x16181dU, 0x9e2b2bU, 0xc9a227U}}},
                {"DK", {Style::Cross, {0x9e2b2bU, 0xe2e0d8U}}},
                {"ES", {Style::Spain, {}}},
                {"FI", {Style::Cross, {0xe2e0d8U, 0x22457aU}}},
                {"FR", {Style::V, {0x22457aU, 0xe2e0d8U, 0x9e2b2bU}}},
                {"GB", {Style::Union, {}}},
                {"GR", {Style::Greece, {}}},
                {"HU", {Style::H, {0x9e2b2bU, 0xe2e0d8U, 0x2c7a4eU}}},
                {"ID", {Style::H2, {0x9e2b2bU, 0xe2e0d8U}}},
                {"IE", {Style::V, {0x2c7a4eU, 0xe2e0d8U, 0xc47a33U}}},
                {"IS", {Style::Cross, {0x22457aU, 0xe2e0d8U}}},
                {"IT", {Style::V, {0x2c7a4eU, 0xe2e0d8U, 0x9e2b2bU}}},
                {"JP", {Style::Disc, {0xe2e0d8U, 0x9e2b2bU}}},
                {"MX", {Style::V, {0x1f6b45U, 0xe2e0d8U, 0x9e2b2bU}}},
                {"NL", {Style::H, {0x9e2b2bU, 0xe2e0d8U, 0x22457aU}}},
                {"NO", {Style::Cross, {0x9e2b2bU, 0x22457aU}}},
                {"NZ", {Style::Union2, {}}},
                {"PL", {Style::H2, {0xe2e0d8U, 0x9e2b2bU}}},
                {"PT", {Style::V2, {0x2c7a4eU, 0x9e2b2bU}}},
                {"RO", {Style::V, {0x22457aU, 0xc9a227U, 0x9e2b2bU}}},
                {"RU", {Style::H, {0xe2e0d8U, 0x22457aU, 0x9e2b2bU}}},
                {"SE", {Style::Cross, {0x22457aU, 0xc9a227U}}},
                {"TR", {Style::Disc, {0x9e2b2bU, 0xe2e0d8U}}},
                {"UA", {Style::H2, {0x2b6fa8U, 0xc9a227U}}},
                {"US", {Style::Stars, {}}}};
            return flags;
        }

        [[nodiscard]] Media::IBrushPtr B(std::uint32_t hex)
        {
            return std::make_shared<Media::SolidColorBrush>(Media::Color::FromRgb(static_cast<std::uint8_t>(hex >> 16),
                static_cast<std::uint8_t>(hex >> 8), static_cast<std::uint8_t>(hex)));
        }

        // Whole pixels: a flag on a half pixel is a smudge.
        void Fill(Media::DrawingContext& c, const Media::IBrushPtr& b, double x, double y, double w, double h)
        {
            c.FillRectangle(b, Rect{::MphRead::NativeRuntime::RoundToEven(x), ::MphRead::NativeRuntime::RoundToEven(y),
                ::MphRead::NativeRuntime::RoundToEven(w), ::MphRead::NativeRuntime::RoundToEven(h)});
        }

        [[nodiscard]] Media::IPenPtr Outline()
        {
            return std::make_shared<Media::Pen>(
                std::make_shared<Media::SolidColorBrush>(Media::Color::FromArgb(190, 0, 0, 0)), 1);
        }
    }

    bool Flags::Known(const std::string& code)
    {
        return Table().contains(code);
    }

    void Flags::Draw(Media::DrawingContext& context, const std::string& code, double x, double y)
    {
        const double w = Width;
        const double h = Height;
        x = ::MphRead::NativeRuntime::RoundToEven(x);
        y = ::MphRead::NativeRuntime::RoundToEven(y);
        const auto found = Table().find(code);
        if (found == Table().end())
        {
            DrawCode(context, code, x, y);
            return;
        }
        const std::vector<std::uint32_t>& c = found->second.Colours;
        switch (found->second.Style)
        {
        case Style::V:
            Fill(context, B(c[0]), x, y, w / 3, h);
            Fill(context, B(c[1]), x + w / 3, y, w / 3, h);
            Fill(context, B(c[2]), x + 2 * w / 3, y, w / 3, h);
            break;
        case Style::V2:
            Fill(context, B(c[0]), x, y, w * 0.4, h);
            Fill(context, B(c[1]), x + w * 0.4, y, w * 0.6, h);
            break;
        case Style::H:
        case Style::H3:
            Fill(context, B(c[0]), x, y, w, h / 3);
            Fill(context, B(c[1]), x, y + h / 3, w, h / 3);
            Fill(context, B(c[2]), x, y + 2 * h / 3, w, h / 3);
            break;
        case Style::H2:
            Fill(context, B(c[0]), x, y, w, h / 2);
            Fill(context, B(c[1]), x, y + h / 2, w, h / 2);
            break;
        case Style::Cross:
            Fill(context, B(c[0]), x, y, w, h);
            Fill(context, B(c[1]), x + 6, y, 4, h);
            Fill(context, B(c[1]), x, y + 6, w, 4);
            break;
        case Style::Swiss:
            Fill(context, B(c[0]), x, y, w, h);
            Fill(context, B(c[1]), x + 9, y + 3, 4, 9);
            Fill(context, B(c[1]), x + 6, y + 6, 10, 3);
            break;
        case Style::Disc:
            Fill(context, B(c[0]), x, y, w, h);
            context.DrawEllipse(B(c[1]), nullptr, Point{x + w / 2, y + h / 2}, 4, 4);
            break;
        case Style::Union:
            DrawUnion(context, x, y, w, h);
            break;
        case Style::Union2:
            Fill(context, B(0x22457aU), x, y, w, h);
            DrawUnion(context, x, y, w / 2, h / 2);
            Fill(context, B(0xe2e0d8U), x + 15, y + 4, 2, 2);
            Fill(context, B(0xe2e0d8U), x + 17, y + 9, 2, 2);
            Fill(context, B(0xe2e0d8U), x + 13, y + 11, 2, 2);
            break;
        case Style::Stars:
            Fill(context, B(0xe2e0d8U), x, y, w, h);
            for (int i = 0; i < 4; i++)
            {
                Fill(context, B(0x9e2b2bU), x, y + i * 4, w, 2);
            }
            Fill(context, B(0x22457aU), x, y, 10, 8);
            break;
        case Style::Maple:
            Fill(context, B(0xe2e0d8U), x, y, w, h);
            Fill(context, B(0x9e2b2bU), x, y, 5, h);
            Fill(context, B(0x9e2b2bU), x + 17, y, 5, h);
            Fill(context, B(0x9e2b2bU), x + 10, y + 4, 2, 8);
            Fill(context, B(0x9e2b2bU), x + 8, y + 6, 6, 3);
            break;
        case Style::Spain:
            Fill(context, B(0x9e2b2bU), x, y, w, h);
            Fill(context, B(0xc9a227U), x, y + 4, w, 7);
            break;
        case Style::Greece:
            Fill(context, B(0xe2e0d8U), x, y, w, h);
            for (int i = 0; i < 3; i++)
            {
                Fill(context, B(0x2b6fa8U), x, y + i * 4, w, 2);
            }
            Fill(context, B(0x2b6fa8U), x, y, 9, 8);
            Fill(context, B(0xe2e0d8U), x + 3, y, 3, 8);
            Fill(context, B(0xe2e0d8U), x, y + 3, 9, 2);
            break;
        case Style::Czech:
            Fill(context, B(0xe2e0d8U), x, y, w, h / 2);
            Fill(context, B(0x9e2b2bU), x, y + h / 2, w, h / 2);
            Fill(context, B(0x22457aU), x, y, 4, h);
            Fill(context, B(0x22457aU), x + 4, y + 3, 3, 9);
            break;
        case Style::Chile:
            Fill(context, B(0xe2e0d8U), x, y, w, h / 2);
            Fill(context, B(0x9e2b2bU), x, y + h / 2, w, h / 2);
            Fill(context, B(0x22457aU), x, y, 8, 8);
            break;
        }
        context.DrawRectangle(nullptr, Outline(), Rect{x + 0.5, y + 0.5, w - 1, h - 1});
    }

    void Flags::DrawUnion(Media::DrawingContext& context, double x, double y, double w, double h)
    {
        Fill(context, B(0x22457aU), x, y, w, h);
        const Media::IBrushPtr white = B(0xe2e0d8U);
        const Media::IBrushPtr red = B(0x9e2b2bU);
        Fill(context, white, x + w / 2 - 2, y, 4, h);
        Fill(context, white, x, y + h / 2 - 2, w, 4);
        Fill(context, red, x + w / 2 - 1, y, 2, h);
        Fill(context, red, x, y + h / 2 - 1, w, 2);
    }

    void Flags::DrawCode(Media::DrawingContext& context, const std::string& code, double x, double y)
    {
        Fill(context, B(0x1b2736U), x, y, Width, Height);
        if (code.size() >= 2)
        {
            const Media::FormattedText text(::MphRead::NativeRuntime::ToUpperInvariant(code), Media::InvariantCulture,
                Media::FlowDirection::LeftToRight,
                Media::Typeface(GuiTheme::PixelSemi, Media::FontStyle::Normal, Media::FontWeight::Normal), 12,
                GuiTheme::TextDimBrush);
            context.DrawText(text, Point{::MphRead::NativeRuntime::RoundToEven(x + (Width - text.Width()) / 2),
                ::MphRead::NativeRuntime::RoundToEven(y + (Height - text.Height()) / 2)});
        }
        context.DrawRectangle(nullptr, Outline(), Rect{x + 0.5, y + 0.5, Width - 1, Height - 1});
    }
}
