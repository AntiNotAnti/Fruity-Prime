#include "ServerBadge.hpp"

#include "GeoCountry.hpp"
#include "../../../NativeRuntime/System/Net.hpp"
#include "../../../NativeRuntime/System/Managed.hpp"

#include <algorithm>
#include <cstdint>
#include <memory>
#include <optional>
#include <string_view>
#include <vector>

namespace MphRead::Mods::Launcher::Gui
{
    namespace Runtime = ::MphRead::NativeRuntime;
    using namespace Runtime::Avalonia;

    namespace
    {
        [[nodiscard]] std::string_view HostOf(std::string_view endpoint) noexcept
        {
            const std::size_t colon = endpoint.rfind(':');
            return colon != std::string_view::npos && colon > 0
                ? endpoint.substr(0, colon) : endpoint;
        }

        [[nodiscard]] Media::IBrushPtr Solid(Media::Color color)
        {
            return std::make_shared<Media::SolidColorBrush>(color);
        }

        [[nodiscard]] Media::Color Rgb(std::uint32_t color) noexcept
        {
            return Media::Color::FromRgb(static_cast<std::uint8_t>(color >> 16),
                static_cast<std::uint8_t>(color >> 8), static_cast<std::uint8_t>(color));
        }

        [[nodiscard]] Media::IPenPtr Outline()
        {
            return std::make_shared<Media::Pen>(Solid(Media::Color::FromArgb(190, 0, 0, 0)), 1);
        }
    }

    std::string ServerBadge::CountryOf(const std::string& endpoint)
    {
        const std::optional<Runtime::IPAddressValue> parsed = Runtime::IPAddressTryParse(HostOf(endpoint));
        if (!parsed.has_value() || parsed->Family != Runtime::IPAddressFamily::InterNetwork)
        {
            return {};
        }
        Runtime::Address address;
        address.Family = Runtime::AddressFamily::InterNetwork;
        std::copy_n(parsed->Bytes.begin(), address.Bytes.size(), address.Bytes.begin());
        return GeoCountry::Of(std::optional<Runtime::Address>(address));
    }

    ServerBadge::Kind ServerBadge::Of(const std::string& endpoint, bool answered)
    {
        if (!answered)
        {
            return Kind::Silent;
        }
        const std::optional<Runtime::IPAddressValue> parsed = Runtime::IPAddressTryParse(HostOf(endpoint));
        if (!parsed.has_value())
        {
            return Kind::Internet;
        }
        if (Runtime::IPAddressIsLoopback(*parsed))
        {
            return Kind::Local;
        }
        const std::vector<std::uint8_t> bytes = Runtime::IPAddressGetAddressBytes(*parsed);
        if (bytes.size() == 4U && (bytes[0] == 10
            || (bytes[0] == 192 && bytes[1] == 168)
            || (bytes[0] == 172 && bytes[1] >= 16 && bytes[1] <= 31)
            || (bytes[0] == 169 && bytes[1] == 254)))
        {
            return Kind::Lan;
        }
        return Kind::Internet;
    }

    void ServerBadge::Draw(Media::DrawingContext& context, const std::string& endpoint,
        bool answered, double x, double y)
    {
        const Kind kind = Of(endpoint, answered);
        if (kind == Kind::Internet)
        {
            const std::string code = CountryOf(endpoint);
            if (code.size() == 2U)
            {
                Flags::Draw(context, code, x, y);
                return;
            }
        }
        Draw(context, kind, x, y);
    }

    void ServerBadge::Draw(Media::DrawingContext& context, Kind kind, double x, double y)
    {
        const Rect frame(Runtime::RoundToEven(x), Runtime::RoundToEven(y), Width, Height);
        const Media::Color ground = kind == Kind::Lan ? Media::Color::FromRgb(0x23, 0x2a, 0x36)
            : kind == Kind::Local ? Media::Color::FromRgb(0x1d, 0x2b, 0x27)
            : kind == Kind::Silent ? Media::Color::FromRgb(0x1a, 0x1f, 0x29)
            : Media::Color::FromRgb(0x1b, 0x27, 0x36);
        context.FillRectangle(Solid(ground), frame, 2);

        const double cx = frame.X;
        const double cy = frame.Y;
        const Media::Color inkColor = kind == Kind::Lan ? Media::Color::FromRgb(0x2c, 0x5a, 0x4e)
            : kind == Kind::Local ? Media::Color::FromRgb(0x5f, 0x9e, 0x72)
            : kind == Kind::Silent ? Media::Color::FromRgb(0x3a, 0x43, 0x53)
            : Media::Color::FromRgb(0x3f, 0x7f, 0xa8);
        const Media::IBrushPtr ink = Solid(inkColor);

        if (kind == Kind::Silent)
        {
            context.FillRectangle(ink, Rect(cx + 4, cy + 7, 14, 2));
            return;
        }
        if (kind == Kind::Lan || kind == Kind::Local)
        {
            const Media::IBrushPtr stand = Solid(Media::Color::FromRgb(0x8a, 0x93, 0xa6));
            context.FillRectangle(ink, Rect(cx + 4, cy + 3, 14, 7));
            context.FillRectangle(stand, Rect(cx + 10, cy + 10, 2, 2));
            context.FillRectangle(stand, Rect(cx + 7, cy + 12, 8, 2));
            return;
        }
        context.FillRectangle(ink, Rect(cx + 4, cy + 3, 14, 9));
        const Media::IBrushPtr pale = Solid(Media::Color::FromRgb(0x8d, 0xc4, 0xe8));
        context.FillRectangle(pale, Rect(cx + 4, cy + 6, 14, 1));
        context.FillRectangle(pale, Rect(cx + 10, cy + 3, 2, 9));
        context.FillRectangle(pale, Rect(cx + 6, cy + 4, 1, 7));
        context.FillRectangle(pale, Rect(cx + 15, cy + 4, 1, 7));
    }
}
