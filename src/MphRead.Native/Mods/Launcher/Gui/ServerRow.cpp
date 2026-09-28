#include "ServerRow.hpp"

#include "Deck.hpp"
#include "DeckText.hpp"
#include "GuiTheme.hpp"
#include "MapShot.hpp"
#include "ServerBadge.hpp"
#include "../../../Metadata/Rooms.hpp"
#include "../../../NativeRuntime/System/Encoding.hpp"
#include "../../../NativeRuntime/System/Managed.hpp"
#include "../../../NativeRuntime/System/Number.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <numbers>
#include <optional>
#include <string_view>
#include <utility>

namespace MphRead::Mods::Launcher::Gui
{
    namespace Runtime = ::MphRead::NativeRuntime;
    using namespace Runtime::Avalonia;

    namespace
    {
        [[nodiscard]] Av::Media::IBrushPtr Solid(Av::Media::Color colour)
        {
            return std::make_shared<Av::Media::SolidColorBrush>(colour);
        }

        [[nodiscard]] std::size_t FindNameTail(const std::string& name) noexcept
        {
            std::size_t at = name.size();
            while (at > 1)
            {
                --at;
                while (at > 0 && (static_cast<unsigned char>(name[at]) & 0xC0U) == 0x80U)
                {
                    --at;
                }
                if (Runtime::Utf16Length(std::string_view(name).substr(at)) > 7U)
                {
                    break;
                }
                const char ch = name[at];
                if (ch == '.' || ch == '#')
                {
                    return at < name.size() - 1 ? at : std::string::npos;
                }
                if (ch == '-')
                {
                    break;
                }
            }
            return std::string::npos;
        }
    }

    ServerRow::Columns::Columns(double width, bool narrow)
        : Narrow(narrow)
    {
        constexpr double padX = 0.6 * Deck::RowEm;
        constexpr double gap = 0.55 * Deck::RowEm;
        constexpr double numberWidth = 2.6 * 0.86 * Deck::RowEm;
        FlagX = padX;
        const double x = padX + FlagWidth + gap;
        if (narrow)
        {
            const double free = std::max(0.0,
                width - padX * 2 - gap * 3 - FlagWidth - numberWidth);
            NameWidth = free * (1.4 / 2.4);
            MapWidth = free - NameWidth;
            ModeWidth = 0;
            PlayersWidth = 0;
            NameX = x;
            MapX = NameX + NameWidth + gap;
            ModeX = PlayersX = 0;
            PingWidth = numberWidth;
            PingX = MapX + MapWidth + gap;
            return;
        }
        const double rest = std::max(0.0,
            width - padX * 2 - gap * 5 - FlagWidth - numberWidth * 2);
        NameWidth = rest * (1.5 / 3.8);
        MapWidth = rest * (1.3 / 3.8);
        ModeWidth = rest - NameWidth - MapWidth;
        NameX = x;
        MapX = NameX + NameWidth + gap;
        ModeX = MapX + MapWidth + gap;
        PlayersWidth = numberWidth;
        PlayersX = ModeX + ModeWidth + gap;
        PingWidth = numberWidth;
        PingX = PlayersX + PlayersWidth + gap;
    }

    ServerRow::ServerRow(std::string name, std::string endpoint)
        : _name(std::move(name)), _endpoint(std::move(endpoint)),
          _pingBrush(GuiTheme::TextDimBrush)
    {
        Height(SlabHeight);
        Focusable(true);
        Cursor(std::make_shared<Input::Cursor>(Input::StandardCursorType::Hand));
        Media::RenderOptions::SetEdgeMode(*this, Media::EdgeMode::Aliased);
        DoubleTapped += [this](Input::InputElement&, Interactivity::RoutedEventArgs&)
        {
            if (_asking || !_answered)
            {
                return;
            }
            Clicked(*this);
            Activated(*this);
        };
    }

    void ServerRow::IsSelected(bool value)
    {
        if (_selected == value)
        {
            return;
        }
        _selected = value;
        InvalidateVisual();
    }

    void ServerRow::SetStatus(const ::MphRead::Mods::Network::ServerStatus& status)
    {
        _asking = false;
        _answered = status.Online;
        if (!status.Online)
        {
            _map = "did not answer";
            _mode.clear();
            _players = "—";
            _ping = "—";
            _pingBrush = Solid(GuiTheme::Bad);
            Cursor(std::make_shared<Input::Cursor>(Input::StandardCursorType::Arrow));
            InvalidateVisual();
            return;
        }

        _roomKey = status.RoomKey;
        const auto room = ::MphRead::Metadata::RoomMetadata.find(status.RoomKey);
        _map = room != ::MphRead::Metadata::RoomMetadata.end() && room->second != nullptr
            && room->second->InGameName.has_value() && !room->second->InGameName->empty()
            ? *room->second->InGameName : status.RoomKey;
        _mode = ::MphRead::Mods::Network::NetStatus::ModeName(status.Mode);
        _players = status.MaxPlayers > 0
            ? Runtime::ToString(status.Players) + "/" + Runtime::ToString(status.MaxPlayers)
            : Runtime::ToStringInvariant(status.Players);
        if (status.Latency >= 0)
        {
            _ping = Runtime::ToStringInvariant(status.Latency);
            _pingBrush = Solid(PingColour(status.Latency));
        }
        else
        {
            _ping = "—";
            _pingBrush = GuiTheme::TextDimBrush;
        }
        InvalidateVisual();
    }

