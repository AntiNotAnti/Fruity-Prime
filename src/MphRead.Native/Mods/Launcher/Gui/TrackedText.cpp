#include "TrackedText.hpp"

#include "../../../NativeRuntime/System/Encoding.hpp"
#include "../../../NativeRuntime/System/Managed.hpp"

namespace MphRead::Mods::Launcher::Gui
{
    using namespace ::MphRead::NativeRuntime::Avalonia;
    namespace Runtime = ::MphRead::NativeRuntime;

    namespace
    {
        // foreach (char c in text): one UTF-16 code unit at a time.
        [[nodiscard]] std::u16string Chars(std::string_view text)
        {
            return Runtime::Utf8ToUtf16(text);
        }

        [[nodiscard]] std::string OneChar(char16_t value)
        {
            return Runtime::Utf16ToUtf8(std::u16string_view(&value, 1));
        }
    }

    Media::FormattedText TrackedText::Make(std::string_view text, double size, bool bold,
        const Media::IBrushPtr& brush)
    {
        return Media::FormattedText(text, Media::InvariantCulture, Media::FlowDirection::LeftToRight,
            GuiTheme::Face(bold), size, brush);
    }

    void TrackedText::Draw(Media::DrawingContext& context, std::string_view text, double size,
        const Media::IBrushPtr& brush, double x, double y, double tracking)
    {
        const double space = SpaceWidth(size);
        double pen = x;
        for (const char16_t c : Chars(text))
        {
            if (c == u' ')
            {
                pen += space + tracking;
                continue;
            }
            const Media::FormattedText glyph = Make(OneChar(c), size, true, brush);
            context.DrawText(glyph, Point{pen, y});
            pen += glyph.Width() + tracking;
        }
    }

    double TrackedText::Measure(std::string_view text, double size, double tracking)
    {
        const double space = SpaceWidth(size);
        double width = 0;
        for (const char16_t c : Chars(text))
        {
            width += (c == u' ' ? space
                                : Make(OneChar(c), size, true, GuiTheme::TextBrush).Width())
                + tracking;
        }
        return width;
    }

    double TrackedText::LineHeight(double size)
    {
        return Make("X", size, true, GuiTheme::TextBrush).Height();
    }

    double TrackedText::SpaceWidth(double size)
    {
        const double pair = Make("nn", size, true, GuiTheme::TextBrush).Width();
        const double spaced = Make("n n", size, true, GuiTheme::TextBrush).Width();
        return Runtime::MathMax(spaced - pair, size * 0.22);
    }
}
