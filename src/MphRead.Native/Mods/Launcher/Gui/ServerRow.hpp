#pragma once

#include "Deck.hpp"
#include "Tap.hpp"
#include "../../../Mods/Network/NetStatus.hpp"

#include <string>

namespace MphRead::Mods::Launcher::Gui
{
    namespace Av = ::MphRead::NativeRuntime::Avalonia;

    // One server in the browser: a slab with its own map behind it.
    class ServerRow final : public Av::Controls::Control
    {
    public:
        struct Columns final
        {
            static constexpr double FlagWidth = 1.5 * Deck::RowEm;
            static constexpr double FlagHeight = 1.05 * Deck::RowEm;

            double FlagX = 0;
            double NameX = 0;
            double NameWidth = 0;
            double MapX = 0;
            double MapWidth = 0;
            double ModeX = 0;
            double ModeWidth = 0;
            double PlayersX = 0;
            double PlayersWidth = 0;
            double PingX = 0;
            double PingWidth = 0;
            bool Narrow = false;

            Columns(double width, bool narrow);
        };

        static constexpr double SlabHeight = 29.6;

        explicit ServerRow(std::string name, std::string endpoint);

        Av::Event<ServerRow&> Clicked;
        Av::Event<ServerRow&> Activated;

        [[nodiscard]] bool IsSelected() const noexcept { return _selected; }
        void IsSelected(bool value);
        [[nodiscard]] bool IsLive() const noexcept { return _answered && !_asking; }

        void SetStatus(const ::MphRead::Mods::Network::ServerStatus& status);
        [[nodiscard]] static Av::Media::Color PingColour(std::int32_t ms) noexcept;

        [[nodiscard]] const std::string& Endpoint() const noexcept { return _endpoint; }
        [[nodiscard]] std::string RoomKey() const { return _answered ? _roomKey : std::string{}; }
        [[nodiscard]] const std::string& DisplayName() const noexcept { return _name; }
        [[nodiscard]] const std::string& MapName() const noexcept { return _map; }
        [[nodiscard]] const std::string& ModeName() const noexcept { return _mode; }
        [[nodiscard]] const std::string& PlayerCount() const noexcept { return _players; }
        [[nodiscard]] const std::string& PingText() const noexcept { return _ping; }
        [[nodiscard]] Av::Media::IBrushPtr PingBrush() const noexcept { return _pingBrush; }

        void Render(Av::Media::DrawingContext& context) override;

    protected:
        void OnPointerEntered(Av::Input::PointerEventArgs& e) override;
        void OnPointerExited(Av::Input::PointerEventArgs& e) override;
        void OnPointerPressed(Av::Input::PointerPressedEventArgs& e) override;
        void OnPointerMoved(Av::Input::PointerEventArgs& e) override;
        void OnPointerReleased(Av::Input::PointerReleasedEventArgs& e) override;
        void OnPointerCaptureLost(Av::Input::PointerCaptureLostEventArgs& e) override;
        void OnKeyDown(Av::Input::KeyEventArgs& e) override;
        void OnGotFocus(Av::Input::GotFocusEventArgs& e) override;
        void OnLostFocus(Av::Input::FocusChangedEventArgs& e) override;

    private:
        static constexpr double Lip = 3;
        static constexpr double LipLive = 5;
        static constexpr double Radius = 0.4 * Deck::RowEm;

        void DrawMap(Av::Media::DrawingContext& context, const Av::RoundedRect& round,
            const Av::Rect& slab, bool live);
        static void Cell(Av::Media::DrawingContext& context, const std::string& text,
            double x, double width, double top, double sizeEms, bool display,
            const Av::Media::IBrushPtr& ink, bool rightAlign = false);
        void DrawName(Av::Media::DrawingContext& context, double x, double width,
            double top, const Av::Media::IBrushPtr& ink);
        static double Shadowed(Av::Media::DrawingContext& context, const std::string& text,
            double size, const Av::Media::IBrushPtr& ink, double x, double top, double width);

        const std::string _name;
        const std::string _endpoint;
        std::string _map = "asking…";
        std::string _roomKey;
        std::string _mode;
        std::string _players = "—";
        std::string _ping = "—";
        Av::Media::IBrushPtr _pingBrush;
        bool _answered = false;
        bool _asking = true;
        bool _hot = false;
        bool _selected = false;
        double _lean = 0;
        Tap _tap;
    };
}