    Media::Color ServerRow::PingColour(std::int32_t ms) noexcept
    {
        return ms < 60 ? GuiTheme::Good : ms < 120 ? GuiTheme::Warn : GuiTheme::Bad;
    }

    void ServerRow::OnPointerEntered(Input::PointerEventArgs& e)
    {
        _hot = true;
        InvalidateVisual();
        Control::OnPointerEntered(e);
    }

    void ServerRow::OnPointerExited(Input::PointerEventArgs& e)
    {
        _hot = false;
        _lean = 0;
        _tap.Cancel();
        InvalidateVisual();
        Control::OnPointerExited(e);
    }

    void ServerRow::OnPointerPressed(Input::PointerPressedEventArgs& e)
    {
        _tap.Press(e, *this);
        InvalidateVisual();
        Control::OnPointerPressed(e);
    }

    void ServerRow::OnPointerMoved(Input::PointerEventArgs& e)
    {
        if (_tap.Moved(e, *this))
        {
            InvalidateVisual();
        }
        Control::OnPointerMoved(e);
    }

    void ServerRow::OnPointerReleased(Input::PointerReleasedEventArgs& e)
    {
        const bool tapped = _tap.Release(e, *this);
        InvalidateVisual();
        if (tapped && IsLive())
        {
            Focus();
            Clicked(*this);
        }
        Control::OnPointerReleased(e);
    }

    void ServerRow::OnPointerCaptureLost(Input::PointerCaptureLostEventArgs& e)
    {
        _tap.Cancel();
        InvalidateVisual();
        Control::OnPointerCaptureLost(e);
    }

    void ServerRow::OnGotFocus(Input::GotFocusEventArgs& e)
    {
        InvalidateVisual();
        Control::OnGotFocus(e);
    }

    void ServerRow::OnLostFocus(Input::FocusChangedEventArgs& e)
    {
        InvalidateVisual();
        Control::OnLostFocus(e);
    }

    void ServerRow::OnKeyDown(Input::KeyEventArgs& e)
    {
        if ((e.Key == Input::Key::Enter || e.Key == Input::Key::Space) && IsLive())
        {
            Clicked(*this);
            Activated(*this);
            e.Handled = true;
            return;
        }
        Control::OnKeyDown(e);
    }

    void ServerRow::Render(Media::DrawingContext& context)
    {
        const double width = Bounds().Width;
        if (width <= 0)
        {
            return;
        }
        const bool live = (_hot || IsFocused() || _selected) && IsLive();
        const double lip = live ? LipLive : Lip;
        const double drop = _tap.Down() && IsLive() ? Lip : 0;
        const Rect slab(0, drop, width, SlabHeight);
        const RoundedRect round(slab, Radius);

        context.FillRectangle(Media::Brushes::Transparent(), Rect(0, 0, width, Bounds().Height));
        const double scale = _hot && !_tap.Down() && IsLive() ? 1.012 : 1;
        auto pose = context.PushTransform(Av::Matrix::CreateTranslation(-width / 2, -SlabHeight / 2)
            * Av::Matrix::CreateScale(scale, scale)
            * Av::Matrix::CreateTranslation(width / 2, SlabHeight / 2));
        std::optional<Media::DrawingContext::PushedState> wait;
        if (_asking)
        {
            wait.emplace(context.PushOpacity(0.72));
        }

        const Media::Color ring = _selected ? GuiTheme::Accent
            : live ? Deck::Rgb(0x4a6f8c) : GuiTheme::Edge;
        const Media::BoxShadows shadows(Deck::Shadow(0, 0, 0, live || _selected ? 2 : 1, ring),
            {Deck::Shadow(0, lip, 0, 0, Deck::Fade(0, live ? 0.5 : 0.45))});
        context.DrawRectangle(Solid(GuiTheme::PanelDeep), {}, round, shadows);

        DrawMap(context, round, slab, live);
        const Columns columns(width, Deck::Narrow(*this));
        const double top = drop;
        ServerBadge::Draw(context, _endpoint, _answered, columns.FlagX,
            top + Runtime::RoundToEven((SlabHeight - Columns::FlagHeight) / 2));

        const Av::Media::IBrushPtr nameInk = _asking || !_answered
            ? GuiTheme::TextDimBrush : GuiTheme::TextBrush;
        DrawName(context, columns.NameX, columns.NameWidth, top, nameInk);
        Cell(context, _map, columns.MapX, columns.MapWidth, top, 1.02, true,
            _answered ? Solid(Deck::Rgb(0xcfd6e4)) : GuiTheme::TextDimBrush);
        if (!columns.Narrow)
        {
            Cell(context, _mode, columns.ModeX, columns.ModeWidth, top, 0.82, false,
                GuiTheme::TextDimBrush);
            Cell(context, _players, columns.PlayersX, columns.PlayersWidth, top, 0.86, false,
                GuiTheme::TextBrush, true);
        }
        Cell(context, _ping, columns.PingX, columns.PingWidth, top, 0.86, false, _pingBrush, true);
    }

