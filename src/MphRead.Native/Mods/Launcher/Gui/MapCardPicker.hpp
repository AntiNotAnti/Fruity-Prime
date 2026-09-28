#pragma once

#include "../../../NativeRuntime/Avalonia/Avalonia.hpp"
#include "DeckTile.hpp"
#include "Rows.hpp"
#include "UiMark.hpp"

#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace MphRead::Mods::Launcher::Gui
{
    namespace Av = ::MphRead::NativeRuntime::Avalonia;

    // Builds the map card shared by the Offline screen and map pickers.
    class MapCardFactory final
    {
    public:
        MapCardFactory() = delete;
        [[nodiscard]] static std::shared_ptr<DeckTile> Create(const std::string& room);
    };

    // One-map picker using the same DeckTile/DeckGrid presentation as Offline.
    class MapCardPicker final : public Av::Controls::UserControl
    {
    public:
        MapCardPicker(const std::vector<std::string>& rooms, std::optional<std::string> selected);

        Av::Event<MapCardPicker&, std::string> Done;
        Av::Event<MapCardPicker&> Cancelled;

    protected:
        void OnKeyDown(Av::Input::KeyEventArgs& e) override;

    private:
        void Select(const std::shared_ptr<DeckTile>& selected);
        void RefreshSelection();
        static void FocusInitialSelection(const std::shared_ptr<DeckGrid>& grid);

        std::shared_ptr<DeckGrid> _grid;
        std::shared_ptr<Note> _note;
        std::shared_ptr<UiMark> _use;
        std::optional<std::string> _selected;
    };
}
