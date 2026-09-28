#pragma once

#include "Deck.hpp"
#include "Tap.hpp"
#include "../../../NativeRuntime/System/Stopwatch.hpp"

#include <memory>
#include <string>

namespace MphRead::Mods::Launcher::Gui
{
    // One map, as a card with the map on it: the reference's .pcard.
    class DeckTile final : public Av::Controls::Control
    {
    public:
        DeckTile(std::string roomKey, std::string code);

        Av::Event<DeckTile&> Click;

        // The room this card stands for, and what the tag says.
        const std::string RoomKey;
        const std::string Code;
        // The line along the bottom. Empty on a map, used by a clip.
        std::string Blurb;
        // What the word along the bottom says when this is not the one.
        std::string Verb = "Select";
        std::string ChosenVerb = "Selected";
        // How many people have picked this map; negative means no ballot.
        std::int32_t Tally = -1;
        bool Leader = false;
        // Width over height.
        double Ratio = 1;

        [[nodiscard]] bool Chosen() const noexcept { return _chosen; }
        void Chosen(bool value);

        // Hover and lean, decided by DeckGrid, in this card's coordinates.
        void Hover(bool over, Av::Point at);
        // The grid decided this card was tapped.
        void Fire() { Click(*this); }

        void Render(Av::Media::DrawingContext& context) override;

    protected:
        Av::Size MeasureOverride(Av::Size availableSize) override;
        void OnKeyDown(Av::Input::KeyEventArgs& e) override;
        void OnGotFocus(Av::Input::GotFocusEventArgs& e) override;
        void OnLostFocus(Av::Input::FocusChangedEventArgs& e) override;

    private:
        [[nodiscard]] double Em() const { return Deck::GetEm(*this); }
        bool Settle();
        void Ask();
        [[nodiscard]] static Av::Media::BoxShadows Shadows(Av::Media::Color ring, bool raised);
        void Badge(Av::Media::DrawingContext& context, double w, double em) const;
        void Drift(Av::Media::DrawingContext& context, double w, double h) const;
        static void Scrim(Av::Media::DrawingContext& context, double w, double h);
        void Info(Av::Media::DrawingContext& context, double w, double h, double em) const;

        static constexpr double MaxTilt = 3.5;
        static constexpr double Stiffness = 220;
        static constexpr double Damping = 18;
        ::MphRead::NativeRuntime::Stopwatch _clock = ::MphRead::NativeRuntime::Stopwatch::StartNew();
        ::MphRead::NativeRuntime::TimeSpan _last{};
        bool _over = false;
        bool _chosen = false;
        double _pop = 1;
        double _popVelocity = 0;
        double _popTarget = 1;
        double _tilt = 0;
        double _tiltTarget = 0;
        double _tiltX = 0;
        double _tiltXTarget = 0;
        bool _framePending = false;
    };

    // The reference's .grid: three cards across, two on a phone, half an em
    // between them. The grid resolves its own pointer against the rectangles
    // it arranged the cards into, since the toolkit hit-tests what a card
    // drew and a card draws its shadows well outside its box.
    class DeckGrid final : public Av::Controls::Panel
    {
    public:
        DeckGrid();

        // Fixed number of columns, or zero for the container query.
        std::int32_t FixedColumns = 0;
        // Every card's shape, handed down as they are added.
        double Ratio = 1;

    protected:
        Av::Size MeasureOverride(Av::Size availableSize) override;
        Av::Size ArrangeOverride(Av::Size finalSize) override;
        void OnPointerExited(Av::Input::PointerEventArgs& e) override;
        void OnPointerCaptureLost(Av::Input::PointerCaptureLostEventArgs& e) override;

    private:
        static constexpr double TwoColumnFrame = 640;

        [[nodiscard]] std::shared_ptr<DeckTile> TileAt(Av::Point at) const;
        void Hover(std::shared_ptr<DeckTile> tile, Av::Point at);
        void Pressed(Av::Input::PointerPressedEventArgs& e);
        void Moved(Av::Input::PointerEventArgs& e);
        void Released(Av::Input::PointerReleasedEventArgs& e);
        [[nodiscard]] std::int32_t Columns() const;
        [[nodiscard]] double Gap() const;

        Tap _tap;
        std::shared_ptr<DeckTile> _down;
        std::shared_ptr<DeckTile> _over;
    };
}