    void ServerRow::DrawMap(Media::DrawingContext& context, const RoundedRect& round,
        const Rect& slab, bool live)
    {
        const std::optional<std::string> key = _answered
            ? std::optional<std::string>(_roomKey) : std::nullopt;
        const auto shot = MapShot::For(key);
        if (shot == nullptr)
        {
            return;
        }
        {
            auto clip = context.PushClip(round);
            // The group opacity only affects this image, so establish its
            // rounded bounds before opening the layer. The CPU renderer can
            // then allocate and composite the row-sized clip instead of the
            // whole launcher surface for every server row.
            auto opacity = context.PushOpacity(live ? 0.72 : 0.5);
            const double scale = slab.Width / shot->Size().Width;
            const double height = shot->Size().Height * scale;
            context.DrawImage(*shot, Rect(slab.X, slab.Y + (slab.Height - height) / 2,
                slab.Width, height));
        }
        {
            auto clip = context.PushClip(round);
            auto scrim = std::make_shared<Media::LinearGradientBrush>();
            scrim->StartPoint = RelativePoint(0, 0, RelativeUnit::Relative);
            scrim->EndPoint = RelativePoint(1, 0, RelativeUnit::Relative);
            scrim->GradientStops = {
                Media::GradientStop(Media::Color::FromArgb(240, 10, 12, 16), 0),
                Media::GradientStop(Media::Color::FromArgb(184, 10, 12, 16), 0.45),
                Media::GradientStop(Media::Color::FromArgb(224, 10, 12, 16), 1)
            };
            context.FillRectangle(scrim, slab);
        }
    }

    void ServerRow::Cell(Media::DrawingContext& context, const std::string& text,
        double x, double width, double top, double sizeEms, bool display,
        const Media::IBrushPtr& ink, bool rightAlign)
    {
        if (text.empty() || width <= 2)
        {
            return;
        }
        const double size = Deck::RowEm * sizeEms;
        const auto laid = DeckText::Run(text, display ? Deck::Label(false) : Deck::Body(false),
            size, ink, Runtime::RoundToEven(width));
        const double left = rightAlign ? x + width - std::min(laid->Width(), width) : x;
        auto clip = context.PushClip(Rect(Runtime::RoundToEven(x), top,
            Runtime::RoundToEven(width), SlabHeight));
        context.DrawText(*laid, Point(Runtime::RoundToEven(left),
            Runtime::RoundToEven(top + (SlabHeight - laid->Height()) / 2)));
    }

    void ServerRow::DrawName(Media::DrawingContext& context, double x, double width,
        double top, const Media::IBrushPtr& ink)
    {
        if (width <= 2)
        {
            return;
        }
        const double size = Deck::RowEm * 1.2;
        const std::size_t cut = FindNameTail(_name);
        const std::string stem = cut != std::string::npos ? _name.substr(0, cut) : _name;
        const std::string tail = cut != std::string::npos ? _name.substr(cut) : std::string{};
        auto clip = context.PushClip(Rect(Runtime::RoundToEven(x), top,
            Runtime::RoundToEven(width), SlabHeight));
        double pen = x;
        pen += Shadowed(context, stem, size, ink, pen, top, width);
        if (!tail.empty())
        {
            const double left = width - (pen - x);
            if (left > size * 0.5)
            {
                Shadowed(context, tail, size, GuiTheme::AccentBrush, pen, top, left);
            }
        }
    }

    double ServerRow::Shadowed(Media::DrawingContext& context, const std::string& text,
        double size, const Media::IBrushPtr& ink, double x, double top, double width)
    {
        const Media::IBrushPtr shadow = Solid(Media::Color::FromArgb(204, 0, 0, 0));
        const auto under = DeckText::Run(text, Deck::Label(false), size, shadow,
            Runtime::RoundToEven(width));
        const auto over = DeckText::Run(text, Deck::Label(false), size, ink,
            Runtime::RoundToEven(width));
        const double y = Runtime::RoundToEven(top + (SlabHeight - over->Height()) / 2);
        context.DrawText(*under, Point(Runtime::RoundToEven(x), y + 2));
        context.DrawText(*over, Point(Runtime::RoundToEven(x), y));
        return std::min(over->Width(), width);
    }
}
