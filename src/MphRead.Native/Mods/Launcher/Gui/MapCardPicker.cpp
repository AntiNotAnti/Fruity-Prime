#include "MapCardPicker.hpp"

#include "GuiTheme.hpp"
#include "UiLayout.hpp"
#include "../../../Metadata/Metadata.hpp"
#include "../../../Metadata/Rooms.hpp"
#include "../../../NativeRuntime/System/Globalization.hpp"

#include <utility>

namespace MphRead::Mods::Launcher::Gui
{
    using namespace ::MphRead::NativeRuntime::Avalonia;
    using ::MphRead::NativeRuntime::StringEqualsOrdinalIgnoreCase;
    using ::MphRead::NativeRuntime::StringIsNullOrWhiteSpace;
    using ::MphRead::NativeRuntime::StringSplit;

    std::shared_ptr<DeckTile> MapCardFactory::Create(const std::string& room)
    {
        const std::vector<std::string> parts = StringSplit(room, ' ', true);
        const std::string code = parts.empty() ? room : parts.front();
        const auto [metadata, roomId] = ::MphRead::Metadata::GetRoomByName(room);
        (void)roomId;

        auto tile = std::make_shared<DeckTile>(room, code);
        tile->Blurb = metadata != nullptr ? metadata->InGameName.value_or("") : "";
        return tile;
    }

    MapCardPicker::MapCardPicker(
        const std::vector<std::string>& rooms, std::optional<std::string> selected)
        : _grid(std::make_shared<DeckGrid>()),
          _note(std::make_shared<Note>("")),
          _selected(std::move(selected))
    {
        Background(Media::Brushes::Transparent());
        Focusable(true);

        auto back = std::make_shared<UiMark>(UiMark::Shape::Cancel, "back");
        back->Click += [this](UiMark&) { Cancelled(*this); };

        _use = std::make_shared<UiMark>(UiMark::Shape::Accept, "use map");
        _use->Click += [this](UiMark&)
        {
            if (!StringIsNullOrWhiteSpace(_selected))
            {
                Done(*this, *_selected);
            }
        };

        auto scroll = std::make_shared<Controls::ScrollViewer>();
        scroll->Content(_grid);
        scroll->ClipToBounds(true);
        scroll->HorizontalScrollBarVisibility(Controls::ScrollBarVisibility::Disabled);
        scroll->VerticalScrollBarVisibility(Controls::ScrollBarVisibility::Auto);

        auto body = std::make_shared<Controls::Grid>();
        body->RowDefinitions(Controls::RowDefinitions("*,Auto"));
        body->RowSpacing(6);
        body->Children.Add(scroll);
        Controls::Grid::SetRow(*_note, 1);
        body->Children.Add(_note);

        Content(UiLayout::Page(false, UiLayout::WellPlay, "choose map", nullptr,
            body, back, _use));

        if (rooms.empty())
        {
            _note->Text("No multiplayer rooms were found. Set the game files up from Settings.");
            _note->Foreground(GuiTheme::WarmBrush);
            _use->IsEnabled(false);
            return;
        }

        for (const std::string& room : rooms)
        {
            std::shared_ptr<DeckTile> tile = MapCardFactory::Create(room);
            tile->Chosen(_selected.has_value()
                && StringEqualsOrdinalIgnoreCase(room, *_selected));
            const std::weak_ptr<DeckTile> weakTile = tile;
            tile->Click += [this, weakTile](DeckTile&)
            {
                if (const std::shared_ptr<DeckTile> selectedTile = weakTile.lock())
                {
                    Select(selectedTile);
                }
            };
            _grid->Children.Add(tile);
        }
        RefreshSelection();

        const std::shared_ptr<DeckGrid> grid = _grid;
        AttachedToVisualTree += [grid](Controls::Control&)
        {
            // The native toolkit attaches a parent before its children. Defer
            // the focus until the grid's tiles have a root to receive it.
            Threading::Dispatcher::UIThread().Post([grid]
            {
                MapCardPicker::FocusInitialSelection(grid);
            }, Threading::DispatcherPriority::Background);
        };
    }

    void MapCardPicker::Select(const std::shared_ptr<DeckTile>& selected)
    {
        _selected = selected->RoomKey;
        for (const Controls::ControlPtr& child : _grid->Children)
        {
            if (const auto tile = std::dynamic_pointer_cast<DeckTile>(child); tile != nullptr)
            {
                tile->Chosen(tile.get() == selected.get());
            }
        }
        RefreshSelection();
    }

    void MapCardPicker::RefreshSelection()
    {
        const bool hasSelection = !StringIsNullOrWhiteSpace(_selected);
        _use->IsEnabled(hasSelection);
        if (!hasSelection)
        {
            _note->Text("Choose a map.");
        }
        else
        {
            const auto [metadata, roomId] = ::MphRead::Metadata::GetRoomByName(*_selected);
            (void)roomId;
            const std::string name = metadata != nullptr
                ? metadata->InGameName.value_or(*_selected)
                : *_selected;
            _note->Text("Selected: " + name);
        }
        _note->Foreground(GuiTheme::TextDimBrush);
    }

    void MapCardPicker::FocusInitialSelection(const std::shared_ptr<DeckGrid>& grid)
    {
        for (const Controls::ControlPtr& child : grid->Children)
        {
            if (const auto tile = std::dynamic_pointer_cast<DeckTile>(child);
                tile != nullptr && tile->Chosen())
            {
                tile->Focus();
                return;
            }
        }
        if (grid->Children.Count() > 0)
        {
            grid->Children[0]->Focus();
        }
    }

    void MapCardPicker::OnKeyDown(Input::KeyEventArgs& e)
    {
        if (e.Key == Input::Key::Escape)
        {
            Cancelled(*this);
            e.Handled = true;
            return;
        }
        UserControl::OnKeyDown(e);
    }
}
