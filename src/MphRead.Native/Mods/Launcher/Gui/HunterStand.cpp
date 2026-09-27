#include "HunterStand.hpp"

#include "../../DebugLog.hpp"
#include "../../HunterSuits.hpp"
#include "../../Render/HunterShot.hpp"
#include "GuiTheme.hpp"
#include "../../../NativeRuntime/Avalonia/Threading.hpp"
#include "../../../NativeRuntime/System/Globalization.hpp"
#include "../../../NativeRuntime/System/Managed.hpp"
#include "../../../NativeRuntime/System/Sort.hpp"
#include "../../../NativeRuntime/System/Tasks.hpp"
#include "../../../Scene.hpp"

#if defined(MPHREAD_SHELL)
#include "../../Render/LauncherHunter.hpp"
#include "UiSurface.hpp"
#endif

#include <algorithm>
#include <array>
#include <bit>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <exception>
#include <future>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace MphRead::Mods::Launcher::Gui
{
    using namespace ::MphRead::NativeRuntime::Avalonia;
    using ::MphRead::NativeRuntime::RoundToEven;

    namespace
    {
        constexpr std::int32_t SwapGrace = 15;
        constexpr std::int32_t ShotEdge = 512;

        [[nodiscard]] std::string_view Trim(std::string_view value) noexcept
        {
            while (!value.empty() && (value.front() == ' ' || value.front() == '\t'
                || value.front() == '\r' || value.front() == '\n' || value.front() == '\f'
                || value.front() == '\v'))
            {
                value.remove_prefix(1);
            }
            while (!value.empty() && (value.back() == ' ' || value.back() == '\t'
                || value.back() == '\r' || value.back() == '\n' || value.back() == '\f'
                || value.back() == '\v'))
            {
                value.remove_suffix(1);
            }
            return value;
        }

        [[nodiscard]] std::optional<::MphRead::Hunter> TryParseHunter(std::string_view value) noexcept
        {
            using ::MphRead::Hunter;
            value = Trim(value);
            static constexpr std::array<std::pair<std::string_view, Hunter>, 9> names{{
                {"Samus", Hunter::Samus}, {"Kanden", Hunter::Kanden}, {"Trace", Hunter::Trace},
                {"Sylux", Hunter::Sylux}, {"Noxus", Hunter::Noxus}, {"Spire", Hunter::Spire},
                {"Weavel", Hunter::Weavel}, {"Guardian", Hunter::Guardian}, {"Random", Hunter::Random}
            }};
            for (const auto& [name, hunter] : names)
            {
                if (::MphRead::NativeRuntime::StringEqualsOrdinalIgnoreCase(value, name))
                {
                    return hunter;
                }
            }
            if (value.empty())
            {
                return std::nullopt;
            }
            bool positive = value.front() == '+';
            if (positive)
            {
                value.remove_prefix(1);
            }
            if (value.empty())
            {
                return std::nullopt;
            }
            std::uint32_t numeric = 0;
            const auto [end, error] = std::from_chars(value.data(), value.data() + value.size(), numeric);
            if (error != std::errc{} || end != value.data() + value.size()
                || numeric > std::numeric_limits<std::uint8_t>::max())
            {
                return std::nullopt;
            }
            return static_cast<Hunter>(numeric);
        }

        [[nodiscard]] std::string Number(double value, std::int32_t decimals)
        {
            std::uint64_t factor = 1;
            for (std::int32_t i = 0; i < decimals; ++i)
            {
                factor *= 10;
            }
            const std::int64_t scaled = static_cast<std::int64_t>(RoundToEven(value * factor));
            const std::int64_t whole = scaled / static_cast<std::int64_t>(factor);
            std::int64_t fraction = scaled % static_cast<std::int64_t>(factor);
            if (fraction < 0)
            {
                fraction = -fraction;
            }
            if (fraction == 0 || decimals == 0)
            {
                return std::to_string(whole);
            }
            std::string tail = std::to_string(fraction);
            while (tail.size() < static_cast<std::size_t>(decimals))
            {
                tail.insert(tail.begin(), '0');
            }
            while (!tail.empty() && tail.back() == '0')
            {
                tail.pop_back();
            }
            return std::to_string(whole) + "." + tail;
        }

        [[nodiscard]] std::string HunterHoleMessage(double left, double top, double right, double bottom,
            double width, double height, double scale)
        {
            auto rounded = [](double value)
            {
                return std::to_string(static_cast<std::int32_t>(RoundToEven(value)));
            };
            return "hunter hole refused: (" + rounded(left) + "," + rounded(top) + ")-("
                + rounded(right) + "," + rounded(bottom) + ") in " + rounded(width) + "x"
                + rounded(height) + " at " + Number(scale, 3) + "x";
        }

    }

    std::array<std::string, 7> HunterStand::Names{
        "Samus", "Kanden", "Trace", "Sylux", "Noxus", "Spire", "Weavel"
    };

    ::MphRead::NativeRuntime::Avalonia::Media::Color HunterStand::Rgb(std::uint32_t hex) noexcept
    {
        return Media::Color::FromRgb(static_cast<std::uint8_t>(hex >> 16),
            static_cast<std::uint8_t>(hex >> 8), static_cast<std::uint8_t>(hex));
    }

    const std::unordered_map<std::string, HunterStand::Hunter>& HunterStand::HunterData()
    {
        static const std::unordered_map<std::string, Hunter> hunters = []
        {
            return std::unordered_map<std::string, Hunter>{
                {"Samus", Hunter{Rgb(0xb4632c), {
                    Box{0.0F, 46.0F, 0.0F, 26.0F, 26.0F, 20.0F, Rgb(0xc4702f)},
                    Box{0.0F, 66.0F, 0.0F, 15.0F, 13.0F, 15.0F, Rgb(0xd8862f)},
                    Box{0.0F, 70.0F, 7.0F, 11.0F, 6.0F, 5.0F, Rgb(0x2f6f8f)},
                    Box{-17.0F, 50.0F, 0.0F, 10.0F, 14.0F, 14.0F, Rgb(0xc4702f)},
                    Box{17.0F, 50.0F, 0.0F, 12.0F, 15.0F, 15.0F, Rgb(0xe08b34)},
                    Box{17.0F, 36.0F, 2.0F, 11.0F, 16.0F, 13.0F, Rgb(0xd8862f)},
                    Box{-15.0F, 34.0F, 0.0F, 8.0F, 16.0F, 8.0F, Rgb(0xa85a28)},
                    Box{-7.0F, 18.0F, 0.0F, 11.0F, 26.0F, 11.0F, Rgb(0x8e4d24)},
                    Box{7.0F, 18.0F, 0.0F, 11.0F, 26.0F, 11.0F, Rgb(0x8e4d24)},
                    Box{-7.0F, 3.0F, 2.0F, 12.0F, 7.0F, 14.0F, Rgb(0x6f3d1e)},
                    Box{7.0F, 3.0F, 2.0F, 12.0F, 7.0F, 14.0F, Rgb(0x6f3d1e)}
                }}},
                {"Kanden", Hunter{Rgb(0x4c8f5a), {
                    Box{0.0F, 48.0F, 0.0F, 22.0F, 28.0F, 17.0F, Rgb(0x4c8f5a)},
                    Box{0.0F, 68.0F, 0.0F, 13.0F, 12.0F, 13.0F, Rgb(0x5da76b)},
                    Box{0.0F, 71.0F, 6.0F, 10.0F, 5.0F, 4.0F, Rgb(0xc9d94a)},
                    Box{0.0F, 78.0F, -6.0F, 8.0F, 7.0F, 10.0F, Rgb(0x3f7a4c)},
                    Box{0.0F, 84.0F, -14.0F, 7.0F, 6.0F, 10.0F, Rgb(0x3f7a4c)},
                    Box{0.0F, 86.0F, -23.0F, 6.0F, 5.0F, 9.0F, Rgb(0x357044)},
                    Box{-15.0F, 50.0F, 0.0F, 8.0F, 13.0F, 11.0F, Rgb(0x4c8f5a)},
                    Box{15.0F, 50.0F, 0.0F, 8.0F, 13.0F, 11.0F, Rgb(0x4c8f5a)},
                    Box{-14.0F, 33.0F, 0.0F, 7.0F, 18.0F, 7.0F, Rgb(0x3f7a4c)},
                    Box{14.0F, 33.0F, 0.0F, 7.0F, 18.0F, 7.0F, Rgb(0x3f7a4c)},
                    Box{-6.0F, 17.0F, 0.0F, 9.0F, 26.0F, 9.0F, Rgb(0x357044)},
                    Box{6.0F, 17.0F, 0.0F, 9.0F, 26.0F, 9.0F, Rgb(0x357044)},
                    Box{-6.0F, 3.0F, 2.0F, 10.0F, 7.0F, 13.0F, Rgb(0x2a5a37)},
                    Box{6.0F, 3.0F, 2.0F, 10.0F, 7.0F, 13.0F, Rgb(0x2a5a37)}
                }}},
                {"Trace", Hunter{Rgb(0x9a8a4e), {
                    Box{0.0F, 52.0F, 0.0F, 19.0F, 30.0F, 15.0F, Rgb(0x9a8a4e)},
                    Box{0.0F, 73.0F, 0.0F, 12.0F, 12.0F, 13.0F, Rgb(0xab9a58)},
                    Box{0.0F, 76.0F, 6.0F, 9.0F, 4.0F, 4.0F, Rgb(0xc94a4a)},
                    Box{-14.0F, 55.0F, 0.0F, 7.0F, 12.0F, 10.0F, Rgb(0x9a8a4e)},
                    Box{14.0F, 55.0F, 0.0F, 7.0F, 12.0F, 10.0F, Rgb(0x9a8a4e)},
                    Box{-13.0F, 38.0F, 0.0F, 6.0F, 20.0F, 6.0F, Rgb(0x8a7b45)},
                    Box{13.0F, 36.0F, 6.0F, 6.0F, 22.0F, 20.0F, Rgb(0x7a6d3d)},
                    Box{-6.0F, 18.0F, 0.0F, 9.0F, 30.0F, 9.0F, Rgb(0x8a7b45)},
                    Box{6.0F, 18.0F, 0.0F, 9.0F, 30.0F, 9.0F, Rgb(0x8a7b45)},
                    Box{-6.0F, 3.0F, 2.0F, 10.0F, 7.0F, 13.0F, Rgb(0x6b6033)},
                    Box{6.0F, 3.0F, 2.0F, 10.0F, 7.0F, 13.0F, Rgb(0x6b6033)}
                }}},
                {"Sylux", Hunter{Rgb(0x3f7fa8), {
                    Box{0.0F, 48.0F, 0.0F, 21.0F, 27.0F, 16.0F, Rgb(0x3f7fa8)},
                    Box{0.0F, 67.0F, 0.0F, 13.0F, 12.0F, 13.0F, Rgb(0x4a8fbc)},
                    Box{0.0F, 70.0F, 6.0F, 10.0F, 5.0F, 4.0F, Rgb(0x6fe0c8)},
                    Box{-16.0F, 51.0F, 0.0F, 9.0F, 13.0F, 12.0F, Rgb(0x3f7fa8)},
                    Box{16.0F, 51.0F, 0.0F, 9.0F, 13.0F, 12.0F, Rgb(0x3f7fa8)},
                    Box{-15.0F, 35.0F, 3.0F, 8.0F, 17.0F, 14.0F, Rgb(0x356f94)},
                    Box{15.0F, 35.0F, 3.0F, 8.0F, 17.0F, 14.0F, Rgb(0x356f94)},
                    Box{-6.0F, 17.0F, 0.0F, 10.0F, 26.0F, 10.0F, Rgb(0x2d6182)},
                    Box{6.0F, 17.0F, 0.0F, 10.0F, 26.0F, 10.0F, Rgb(0x2d6182)},
                    Box{-6.0F, 3.0F, 2.0F, 11.0F, 7.0F, 13.0F, Rgb(0x24506c)},
                    Box{6.0F, 3.0F, 2.0F, 11.0F, 7.0F, 13.0F, Rgb(0x24506c)}
                }}},
                {"Noxus", Hunter{Rgb(0x6f86b8), {
                    Box{0.0F, 42.0F, 0.0F, 24.0F, 24.0F, 18.0F, Rgb(0x8c9fc9)},
                    Box{0.0F, 60.0F, 0.0F, 14.0F, 12.0F, 14.0F, Rgb(0xa8b8d8)},
                    Box{0.0F, 63.0F, 6.0F, 10.0F, 5.0F, 4.0F, Rgb(0x3f5f8f)},
                    Box{0.0F, 46.0F, -13.0F, 30.0F, 26.0F, 6.0F, Rgb(0x6f86b8)},
                    Box{-17.0F, 44.0F, 0.0F, 9.0F, 12.0F, 12.0F, Rgb(0x8c9fc9)},
                    Box{17.0F, 44.0F, 0.0F, 9.0F, 12.0F, 12.0F, Rgb(0x8c9fc9)},
                    Box{-15.0F, 30.0F, 0.0F, 8.0F, 14.0F, 8.0F, Rgb(0x7a8cbe)},
                    Box{15.0F, 30.0F, 0.0F, 8.0F, 14.0F, 8.0F, Rgb(0x7a8cbe)},
                    Box{-7.0F, 15.0F, 0.0F, 11.0F, 22.0F, 11.0F, Rgb(0x6f86b8)},
                    Box{7.0F, 15.0F, 0.0F, 11.0F, 22.0F, 11.0F, Rgb(0x6f86b8)},
                    Box{-7.0F, 3.0F, 2.0F, 12.0F, 7.0F, 14.0F, Rgb(0x5a6f9e)},
                    Box{7.0F, 3.0F, 2.0F, 12.0F, 7.0F, 14.0F, Rgb(0x5a6f9e)}
                }}},
                {"Spire", Hunter{Rgb(0xa85f2e), {
                    Box{0.0F, 50.0F, 0.0F, 30.0F, 30.0F, 22.0F, Rgb(0xa85f2e)},
                    Box{0.0F, 71.0F, 0.0F, 16.0F, 14.0F, 15.0F, Rgb(0xbb6f35)},
                    Box{0.0F, 74.0F, 7.0F, 11.0F, 5.0F, 4.0F, Rgb(0xe0a43c)},
                    Box{-21.0F, 54.0F, 0.0F, 13.0F, 16.0F, 16.0F, Rgb(0x96542a)},
                    Box{21.0F, 54.0F, 0.0F, 13.0F, 16.0F, 16.0F, Rgb(0x96542a)},
                    Box{-20.0F, 36.0F, 0.0F, 11.0F, 18.0F, 11.0F, Rgb(0x8a4d26)},
                    Box{20.0F, 36.0F, 0.0F, 11.0F, 18.0F, 11.0F, Rgb(0x8a4d26)},
                    Box{-9.0F, 17.0F, 0.0F, 13.0F, 26.0F, 13.0F, Rgb(0x7c4522)},
                    Box{9.0F, 17.0F, 0.0F, 13.0F, 26.0F, 13.0F, Rgb(0x7c4522)},
                    Box{-9.0F, 3.0F, 2.0F, 14.0F, 7.0F, 15.0F, Rgb(0x63371b)},
                    Box{9.0F, 3.0F, 2.0F, 14.0F, 7.0F, 15.0F, Rgb(0x63371b)}
                }}},
                {"Weavel", Hunter{Rgb(0x8a7f96), {
                    Box{0.0F, 56.0F, 0.0F, 24.0F, 20.0F, 17.0F, Rgb(0x8a7f96)},
                    Box{0.0F, 44.0F, 0.0F, 10.0F, 10.0F, 9.0F, Rgb(0x5f5769)},
                    Box{0.0F, 71.0F, 0.0F, 13.0F, 12.0F, 13.0F, Rgb(0x9a8ea8)},
                    Box{0.0F, 74.0F, 6.0F, 10.0F, 5.0F, 4.0F, Rgb(0xc94a4a)},
                    Box{-17.0F, 58.0F, 0.0F, 10.0F, 13.0F, 12.0F, Rgb(0x8a7f96)},
                    Box{17.0F, 58.0F, 0.0F, 10.0F, 13.0F, 12.0F, Rgb(0x8a7f96)},
                    Box{-16.0F, 42.0F, 0.0F, 8.0F, 18.0F, 8.0F, Rgb(0x776c85)},
                    Box{16.0F, 42.0F, 2.0F, 9.0F, 18.0F, 13.0F, Rgb(0x776c85)},
                    Box{-7.0F, 32.0F, 0.0F, 11.0F, 16.0F, 11.0F, Rgb(0x6b6178)},
                    Box{7.0F, 32.0F, 0.0F, 11.0F, 16.0F, 11.0F, Rgb(0x6b6178)},
                    Box{-7.0F, 15.0F, 2.0F, 10.0F, 20.0F, 12.0F, Rgb(0x5f5769)},
                    Box{7.0F, 15.0F, 2.0F, 10.0F, 20.0F, 12.0F, Rgb(0x5f5769)},
                    Box{-7.0F, 3.0F, 4.0F, 12.0F, 7.0F, 16.0F, Rgb(0x4d4659)},
                    Box{7.0F, 3.0F, 4.0F, 12.0F, 7.0F, 16.0F, Rgb(0x4d4659)}
                }}}
            };
        }();
        return hunters;
    }

    const std::array<std::array<std::int32_t, 4>, 6>& HunterStand::Faces() noexcept
    {
        static const std::array<std::array<std::int32_t, 4>, 6> faces{{
            {{0, 1, 2, 3}}, {{5, 4, 7, 6}}, {{4, 0, 3, 7}}, {{1, 5, 6, 2}}, {{4, 5, 1, 0}}, {{3, 2, 6, 7}}
        }};
        return faces;
    }

    const std::array<std::array<double, 3>, 6>& HunterStand::Normals() noexcept
    {
        static const std::array<std::array<double, 3>, 6> normals{{
            {{0, 0, 1}}, {{0, 0, -1}}, {{-1, 0, 0}}, {{1, 0, 0}}, {{0, 1, 0}}, {{0, -1, 0}}
        }};
        return normals;
    }

    const std::array<double, 3>& HunterStand::Light() noexcept
    {
        static const std::array<double, 3> light = []
        {
            constexpr double x = -0.45;
            constexpr double y = 0.78;
            constexpr double z = 0.44;
            const double magnitude = std::sqrt(x * x + y * y + z * z);
            return std::array<double, 3>{x / magnitude, y / magnitude, z / magnitude};
        }();
        return light;
    }

    Media::Color HunterStand::TintOf(const std::string& name)
    {
        const auto& hunters = HunterData();
        const auto found = hunters.find(name);
        return found != hunters.end() ? found->second.Tint : GuiTheme::Accent;
    }

    HunterStand::HunterStand()
    {
        Focusable(false);
        Cursor(std::make_shared<Av::Input::Cursor>(Av::Input::StandardCursorType::SizeWestEast));
        Media::RenderOptions::SetEdgeMode(*this, Media::EdgeMode::Antialias);
        _turn.Interval(Threading::TimeSpan(0.033));
        _turn.Priority = Threading::DispatcherPriority::Background;
        _turn.Tick += [this](Threading::DispatcherTimer&) { Beat(); };
    }

    void HunterStand::Suit(std::int32_t value)
    {
        const std::int32_t clamped = std::clamp(value, 0, 3);
        if (_suit == clamped)
        {
            return;
        }
        _suit = clamped;
        InvalidateVisual();
    }

    void HunterStand::Name2(std::string value)
    {
        _who = std::move(value);
        InvalidateVisual();
    }

    ::MphRead::Hunter HunterStand::Asked() const noexcept
    {
        const auto parsed = TryParseHunter(_who);
        return parsed ? *parsed : ::MphRead::Hunter::Samus;
    }

    bool HunterStand::EnginePainting() const noexcept
    {
        return Scene::PreviewDrawnLastFrame()
            && (_swapping < SwapGrace
                || (Scene::PreviewDrawnHunter() == Asked()
                    && Scene::PreviewDrawnSuit() == _suit));
    }

    Media::Color HunterStand::SuitTint(Media::Color baseTint) const
    {
        const auto parsed = TryParseHunter(_who);
        if (!parsed)
        {
            return baseTint;
        }
        try
        {
            const ColorRgba sampled = ::MphRead::Mods::HunterSuits::Color(*parsed, _suit);
            return Media::Color::FromRgb(sampled.Red, sampled.Green, sampled.Blue);
        }
        catch (const std::exception&)
        {
            return baseTint;
        }
    }

    Media::Color HunterStand::Wear(Media::Color box, Media::Color baseTint, Media::Color suit) noexcept
    {
        if (suit == baseTint)
        {
            return box;
        }
        auto mix = [](std::uint8_t value, std::uint8_t from, std::uint8_t to)
        {
            const double ratio = from == 0 ? 1.0 : value / static_cast<double>(from);
            return static_cast<std::uint8_t>(std::clamp(to * ratio, 0.0, 255.0));
        };
        return Media::Color::FromRgb(mix(box.R, baseTint.R, suit.R),
            mix(box.G, baseTint.G, suit.G), mix(box.B, baseTint.B, suit.B));
    }

    Media::Color HunterStand::Shade(Media::Color color, double factor) noexcept
    {
        return Media::Color::FromRgb(
            static_cast<std::uint8_t>(std::clamp(color.R * factor, 0.0, 255.0)),
            static_cast<std::uint8_t>(std::clamp(color.G * factor, 0.0, 255.0)),
            static_cast<std::uint8_t>(std::clamp(color.B * factor, 0.0, 255.0)));
    }

    void HunterStand::OnAttachedToVisualTree()
    {
        Av::Controls::Control::OnAttachedToVisualTree();
        _turn.Start();
    }

    void HunterStand::AskForShot()
    {
        const std::shared_ptr<::MphRead::Mods::Render::IHunterShot> host
            = ::MphRead::Mods::Render::HunterShot::Current;
        if (host == nullptr)
        {
            return;
        }
        const double width = Bounds().Width;
        const double height = Bounds().Height;
        if (width <= 8 || height <= 8)
        {
            return;
        }
        const double scale = std::min(1.0, ShotEdge / std::max(width, height));
        const std::int32_t pixelWidth = Grain(static_cast<std::int32_t>(RoundToEven(width * scale)));
        const std::int32_t pixelHeight = Grain(static_cast<std::int32_t>(RoundToEven(height * scale)));
        const std::string want = _who + "/" + std::to_string(_suit) + "/"
            + std::to_string(pixelWidth) + "x" + std::to_string(pixelHeight);
        if (want == _shotAsked)
        {
            return;
        }
        _shotAsked = want;
        const auto hunter = TryParseHunter(_who);
        if (!hunter)
        {
            return;
        }
        auto work = host->RenderAsync(*hunter, _suit, pixelWidth, pixelHeight);
        const std::shared_ptr<HunterStand> self = std::static_pointer_cast<HunterStand>(shared_from_this());
        ::MphRead::NativeRuntime::TaskRun([self, work = std::move(work), pixelWidth, pixelHeight, want]() mutable
        {
            work.wait();
            Threading::Dispatcher::UIThread().Post(
                [self, work = std::move(work), pixelWidth, pixelHeight, want]() mutable
                {
                    std::optional<std::vector<std::uint8_t>> pixels = work.get();
                    if (!pixels || want != self->_shotAsked)
                    {
                        return;
                    }
                    self->Take(std::move(*pixels), pixelWidth, pixelHeight, want);
                });
        });
    }

    std::int32_t HunterStand::Grain(std::int32_t pixels) noexcept
    {
        return std::max(32, (pixels + 31) / 32 * 32);
    }

    void HunterStand::Take(std::vector<std::uint8_t> pixels, std::int32_t width, std::int32_t height,
        const std::string& key)
    {
        try
        {
            const std::size_t byteCount = static_cast<std::size_t>(width) * height * 4;
            std::vector<std::uint8_t> rgba(byteCount, 0);
            const std::size_t copied = std::min(pixels.size(), byteCount);
            for (std::size_t i = 0; i < copied; ++i)
            {
                const std::size_t channel = i % 4;
                if (channel == 0)
                {
                    rgba[i + 2] = pixels[i];
                }
                else if (channel == 1)
                {
                    rgba[i] = pixels[i];
                }
                else if (channel == 2)
                {
                    rgba[i - 2] = pixels[i];
                }
            }
            for (std::size_t i = 3; i < byteCount; i += 4)
            {
                rgba[i] = 255;
            }
            _shot = Media::Imaging::Bitmap::FromPremultipliedRgba(rgba.data(), PixelSize(width, height), width * 4);
            _shotIs = key;
            InvalidateVisual();
        }
        catch (const std::exception& ex)
        {
            ::MphRead::Mods::DebugLog::Line("ui",
                std::string("the hunter picture could not be taken: ") + ex.what());
        }
    }

    void HunterStand::Beat()
    {
        if (Scene::PreviewDrawnHunter() == Asked() && Scene::PreviewDrawnSuit() == _suit)
        {
            _swapping = 0;
        }
        else
        {
            _swapping = std::bit_cast<std::int32_t>(std::bit_cast<std::uint32_t>(_swapping) + 1U);
        }
#if defined(MPHREAD_SHELL)
        if (!IsEffectivelyVisible())
        {
            ::MphRead::Mods::Render::LauncherHunter::Wanted(false);
            return;
        }
        Publish();
        if (EnginePainting())
        {
            return;
        }
#else
        if (::MphRead::Mods::Render::HunterShot::InFrame)
        {
            PublishInFrame();
        }
        else
        {
            AskForShot();
        }
        if (::MphRead::Mods::Render::HunterShot::InFrame && EnginePainting())
        {
            return;
        }
#endif
        InvalidateVisual();
    }

    void HunterStand::PublishInFrame()
    {
        TopLevel* top = TopLevel::GetTopLevel(this);
        if (!IsEffectivelyVisible() || top == nullptr)
        {
            ::MphRead::Mods::Render::HunterShot::HoleWanted = false;
            return;
        }
        const double width = ::MphRead::Mods::Render::HunterShot::FrameWidth > 0
            ? ::MphRead::Mods::Render::HunterShot::FrameWidth : top->ClientSize().Width;
        const double height = ::MphRead::Mods::Render::HunterShot::FrameHeight > 0
            ? ::MphRead::Mods::Render::HunterShot::FrameHeight : top->ClientSize().Height;
        const double scale = ::MphRead::Mods::Render::HunterShot::FrameScale > 0
            ? ::MphRead::Mods::Render::HunterShot::FrameScale : 1;
        const Point origin = TranslatePoint(Point(0, 0), top).value_or(Point(0, 0));
        const Point far = TranslatePoint(Point(Bounds().Width, Bounds().Height), top).value_or(origin);
        if (width <= 0 || height <= 0 || far.X <= origin.X || far.Y <= origin.Y
            || origin.X < 0 || origin.Y < 0 || far.X > width || far.Y > height)
        {
            ::MphRead::Mods::Render::HunterShot::HoleWanted = false;
            ::MphRead::Mods::DebugLog::Line("ui",
                HunterHoleMessage(origin.X, origin.Y, far.X, far.Y, width, height, scale));
            return;
        }
        ::MphRead::Mods::Render::HunterShot::HoleHunter = Asked();
        ::MphRead::Mods::Render::HunterShot::HoleSuit = _suit;
        ::MphRead::Mods::Render::HunterShot::HoleLeft = static_cast<float>(origin.X / width);
        ::MphRead::Mods::Render::HunterShot::HoleTop = static_cast<float>(origin.Y / height);
        ::MphRead::Mods::Render::HunterShot::HoleRight = static_cast<float>(far.X / width);
        ::MphRead::Mods::Render::HunterShot::HoleBottom = static_cast<float>(far.Y / height);
        ::MphRead::Mods::Render::HunterShot::HoleWanted = true;
    }

#if defined(MPHREAD_SHELL)
    bool HunterStand::Publish()
    {
        const std::shared_ptr<UiSurface> surface = UiSurface::Current();
        if (surface == nullptr)
        {
            return false;
        }
        const Point origin = TranslatePoint(Point(0, 0), &surface->Root()).value_or(Point(0, 0));
        const Point far = TranslatePoint(Point(Bounds().Width, Bounds().Height), &surface->Root()).value_or(origin);
        const double windowWidth = surface->WindowWidth();
        const double windowHeight = surface->WindowHeight();
        if (windowWidth <= 0 || windowHeight <= 0 || far.X <= origin.X || far.Y <= origin.Y)
        {
            return false;
        }
        ::MphRead::Mods::Render::LauncherHunter::Wanted(true);
        ::MphRead::Mods::Render::LauncherHunter::Hunter(Asked());
        ::MphRead::Mods::Render::LauncherHunter::Suit(_suit);
        ::MphRead::Mods::Render::LauncherHunter::Left(static_cast<float>(origin.X / windowWidth));
        ::MphRead::Mods::Render::LauncherHunter::Top(static_cast<float>(origin.Y / windowHeight));
        ::MphRead::Mods::Render::LauncherHunter::Right(static_cast<float>(far.X / windowWidth));
        ::MphRead::Mods::Render::LauncherHunter::Bottom(static_cast<float>(far.Y / windowHeight));
        Scene::PreviewWanted(true);
        Scene::PreviewLeft(::MphRead::Mods::Render::LauncherHunter::Left());
        Scene::PreviewTop(::MphRead::Mods::Render::LauncherHunter::Top());
        Scene::PreviewRight(::MphRead::Mods::Render::LauncherHunter::Right());
        Scene::PreviewBottom(::MphRead::Mods::Render::LauncherHunter::Bottom());
        return Scene::PreviewDrawnLastFrame();
    }
#endif

    void HunterStand::OnDetachedFromVisualTree()
    {
        _turn.Stop();
#if defined(MPHREAD_SHELL)
        ::MphRead::Mods::Render::LauncherHunter::Reset();
#else
        ::MphRead::Mods::Render::HunterShot::HoleWanted = false;
#endif
        _shot.reset();
        _shotIs.clear();
        _shotAsked.clear();
        Av::Controls::Control::OnDetachedFromVisualTree();
    }

    void HunterStand::OnPointerPressed(Av::Input::PointerPressedEventArgs& e)
    {
        _dragging = true;
        _dragX = e.GetPosition(this).X;
        _dragBase = _spin;
        e.Pointer->Capture(this);
        e.Handled = true;
        Av::Controls::Control::OnPointerPressed(e);
    }

    void HunterStand::OnPointerMoved(Av::Input::PointerEventArgs& e)
    {
        if (_dragging)
        {
            _spin = _dragBase + (e.GetPosition(this).X - _dragX) * 0.014;
            InvalidateVisual();
        }
        Av::Controls::Control::OnPointerMoved(e);
    }

    void HunterStand::OnPointerReleased(Av::Input::PointerReleasedEventArgs& e)
    {
        _dragging = false;
        Av::Controls::Control::OnPointerReleased(e);
    }

    void HunterStand::OnPointerCaptureLost(Av::Input::PointerCaptureLostEventArgs& e)
    {
        _dragging = false;
        Av::Controls::Control::OnPointerCaptureLost(e);
    }

    void HunterStand::Render(Media::DrawingContext& context)
    {
        const ::MphRead::NativeRuntime::TimeSpan now = _clock.Elapsed();
        const double dt = std::min(0.1, (now - _lastFrame).TotalSeconds());
        _lastFrame = now;
        if (!_dragging)
        {
            _spin += dt * 0.7;
        }
        const double width = Bounds().Width;
        const double height = Bounds().Height;
        if (width <= 0 || height <= 0)
        {
            return;
        }
#if defined(MPHREAD_SHELL)
        if (EnginePainting())
        {
            return;
        }
#else
        if (::MphRead::Mods::Render::HunterShot::InFrame && EnginePainting())
        {
            return;
        }
#endif
        if (_shot != nullptr && _shotIs == _shotAsked)
        {
            const Rect well(0, 0, width, height);
            auto clip = context.PushClip(well, CornerRadius(7));
            context.DrawImage(*_shot, well);
            return;
        }

        const auto& hunters = HunterData();
        const auto found = hunters.find(_who);
        const Hunter& hunter = found != hunters.end() ? found->second : hunters.at("Samus");
        const Rect well(0, 0, width, height);
        context.DrawRectangle(std::make_shared<Media::SolidColorBrush>(GuiTheme::Ink), nullptr,
            RoundedRect(well, 7));

        auto glow = std::make_shared<Media::RadialGradientBrush>();
        glow->Center = RelativePoint(0.5, 0.42, RelativeUnit::Relative);
        glow->GradientOrigin = RelativePoint(0.5, 0.42, RelativeUnit::Relative);
        glow->RadiusX = RelativeScalar(0.7, RelativeUnit::Relative);
        glow->RadiusY = RelativeScalar(0.6, RelativeUnit::Relative);
        glow->GradientStops.emplace_back(Media::Color::FromArgb(70,
            hunter.Tint.R, hunter.Tint.G, hunter.Tint.B), 0);
        glow->GradientStops.emplace_back(Media::Color::FromArgb(0,
            hunter.Tint.R, hunter.Tint.G, hunter.Tint.B), 1);
        context.DrawRectangle(glow, nullptr, RoundedRect(well, 7));

        const double cos = std::cos(_spin);
        const double sin = std::sin(_spin);
        constexpr double pitch = 0.16;
        const double cp = std::cos(pitch);
        const double sp = std::sin(pitch);
        constexpr double distance = 170;
        const double focal = height * 1.02;
        const double cx = width / 2;
        const double cy = height * 0.63;
        const Media::Color suit = SuitTint(hunter.Tint);

        struct Quad final
        {
            double Z;
            std::array<Point, 4> Points;
            Media::Color Colour;
        };
        std::vector<Quad> quads;
        quads.reserve(hunter.Boxes.size() * 6);
        for (const Box& box : hunter.Boxes)
        {
            const Media::Color worn = Wear(box.Colour, hunter.Tint, suit);
            const double hw = box.W / 2;
            const double hh = box.H / 2;
            const double hd = box.D / 2;
            std::array<Point, 8> points{};
            std::array<double, 8> depth{};
            for (std::int32_t i = 0; i < 8; ++i)
            {
                const double lx = box.X + ((i & 1) != 0 ? hw : -hw);
                const double ly = box.Y + ((i & 4) != 0 ? hh : -hh);
                const double lz = box.Z + ((i & 2) != 0 ? hd : -hd);
                const double rx = lx * cos - lz * sin;
                const double rz0 = lx * sin + lz * cos;
                const double ry = ly * cp - rz0 * sp;
                const double rz = ly * sp + rz0 * cp;
                const double projection = focal / (distance + rz);
                points[static_cast<std::size_t>(i)] = Point(cx + rx * projection, cy - ry * projection);
                depth[static_cast<std::size_t>(i)] = rz;
            }
            for (std::int32_t faceIndex = 0; faceIndex < 6; ++faceIndex)
            {
                const auto& face = Faces()[static_cast<std::size_t>(faceIndex)];
                const auto& normal = Normals()[static_cast<std::size_t>(faceIndex)];
                const double nx = normal[0] * cos - normal[2] * sin;
                const double nz = normal[0] * sin + normal[2] * cos;
                const double ny = normal[1] * cp - nz * sp;
                const double lit = std::max(0.0,
                    nx * Light()[0] + ny * Light()[1] + nz * Light()[2]);
                quads.push_back(Quad{
                    (depth[static_cast<std::size_t>(face[0])] + depth[static_cast<std::size_t>(face[1])]
                        + depth[static_cast<std::size_t>(face[2])] + depth[static_cast<std::size_t>(face[3])]) / 4,
                    {{points[static_cast<std::size_t>(face[0])], points[static_cast<std::size_t>(face[1])],
                      points[static_cast<std::size_t>(face[2])], points[static_cast<std::size_t>(face[3])]}},
                    Shade(worn, 0.42 + lit * 0.78)});
            }
        }
        ::MphRead::NativeRuntime::ManagedSort(quads, [](const Quad& a, const Quad& b)
        {
            // List<T>.Sort uses Double.CompareTo on the reversed operands.
            // Preserve its order for equal depths and its NaN rule too.
            if (std::isnan(a.Z) || std::isnan(b.Z))
            {
                if (std::isnan(a.Z) && std::isnan(b.Z))
                {
                    return 0;
                }
                return std::isnan(b.Z) ? -1 : 1;
            }
            return b.Z < a.Z ? -1 : (b.Z > a.Z ? 1 : 0);
        });

        const auto edge = std::make_shared<Media::Pen>(
            std::make_shared<Media::SolidColorBrush>(Media::Color::FromArgb(72, 0, 0, 0)), 0.7);
        for (const Quad& quad : quads)
        {
            Media::StreamGeometry geometry;
            {
                auto path = geometry.Open();
                path.BeginFigure(quad.Points[0], true);
                path.LineTo(quad.Points[1]);
                path.LineTo(quad.Points[2]);
                path.LineTo(quad.Points[3]);
                path.EndFigure(true);
            }
            context.DrawGeometry(std::make_shared<Media::SolidColorBrush>(quad.Colour), edge, geometry);
        }
    }
}
