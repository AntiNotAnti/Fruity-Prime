#pragma once

#include "../../../NativeRuntime/Avalonia/Avalonia.hpp"

#include <array>
#include <cstdint>
#include <memory>
#include <string>

namespace MphRead::Mods::Launcher::Gui
{
    namespace Av = ::MphRead::NativeRuntime::Avalonia;

    class ChoiceRow;
    class DeckButton;
    class DeckGrid;
    class HunterStand;
    class Note;
    class UiTabs;

    // The ballot and hunter picker drawn beside the engine-owned scoreboard.
    class EndPanelView final : public Av::Controls::UserControl
    {
    public:
        EndPanelView();

        // Open on the hunter face, for -uishot.
        void ShowHunter();
        void Refresh();

    private:
        [[nodiscard]] std::int32_t HunterIndex() const;
        [[nodiscard]] Av::Media::Color SuitColour() const;
        void ShowFace();
        void Commit();

        std::array<std::string, 7> _hunters;
        std::shared_ptr<UiTabs> _tabs;
        std::shared_ptr<DeckGrid> _ballot;
        std::shared_ptr<Av::Controls::ScrollViewer> _ballotScroll;
        std::shared_ptr<Av::Controls::StackPanel> _hunterPane;
        std::shared_ptr<HunterStand> _stand;
        std::shared_ptr<ChoiceRow> _hunter;
        std::shared_ptr<ChoiceRow> _suit;
        std::shared_ptr<DeckButton> _ready;
        std::shared_ptr<Note> _count;
        std::shared_ptr<Note> _empty;
        std::string _order;
    };
}
