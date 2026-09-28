#include "DeckText.hpp"

#include "../../../NativeRuntime/System/Encoding.hpp"
#include "../../../NativeRuntime/System/Managed.hpp"

#include <cmath>
#include <map>
#include <numbers>
#include <string>

namespace MphRead::Mods::Launcher::Gui
{
    using namespace ::MphRead::NativeRuntime::Avalonia;

    namespace
    {
        namespace Runtime = ::MphRead::NativeRuntime;

        struct Key final
        {
            std::string Text;
            double Size;
            std::int32_t Weight;
            std::string Family;
            std::uint32_t Colour;
            double Width;
        };

        [[nodiscard]] int CompareDouble(double left, double right) noexcept
        {
            if (std::isnan(left))
            {
                return std::isnan(right) ? 0 : -1;
            }
            if (std::isnan(right))
            {
                return 1;
            }
            return left < right ? -1 : left > right ? 1 : 0;
        }

        struct KeyLess final
        {
            [[nodiscard]] bool operator()(const Key& left, const Key& right) const
            {
                if (left.Text != right.Text)
                {
                    return left.Text < right.Text;
                }
                if (const int compare = CompareDouble(left.Size, right.Size); compare != 0)
                {
                    return compare < 0;
                }
                if (left.Weight != right.Weight)
                {
                    return left.Weight < right.Weight;
                }
                if (left.Family != right.Family)
                {
                    return left.Family < right.Family;
                }
                if (left.Colour != right.Colour)
                {
                    return left.Colour < right.Colour;
                }
                return CompareDouble(left.Width, right.Width) < 0;
            }
        };

        [[nodiscard]] std::u16string Chars(std::string_view text)
        {
            return Runtime::Utf8ToUtf16(text);
        }

        [[nodiscard]] std::string OneChar(char16_t value)
        {
            return Runtime::Utf16ToUtf8(std::u16string_view(&value, 1));
        }

        [[nodiscard]] std::map<Key, std::shared_ptr<Media::FormattedText>, KeyLess>& Cache()
        {
            static std::map<Key, std::shared_ptr<Media::FormattedText>, KeyLess> cache;
            return cache;
        }
    }

    std::shared_ptr<Media::FormattedText> DeckText::Lay(std::string_view text, const Media::Typeface& face, double size,
        const Media::IBrushPtr& brush, bool display, double maxWidth)
    {
        (void)display;
        const auto* solid = dynamic_cast<const Media::SolidColorBrush*>(brush.get());
        const std::uint32_t colour = solid != nullptr ? solid->Color.ToUInt32() : 0U;
        // The weight and the family are both part of the key; the three
        // Pixelify files share a name, so the family is keyed by its source.
        const Key key{std::string(text), size, static_cast<std::int32_t>(face.Weight),
            face.FontFamily != nullptr ? face.FontFamily->Name() : std::string(), colour, maxWidth};
        auto& cache = Cache();
        const auto found = cache.find(key);
        if (found != cache.end())
        {
            return found->second;
        }
        if (cache.size() > 1024)
        {
            cache.clear();
        }
        auto laid = std::make_shared<Media::FormattedText>(text, Media::InvariantCulture,
            Media::FlowDirection::LeftToRight, face, size, brush);
        if (maxWidth > 0)
        {
            laid->MaxTextWidth(maxWidth);
            // One line, whatever the trimming decides: without a height limit
            // the text wraps at the first space rather than ellipsizing.
            laid->MaxTextHeight(size * 1.9);
            laid->Trimming(Media::TextTrimming::CharacterEllipsis);
        }
        cache[key] = laid;
        return laid;
    }

    std::shared_ptr<Media::FormattedText> DeckText::Run(std::string_view text, const Media::Typeface& face, double size,
        const Media::IBrushPtr& brush, double maxWidth, bool display)
    {
        return Lay(text, face, size, brush, display, maxWidth);
    }

    double DeckText::MeasureTracked(std::string_view text, const Media::Typeface& face, double size, double trackingEms)
    {
        if (text.empty())
        {
            return 0;
        }
        const double tracking = size * trackingEms;
        double pen = 0;
        for (const char16_t c : Chars(text))
        {
            pen += (c == u' ' ? size * SpaceEm
                              : Lay(OneChar(c), face, size, Media::Brushes::White(), true, 0)
                                    ->Width())
                + tracking;
        }
        return pen;
    }

    double DeckText::DrawTracked(Media::DrawingContext& context, std::string_view text, const Media::Typeface& face,
        double size, const Media::IBrushPtr& brush, double x, double top, double height, double trackingEms,
        const std::function<std::pair<double, double>(std::int32_t)>& hop)
    {
        if (text.empty())
        {
            return 0;
        }
        const double tracking = size * trackingEms;
        double pen = x;
        std::int32_t index = 0;
        for (const char16_t c : Chars(text))
        {
            if (c == u' ')
            {
                pen += size * SpaceEm + tracking;
                continue;
            }
            const std::shared_ptr<Media::FormattedText> glyph
                = Lay(OneChar(c), face, size, brush, true, 0);
            const double gx = Runtime::RoundToEven(pen);
            const double gy = Runtime::RoundToEven(top + (height - glyph->Height()) / 2);
            const auto [lift, degrees] = hop ? hop(index) : std::pair<double, double>{0.0, 0.0};
            index++;
            if (lift == 0 && degrees == 0)
            {
                context.DrawText(*glyph, Point{gx, gy});
                pen += glyph->Width() + tracking;
                continue;
            }
            // Rotated about the character's own middle, where a CSS
            // transform's origin is by default.
            const double cx = gx + glyph->Width() / 2;
            const double cy = gy + glyph->Height() / 2;
            {
                auto state = context.PushTransform(Matrix::CreateTranslation(-cx, -cy)
                    * Matrix::CreateRotation(degrees * std::numbers::pi / 180)
                    * Matrix::CreateTranslation(cx, cy + lift));
                context.DrawText(*glyph, Point{gx, gy});
            }
            pen += glyph->Width() + tracking;
        }
        return pen - x;
    }

    void DeckText::Forget()
    {
        Cache().clear();
    }
}
