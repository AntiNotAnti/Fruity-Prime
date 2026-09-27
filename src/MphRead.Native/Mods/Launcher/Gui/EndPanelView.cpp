#include "EndPanelView.hpp"

#include "Deck.hpp"
#include "DeckButton.hpp"
#include "DeckCard.hpp"
#include "DeckTile.hpp"
#include "HunterStand.hpp"
#include "GuiTheme.hpp"
#include "Rows.hpp"
#include "UiTabs.hpp"
#include "../../EndScreen.hpp"
#include "../../HunterSuits.hpp"
#include "../../MapPick.hpp"
#include "../../../Formats/Enums.hpp"
#include "../../../NativeRuntime/System/Globalization.hpp"

#include <algorithm>
#include <cstdint>
#include <exception>
#include <iterator>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace MphRead::Mods::Launcher::Gui
{
    using namespace ::MphRead::NativeRuntime::Avalonia;
    using ::MphRead::NativeRuntime::StringSplit;

    EndPanelView::EndPanelView()
        : _hunters(HunterStand::Names),
          _tabs(std::make_shared<UiTabs>(std::vector<std::string>{"Vote map", "Change hunter"})),
          _ballot(std::make_shared<DeckGrid>()),
          _ballotScroll(std::make_shared<Controls::ScrollViewer>()),
          _hunterPane(std::make_shared<Controls::StackPanel>()),
          _stand(std::make_shared<HunterStand>()),
          _ready(std::make_shared<DeckButton>("READY", Deck::Face::Slate(), 1.25, 1.3, 0.45, 5)),
          _count(std::make_shared<Note>("")),
          _empty(std::make_shared<Note>("The rotation decides where next."))
    {
        Background(Media::Brushes::Transparent());
        Focusable(true);
        IsHitTestVisible(true);

        _ballot->FixedColumns = 2;
        _ballot->Ratio = 16 / 9.0;
        _tabs->Changed += [this](UiTabs&) { ShowFace(); };

        _ballotScroll->Content(_ballot);
        _ballotScroll->ClipToBounds(true);
        _ballotScroll->HorizontalScrollBarVisibility(Controls::ScrollBarVisibility::Disabled);
        _ballotScroll->VerticalScrollBarVisibility(Controls::ScrollBarVisibility::Auto);

        _stand->Height(150);
        _stand->HorizontalAlignment(Layout::HorizontalAlignment::Stretch);
        _hunter = std::make_shared<ChoiceRow>("Hunter",
            std::vector<std::string>(_hunters.begin(), _hunters.end()), HunterIndex());
        _suit = std::make_shared<ChoiceRow>("Suit",
            std::vector<std::string>{"1", "2", "3", "4"},
            std::clamp(::MphRead::Mods::EndScreen::Suit(), 0, 3));
        _suit->Preview([this](Media::DrawingContext& context, Rect area)
        {
            const Media::IBrushPtr brush
                = std::make_shared<Media::SolidColorBrush>(SuitColour());
            context.DrawRectangle(brush, nullptr, RoundedRect(area, 3));
        });
        _hunter->Changed += [this](ChoiceRow&) { Commit(); };
        _suit->Changed += [this](ChoiceRow&) { Commit(); };
        _hunterPane->Spacing(8);
        _hunterPane->Children.Add(_stand);
        _hunterPane->Children.Add(_hunter);
        _hunterPane->Children.Add(_suit);
        _hunterPane->IsVisible(false);

        _ready->Click += [this](DeckButton&)
        {
            ::MphRead::Mods::EndScreen::ToggleReady();
            Refresh();
        };

        _empty->HorizontalAlignment(Layout::HorizontalAlignment::Center);
        _empty->VerticalAlignment(Layout::VerticalAlignment::Center);
        _empty->IsVisible(false);

        auto body = std::make_shared<Controls::Panel>();
        body->Children.Add(_ballotScroll);
        body->Children.Add(_empty);
        body->Children.Add(_hunterPane);

        auto foot = std::make_shared<Controls::Grid>();
        foot->ColumnDefinitions(Controls::ColumnDefinitions("*,Auto"));
        _count->VerticalAlignment(Layout::VerticalAlignment::Center);
        Controls::Grid::SetColumn(*_count, 0);
        foot->Children.Add(_count);
        Controls::Grid::SetColumn(*_ready, 1);
        foot->Children.Add(_ready);

        auto stack = std::make_shared<Controls::Grid>();
        stack->RowDefinitions(Controls::RowDefinitions("Auto,*,Auto"));
        stack->RowSpacing(8);
        Controls::Grid::SetRow(*_tabs, 0);
        stack->Children.Add(_tabs);
        Controls::Grid::SetRow(*body, 1);
        stack->Children.Add(body);
        Controls::Grid::SetRow(*foot, 2);
        stack->Children.Add(foot);

        auto card = std::make_shared<DeckCard>();
        card->Child(stack);
        card->MaxWidthEms = 22;
        card->Fill = true;
        card->VerticalAlignment(Layout::VerticalAlignment::Stretch);

        auto root = std::make_shared<Controls::Panel>();
        auto host = std::make_shared<Controls::Border>();
        host->Child(card);
        host->Width(Deck::Phone() ? 285 : 340);
        host->HorizontalAlignment(Layout::HorizontalAlignment::Right);
        host->VerticalAlignment(Layout::VerticalAlignment::Stretch);
        host->Margin(Thickness(0, 14, 14, 14));
        root->Children.Add(host);
        GuiTheme::PixelPerfect(*root);
        Content(root);
        ShowFace();
        Refresh();
    }

    void EndPanelView::ShowHunter()
    {
        _tabs->Index(1);
    }

    std::int32_t EndPanelView::HunterIndex() const
    {
        const std::string current = ::MphRead::ToString(::MphRead::Mods::EndScreen::Hunter());
        const auto found = std::find(_hunters.begin(), _hunters.end(), current);
        return found == _hunters.end() ? 0
            : static_cast<std::int32_t>(std::distance(_hunters.begin(), found));
    }

    Media::Color EndPanelView::SuitColour() const
    {
        try
        {
            const ::MphRead::ColorRgba sampled = ::MphRead::Mods::HunterSuits::Color(
                ::MphRead::Mods::EndScreen::Hunter(), std::clamp(_suit->Index(), 0, 3));
            return Media::Color::FromRgb(sampled.Red, sampled.Green, sampled.Blue);
        }
        catch (const std::exception&)
        {
            return GuiTheme::Accent;
        }
    }

    void EndPanelView::ShowFace()
    {
        const std::int32_t face = _tabs->Index();
        _ballotScroll->IsVisible(face == 0);
        _hunterPane->IsVisible(face == 1);
        // The hidden pane retains its old arranged bounds. The engine paints
        // the real hunter into the stand's bounds, so hide the stand too.
        _stand->IsVisible(face == 1);
        const std::vector<std::string> order = ::MphRead::Mods::MapPick::Order();
        _empty->IsVisible(face == 0 && order.empty());
    }

    void EndPanelView::Commit()
    {
        ::MphRead::Hunter which{};
        if (!::MphRead::TryParse(_hunter->Value(), true, which))
        {
            return;
        }
        const std::int32_t suit = std::clamp(_suit->Index(), 0, 3);
        ::MphRead::Mods::EndScreen::Pick(which, suit);
        _stand->Name2(_hunter->Value());
        _stand->Suit(suit);
        _suit->InvalidateVisual();
    }

    void EndPanelView::Refresh()
    {
        // Take the same snapshot the C# ToArray makes before enumerating a
        // ballot whose order may be updated by the session.
        const std::vector<std::string> order = ::MphRead::Mods::MapPick::Order();
        std::string key;
        for (std::size_t i = 0; i < order.size(); i++)
        {
            if (i > 0)
            {
                key += '|';
            }
            key += order[i];
        }
        if (key != _order)
        {
            _order = key;
            _ballot->Children.Clear();
            for (const std::string& room : order)
            {
                const std::vector<std::string> parts = StringSplit(room, ' ', true);
                const std::string code = parts.empty() ? room : parts.front();
                auto tile = std::make_shared<DeckTile>(room, code);
                tile->Blurb = ::MphRead::Mods::MapPick::NameOf(room);
                tile->Verb.clear();
                tile->ChosenVerb.clear();
                tile->Ratio = 16 / 9.0;
                const std::weak_ptr<DeckTile> weakTile = tile;
                tile->Click += [this, weakTile](DeckTile&)
                {
                    if (const std::shared_ptr<DeckTile> selected = weakTile.lock())
                    {
                        ::MphRead::Mods::MapPick::Choose(
                            ::MphRead::Mods::MapPick::IndexOf(selected->RoomKey));
                        Refresh();
                    }
                };
                _ballot->Children.Add(tile);
            }
        }
        _empty->IsVisible(order.empty() && _tabs->Index() == 0);
        std::int32_t best = 0;
        for (const std::string& room : order)
        {
            best = std::max(best, ::MphRead::Mods::MapPick::VotesFor(room));
        }
        for (const Controls::ControlPtr& child : _ballot->Children)
        {
            const std::shared_ptr<DeckTile> tile = std::dynamic_pointer_cast<DeckTile>(child);
            if (tile == nullptr)
            {
                continue;
            }
            const std::int32_t votes = ::MphRead::Mods::MapPick::VotesFor(tile->RoomKey);
            const bool leader = best > 0 && votes == best;
            if (tile->Tally != votes || tile->Leader != leader)
            {
                tile->Tally = votes;
                tile->Leader = leader;
                tile->InvalidateVisual();
            }
            tile->Chosen(tile->RoomKey == ::MphRead::Mods::MapPick::Picked());
        }

        const std::int32_t wantHunter = HunterIndex();
        if (_hunter->Index() != wantHunter)
        {
            _hunter->Index(wantHunter);
        }
        const std::int32_t wantSuit = std::clamp(::MphRead::Mods::EndScreen::Suit(), 0, 3);
        if (_suit->Index() != wantSuit)
        {
            _suit->Index(wantSuit);
        }
        _stand->Name2(_hunter->Value());
        _stand->Suit(wantSuit);

        const bool ready = ::MphRead::Mods::EndScreen::Ready();
        _ready->Wear(ready ? Deck::Face::Moss() : Deck::Face::Slate(), false);
        _count->Text(::MphRead::Mods::MapPick::Eligible() > 1
            ? std::to_string(::MphRead::Mods::MapPick::Eligible()) + " in the room"
            : "");
    }
}
