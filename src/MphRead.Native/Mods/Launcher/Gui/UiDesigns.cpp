#include "UiDesigns.hpp"

#include "GuiTheme.hpp"
#include "Rows.hpp"
#include "ServerRow.hpp"
#include "SliderRow.hpp"
#include "UiLayout.hpp"
#include "UiList.hpp"
#include "UiMark.hpp"
#include "UiTabs.hpp"
#include "UiWord.hpp"
#include "../../../Formats/Formats.hpp"
#include "../../Network/NetStatus.hpp"
#include "../../ThumbnailGenerator.hpp"
#include "../../../NativeRuntime/System/Exceptions.hpp"
#include "../../../NativeRuntime/System/Globalization.hpp"
#include "../../../NativeRuntime/System/IO.hpp"
#include "../../../NativeRuntime/System/Runtime.hpp"

#include <array>
#include <cmath>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace MphRead::Mods::Launcher::Gui
{
    namespace Runtime = ::MphRead::NativeRuntime;
    namespace Network = ::MphRead::Mods::Network;
    namespace Controls = ::MphRead::NativeRuntime::Avalonia::Controls;
    namespace Layout = ::MphRead::NativeRuntime::Avalonia::Layout;
    namespace Media = ::MphRead::NativeRuntime::Avalonia::Media;
    namespace Imaging = ::MphRead::NativeRuntime::Avalonia::Media::Imaging;
    using namespace ::MphRead::NativeRuntime::Avalonia;

    namespace
    {
        constexpr UiCaptureSize CaptureSize{1280.0, 720.0};

        struct Room final
        {
            std::string_view Title;
            std::string_view Key;
        };

        struct SampleServer final
        {
            std::string_view Name;
            std::string_view Room;
            ::MphRead::GameMode Mode;
            std::int32_t Players;
            std::int32_t Ping;
        };

        const std::vector<std::string> PlayTabs{"Online", "Offline", "Story", "Clips"};
        const std::vector<std::string> SettingsTabs{"Game", "Controls", "Player"};
        const std::vector<std::string> PauseEntries{
            "Resume", "Vote map", "Spectate", "Fullscreen",
            "Record demo", "Settings", "Leave match", "Quit"
        };
        constexpr std::string_view MatchText = "online match -- MP3 PROVING GROUND";
        constexpr std::string_view RoomKey = "MP3 PROVING GROUND";
        constexpr std::string_view ServerEndpoint = "203.0.113.7:27888";

        const std::array<Room, 12> Rooms{{
            {"Combat Hall", "MP3 PROVING GROUND"},
            {"High Ground", "MP4 HIGHGROUND"},
            {"Elder Passage", "MP4 HIGHGROUND - EXPANDED"},
            {"Compression Chamber", "MP5 FUEL SLUICE"},
            {"Head Shot", "MP6 HEADSHOT"},
            {"Processor Core", "MP7 PROCESSOR CORE"},
            {"Weapons Complex", "MP8 FIRE CONTROL"},
            {"Ice Hive", "MP9 CRYOCHASM"},
            {"Test Arena", "TEST ARENA"},
            {"VDO Gateway", "UNIT 3 VESPER STARPORT"},
            {"Arcterra Gateway", "UNIT 4 ARCTERRA BASE"},
            {"Sanctorus", "MP1 SANCTORUS"}
        }};

        const std::array<Room, 6> MoreRooms{{
            {"High Ground", "MP4 HIGHGROUND"},
            {"Elder Passage", "MP4 HIGHGROUND - EXPANDED"},
            {"Compression Chamber", "MP5 FUEL SLUICE"},
            {"Head Shot", "MP6 HEADSHOT"},
            {"Processor Core", "MP7 PROCESSOR CORE"},
            {"Weapons Complex", "MP8 FIRE CONTROL"}
        }};

        const std::array<SampleServer, 5> Servers{{
            {"net.livetek.fr", "MP3 PROVING GROUND", ::MphRead::GameMode::Battle, 3, 41},
            {"Fruity Prime - West Europe", "UNIT 3 VESPER STARPORT", ::MphRead::GameMode::Battle, 0, 39},
            {"Fruity Prime - West US 2", "MP10 OVERLOAD", ::MphRead::GameMode::Bounty, 5, 150},
            {"Fruity Prime - Japan", "UNIT1 ALINOS LANDFALL", ::MphRead::GameMode::PrimeHunter, 8, 251},
            {"raspberrypi", "MP2 HARVESTER", ::MphRead::GameMode::Battle, 1, 2}
        }};

        [[nodiscard]] Media::IBrushPtr Solid(Media::Color colour)
        {
            return std::make_shared<Media::SolidColorBrush>(colour);
        }

        [[nodiscard]] std::shared_ptr<Imaging::Bitmap> RoomPicture(std::string_view room)
        {
            try
            {
                const std::string path = ::MphRead::Mods::ThumbnailGenerator::PathFor(std::string(room));
                if (!Runtime::FileExists(path))
                {
                    return nullptr;
                }
                // Match `new Bitmap(new MemoryStream(File.ReadAllBytes(path)))`:
                // decode owned bytes, so the thumbnail is not held open.
                return Imaging::Bitmap::FromBytes(Runtime::FileReadAllBytes(path));
            }
            catch (...)
            {
                // No game files, or no preview rendered yet.
                return nullptr;
            }
        }

        [[nodiscard]] std::shared_ptr<Controls::Image> ImageOf(
            const std::shared_ptr<Imaging::Bitmap>& bitmap)
        {
            auto image = std::make_shared<Controls::Image>();
            image->Source(bitmap);
            image->Stretch(Media::Stretch::UniformToFill);
            return image;
        }

        [[nodiscard]] std::shared_ptr<Controls::TextBlock> Text(
            std::string value, double size, Media::IBrushPtr foreground,
            bool black = false)
        {
            auto text = std::make_shared<Controls::TextBlock>();
            text->Text(std::move(value));
            text->FontFamily(GuiTheme::Display());
            text->FontSize(size);
            if (black)
            {
                text->FontWeight(Media::FontWeight::Black);
            }
            text->Foreground(std::move(foreground));
            return text;
        }

        [[nodiscard]] std::shared_ptr<UiWord> Word(std::string text,
            double size = UiLayout::WordSize,
            std::optional<Media::Color> colour = std::nullopt,
            Media::FontFamilyPtr font = nullptr)
        {
            return std::make_shared<UiWord>(text, size, std::move(font), colour);
        }

        [[nodiscard]] std::shared_ptr<Controls::TextBlock> Footer(
            std::string_view text, double left)
        {
            auto footer = Text(std::string(text), 12, GuiTheme::TextDimBrush);
            footer->HorizontalAlignment(Layout::HorizontalAlignment::Left);
            footer->VerticalAlignment(Layout::VerticalAlignment::Bottom);
            footer->Margin(Thickness(left, 0, 0, UiLayout::FooterBottom));
            return footer;
        }

        [[nodiscard]] std::shared_ptr<UiMark> Corner(UiMark::Shape shape,
            const std::string& label)
        {
            auto mark = std::make_shared<UiMark>(shape, label);
            mark->HorizontalAlignment(shape == UiMark::Shape::Accept
                ? Layout::HorizontalAlignment::Right : Layout::HorizontalAlignment::Left);
            mark->VerticalAlignment(Layout::VerticalAlignment::Bottom);
            mark->Margin(shape == UiMark::Shape::Accept
                ? Thickness(0, 0, UiLayout::CornerX, UiLayout::CornerY)
                : Thickness(UiLayout::CornerX, 0, 0, UiLayout::CornerY));
            return mark;
        }

        [[nodiscard]] std::shared_ptr<Controls::Panel> MatchBehind()
        {
            auto root = std::make_shared<Controls::Panel>();
            const std::shared_ptr<Imaging::Bitmap> picture = RoomPicture(RoomKey);
            if (picture != nullptr)
            {
                root->Children.Add(ImageOf(picture));
            }
            auto scrim = std::make_shared<Controls::Border>();
            scrim->Background(GuiTheme::ScrimBrush);
            root->Children.Add(scrim);
            return root;
        }

        [[nodiscard]] std::shared_ptr<Controls::Panel> Backdrop()
        {
            return UiLayout::Backdrop(false, UiLayout::BackdropWash::None);
        }

        [[nodiscard]] std::shared_ptr<Controls::Panel> Body(
            std::string heading, const std::vector<std::string>* tabs,
            std::int32_t tab, const Controls::ControlPtr& content)
        {
            auto root = Backdrop();
            root->Children.Add(UiLayout::Heading(heading));
            if (tabs != nullptr)
            {
                auto strip = std::make_shared<UiTabs>(*tabs, tab);
                strip->HorizontalAlignment(Layout::HorizontalAlignment::Left);
                strip->VerticalAlignment(Layout::VerticalAlignment::Top);
                strip->Margin(UiLayout::TabMargin());
                root->Children.Add(strip);
            }
            content->Margin(UiLayout::BodyMargin());
            root->Children.Add(content);
            return root;
        }

        [[nodiscard]] Controls::ControlPtr ListAndSide(
            const Controls::ControlPtr& list, const Controls::ControlPtr& preview,
            const Controls::ControlPtr& options)
        {
            auto grid = std::make_shared<Controls::Grid>();
            grid->ColumnDefinitions(Controls::ColumnDefinitions("*,Auto"));
            Controls::Grid::SetColumn(*list, 0);
            grid->Children.Add(list);
            auto column = std::make_shared<Controls::StackPanel>();
            column->Spacing(2);
            column->Width(300);
            if (preview != nullptr)
            {
                column->Children.Add(preview);
            }
            column->Children.Add(options);
            auto side = std::make_shared<Controls::ScrollViewer>();
            side->Content(column);
            side->Margin(Thickness(30, 0, 0, 0));
            side->HorizontalScrollBarVisibility(Controls::ScrollBarVisibility::Disabled);
            side->VerticalScrollBarVisibility(Controls::ScrollBarVisibility::Auto);
            Controls::Grid::SetColumn(*side, 1);
            grid->Children.Add(side);
            return grid;
        }

        [[nodiscard]] Controls::ControlPtr MakeListAndOptionsSplit(
            const Controls::ControlPtr& list, const Controls::ControlPtr& preview,
            const Controls::ControlPtr& options, double width = 320, double gap = 34)
        {
            auto grid = std::make_shared<Controls::Grid>();
            grid->ColumnDefinitions(Controls::ColumnDefinitions("*,Auto"));
            Controls::Grid::SetColumn(*list, 0);
            grid->Children.Add(list);
            auto column = std::make_shared<Controls::StackPanel>();
            column->Spacing(4);
            column->Width(width);
            if (preview != nullptr)
            {
                column->Children.Add(preview);
            }
            column->Children.Add(options);
            auto side = std::make_shared<Controls::ScrollViewer>();
            side->Content(column);
            side->Margin(Thickness(gap, 0, 0, 0));
            side->HorizontalScrollBarVisibility(Controls::ScrollBarVisibility::Disabled);
            side->VerticalScrollBarVisibility(Controls::ScrollBarVisibility::Auto);
            Controls::Grid::SetColumn(*side, 1);
            grid->Children.Add(side);
            return grid;
        }

        [[nodiscard]] std::shared_ptr<UiTabs> Nav(
            const std::vector<std::string>& items, std::int32_t index)
        {
            auto tabs = std::make_shared<UiTabs>(items, index);
            tabs->HorizontalAlignment(Layout::HorizontalAlignment::Left);
            return tabs;
        }

        [[nodiscard]] std::shared_ptr<UiList> MapList()
        {
            auto list = std::make_shared<UiList>();
            for (const Room& room : Rooms)
            {
                auto row = std::make_shared<UiListRow>(std::string(room.Title), std::string(room.Key));
                row->Choice = std::string(room.Key);
                list->Add(row);
            }
            list->FocusFirst();
            return list;
        }

        [[nodiscard]] std::shared_ptr<ServerRow> Server(std::size_t index)
        {
            const SampleServer& sample = Servers.at(index);
            auto row = std::make_shared<ServerRow>(std::string(sample.Name), std::string(ServerEndpoint));
            Network::ServerStatus status;
            status.Online = true;
            status.RoomKey = std::string(sample.Room);
            status.Mode = sample.Mode;
            status.Players = sample.Players;
            status.MaxPlayers = 8;
            status.Latency = sample.Ping;
            row->SetStatus(status);
            return row;
        }

        [[nodiscard]] std::shared_ptr<UiList> ServerTable(bool compact = false)
        {
            (void)compact; // The C# parameter is intentionally unused as well.
            auto list = std::make_shared<UiList>();
            for (std::size_t i = 0; i < Servers.size(); ++i)
            {
                list->Add(Server(i));
            }
            list->FocusFirst();
            return list;
        }

        [[nodiscard]] Controls::ControlPtr OfflineOptions(double width = std::nan(""))
        {
            auto stack = std::make_shared<Controls::StackPanel>();
            stack->Spacing(2);
            if (!std::isnan(width))
            {
                stack->Width(width);
            }
            stack->Children.Add(std::make_shared<ChoiceRow>("Where", std::vector<std::string>{"Local", "Online"}, 0));
            stack->Children.Add(std::make_shared<ChoiceRow>("Match type",
                std::vector<std::string>{"Battle", "Battle teams", "Survival", "Bounty"}, 0));
            stack->Children.Add(std::make_shared<ChoiceRow>("Hunter",
                std::vector<std::string>{"Samus", "Kanden", "Trace", "Sylux", "Noxus", "Spire", "Weavel"}, 0));
            stack->Children.Add(std::make_shared<ChoiceRow>("Bots",
                std::vector<std::string>{"0", "1", "2", "3"}, 3));
            stack->Children.Add(std::make_shared<ChoiceRow>("Bot skill",
                std::vector<std::string>{"Easy", "Normal", "Hard", "Insane"}, 1));
            return stack;
        }

        [[nodiscard]] Controls::ControlPtr OnlineOptions(double width = std::nan(""))
        {
            auto stack = std::make_shared<Controls::StackPanel>();
            stack->Spacing(2);
            if (!std::isnan(width))
            {
                stack->Width(width);
            }
            stack->Children.Add(std::make_shared<FieldRow>("Name", "Livetek", 150));
            stack->Children.Add(std::make_shared<ChoiceRow>("Hunter",
                std::vector<std::string>{"Samus", "Kanden", "Trace", "Sylux", "Noxus", "Spire", "Weavel"}, 3));
            stack->Children.Add(std::make_shared<FieldRow>("Address", "89.160.162.50:27888", 170));
            return stack;
        }

        [[nodiscard]] Controls::ControlPtr SettingRows(double width)
        {
            auto stack = std::make_shared<Controls::StackPanel>();
            stack->Spacing(2);
            if (!std::isnan(width))
            {
                stack->Width(width);
            }
            stack->Children.Add(std::make_shared<Caption>("window"));
            stack->Children.Add(std::make_shared<ChoiceRow>("Mode",
                std::vector<std::string>{"Windowed", "Fullscreen", "Borderless"}, 0));
            stack->Children.Add(std::make_shared<Caption>("view"));
            stack->Children.Add(std::make_shared<SliderRow>("Field of view", 78,
                [](std::int32_t value)
                {
                    return std::to_string(value) + "°" + (value == 78 ? " (DS)" : "");
                }, 130, 60, 120));
            stack->Children.Add(std::make_shared<SliderRow>("Render scale", 100,
                [](std::int32_t value) { return std::to_string(value) + "%"; }, 130, 50, 100));
            stack->Children.Add(std::make_shared<ToggleRow>("Lighting", true));
            stack->Children.Add(std::make_shared<ToggleRow>("Fog", true));
            stack->Children.Add(std::make_shared<ToggleRow>("FPS counter", false));
            stack->Children.Add(std::make_shared<Caption>("hud"));
            stack->Children.Add(std::make_shared<ToggleRow>("Pro mode HUD", true));
            stack->Children.Add(std::make_shared<ChoiceRow>("Crosshair size",
                std::vector<std::string>{"Small", "Medium", "Big"}, 1));
            stack->Children.Add(std::make_shared<ChoiceRow>("Crosshair type",
                std::vector<std::string>{"Cross", "Dot", "CrossDot", "Circle", "Brackets"}, 0));
            stack->Children.Add(std::make_shared<Caption>("match"));
            stack->Children.Add(std::make_shared<ChoiceRow>("Weapon",
                std::vector<std::string>{"Dynamic (DS)", "Static (Quake)"}, 0));
            stack->Children.Add(std::make_shared<ToggleRow>("Chat", true));
            return stack;
        }

        [[nodiscard]] Controls::ControlPtr Preview(double height,
            double width = std::nan(""))
        {
            auto box = std::make_shared<Controls::Border>();
            box->Height(height);
            box->CornerRadius(CornerRadius(3));
            box->ClipToBounds(true);
            box->Margin(Thickness(0, 0, 0, 10));
            auto image = ImageOf(RoomPicture(RoomKey));
            box->Child(image);
            if (!std::isnan(width))
            {
                box->Width(width);
            }
            return box;
        }

        class Design
        {
        public:
            virtual ~Design() = default;
            [[nodiscard]] virtual std::string_view Id() const = 0;
            [[nodiscard]] virtual std::string_view Title() const = 0;
            [[nodiscard]] virtual Controls::ControlPtr PlayOffline() const = 0;
            [[nodiscard]] virtual Controls::ControlPtr PlayOnline() const = 0;
            [[nodiscard]] virtual Controls::ControlPtr Settings() const = 0;
            [[nodiscard]] virtual Controls::ControlPtr Pause() const = 0;

            [[nodiscard]] Controls::ControlPtr Screen(std::size_t index) const
            {
                switch (index)
                {
                    case 0: return PlayOffline();
                    case 1: return PlayOnline();
                    case 2: return Settings();
                    default: return Pause();
                }
            }
        };

        class CurrentDesign final : public Design
        {
        public:
            std::string_view Id() const override { return "a-current"; }
            std::string_view Title() const override
            {
                return "what F replaced: list left, preview and options right, marks in the bottom corners";
            }

            Controls::ControlPtr PlayOffline() const override
            {
                auto root = Body("play", &PlayTabs, 1,
                    ListAndSide(MapList(), Preview(172), OfflineOptions()));
                root->Children.Add(Corner(UiMark::Shape::Cancel, "back"));
                root->Children.Add(Corner(UiMark::Shape::Accept, "start"));
                return root;
            }

            Controls::ControlPtr PlayOnline() const override
            {
                auto root = Body("play", &PlayTabs, 0,
                    ListAndSide(ServerTable(), Preview(172), OnlineOptions()));
                root->Children.Add(Corner(UiMark::Shape::Cancel, "back"));
                root->Children.Add(Corner(UiMark::Shape::Accept, "join"));
                return root;
            }

            Controls::ControlPtr Settings() const override
            {
                auto scroll = std::make_shared<Controls::ScrollViewer>();
                scroll->Content(SettingRows(560));
                scroll->HorizontalScrollBarVisibility(Controls::ScrollBarVisibility::Disabled);
                scroll->VerticalScrollBarVisibility(Controls::ScrollBarVisibility::Auto);
                auto root = Body("settings", &SettingsTabs, 0, scroll);
                root->Children.Add(Corner(UiMark::Shape::Cancel, "cancel"));
                root->Children.Add(Corner(UiMark::Shape::Accept, "save"));
                return root;
            }

            Controls::ControlPtr Pause() const override
            {
                auto root = MatchBehind();
                auto menu = UiLayout::Column(12);
                for (const std::string& entry : PauseEntries)
                {
                    menu->Children.Add(Word(entry));
                }
                root->Children.Add(menu);
                root->Children.Add(Footer(MatchText, UiLayout::ColumnLeft - 50));
                return root;
            }
        };

        class SheetDesign final : public Design
        {
        public:
            std::string_view Id() const override { return "b-sheet"; }
            std::string_view Title() const override
            {
                return "a sheet of ink over the photo, ruled top and bottom, actions on a footer rail";
            }

            Controls::ControlPtr PlayOffline() const override
            {
                return Page("play", Nav(PlayTabs, 1),
                    Split(MapList(), Preview(180), OfflineOptions()), "back", "start");
            }
            Controls::ControlPtr PlayOnline() const override
            {
                return Page("play", Nav(PlayTabs, 0),
                    Split(ServerTable(), Preview(180), OnlineOptions()), "back", "join");
            }
            Controls::ControlPtr Settings() const override
            {
                auto scroll = std::make_shared<Controls::ScrollViewer>();
                scroll->Content(SettingRows(620));
                scroll->HorizontalScrollBarVisibility(Controls::ScrollBarVisibility::Disabled);
                scroll->VerticalScrollBarVisibility(Controls::ScrollBarVisibility::Auto);
                return Page("settings", Nav(SettingsTabs, 0), scroll, "cancel", "save");
            }
            Controls::ControlPtr Pause() const override
            {
                auto menu = std::make_shared<Controls::StackPanel>();
                menu->Spacing(11);
                menu->Width(320);
                for (const std::string& entry : PauseEntries)
                {
                    menu->Children.Add(Word(entry));
                }
                auto holder = std::make_shared<Controls::Grid>();
                holder->ColumnDefinitions(Controls::ColumnDefinitions("Auto,*"));
                Controls::Grid::SetColumn(*menu, 0);
                holder->Children.Add(menu);
                auto note = Text(std::string(MatchText), 12, GuiTheme::TextDimBrush);
                note->HorizontalAlignment(Layout::HorizontalAlignment::Right);
                note->VerticalAlignment(Layout::VerticalAlignment::Bottom);
                Controls::Grid::SetColumn(*note, 1);
                holder->Children.Add(note);
                return Page("paused", nullptr, holder, "leave match", "resume", true);
            }

        private:
            static constexpr double Pad = 56;

            static std::shared_ptr<Controls::Panel> Page(std::string heading,
                const Controls::ControlPtr& nav, const Controls::ControlPtr& content,
                std::string cancel, std::string accept, bool overGame = false)
            {
                auto root = overGame ? MatchBehind() : Backdrop();
                auto sheet = std::make_shared<Controls::Grid>();
                sheet->RowDefinitions(Controls::RowDefinitions("Auto,*,Auto"));
                sheet->Margin(Thickness(0, 44, 0, 44));

                auto head = std::make_shared<Controls::StackPanel>();
                head->Spacing(0);
                head->Margin(Thickness(Pad, 22, Pad, 14));
                auto headingText = Text(Runtime::ToUpperInvariant(heading), 19, Solid(GuiTheme::Text), true);
                headingText->Margin(Thickness(0, 0, 0, nav == nullptr ? 0 : 12));
                head->Children.Add(headingText);
                if (nav != nullptr)
                {
                    head->Children.Add(nav);
                }
                Controls::Grid::SetRow(*head, 0);
                sheet->Children.Add(head);

                content->Margin(Thickness(Pad, 0, Pad, 0));
                auto body = std::make_shared<Controls::Border>();
                body->BorderBrush(GuiTheme::EdgeBrush);
                body->BorderThickness(Thickness(0, 1, 0, 1));
                body->Padding(Thickness(0, 18, 0, 18));
                body->Child(content);
                Controls::Grid::SetRow(*body, 1);
                sheet->Children.Add(body);

                auto rail = std::make_shared<Controls::Grid>();
                rail->ColumnDefinitions(Controls::ColumnDefinitions("Auto,*,Auto"));
                rail->Margin(Thickness(Pad, 16, Pad, 20));
                auto back = std::make_shared<UiMark>(UiMark::Shape::Cancel, cancel);
                Controls::Grid::SetColumn(*back, 0);
                rail->Children.Add(back);
                auto go = std::make_shared<UiMark>(UiMark::Shape::Accept, accept);
                go->HorizontalAlignment(Layout::HorizontalAlignment::Right);
                Controls::Grid::SetColumn(*go, 2);
                rail->Children.Add(go);
                Controls::Grid::SetRow(*rail, 2);
                sheet->Children.Add(rail);

                auto ground = std::make_shared<Controls::Border>();
                ground->Background(Solid(Media::Color::FromArgb(
                    static_cast<std::uint8_t>(overGame ? 242 : 235),
                    GuiTheme::Ink.R, GuiTheme::Ink.G, GuiTheme::Ink.B)));
                ground->Child(sheet);
                root->Children.Add(ground);
                return root;
            }

            static Controls::ControlPtr Split(const Controls::ControlPtr& list,
                const Controls::ControlPtr& preview, const Controls::ControlPtr& options)
            {
                return MakeListAndOptionsSplit(list, preview, options, 320, 34);
            }
        };

        [[nodiscard]] std::shared_ptr<Controls::Border> SplitColumnWash(double columnWidth)
        {
            auto brush = std::make_shared<Media::LinearGradientBrush>();
            brush->StartPoint = RelativePoint(0, 0, RelativeUnit::Relative);
            brush->EndPoint = RelativePoint(1, 0, RelativeUnit::Relative);
            const double solidEnd = columnWidth / (columnWidth + 70);
            brush->GradientStops = {
                Media::GradientStop(Media::Color::FromArgb(252, 10, 12, 16), 0),
                Media::GradientStop(Media::Color::FromArgb(252, 10, 12, 16), solidEnd),
                Media::GradientStop(Media::Color::FromArgb(0, 10, 12, 16), 1)
            };
            auto wash = std::make_shared<Controls::Border>();
            wash->Width(columnWidth + 70);
            wash->HorizontalAlignment(Layout::HorizontalAlignment::Left);
            wash->Background(brush);
            return wash;
        }

        [[nodiscard]] std::shared_ptr<Controls::Grid> SplitColumn(
            std::string heading, const Controls::ControlPtr& nav,
            const Controls::ControlPtr& content, std::string cancel, std::string accept,
            double pad, double rightPad)
        {
            auto grid = std::make_shared<Controls::Grid>();
            grid->RowDefinitions(Controls::RowDefinitions("Auto,Auto,*,Auto"));
            grid->Margin(Thickness(pad, 40, rightPad, 34));
            auto title = Text(Runtime::ToUpperInvariant(heading), 18, Solid(GuiTheme::Text), true);
            title->Margin(Thickness(0, 0, 0, nav == nullptr ? 16 : 12));
            Controls::Grid::SetRow(*title, 0);
            grid->Children.Add(title);
            if (nav != nullptr)
            {
                nav->Margin(Thickness(0, 0, 0, 18));
                Controls::Grid::SetRow(*nav, 1);
                grid->Children.Add(nav);
            }
            Controls::Grid::SetRow(*content, 2);
            grid->Children.Add(content);

            auto rail = std::make_shared<Controls::Grid>();
            rail->ColumnDefinitions(Controls::ColumnDefinitions("Auto,*,Auto"));
            rail->Margin(Thickness(0, 20, 0, 0));
            auto back = std::make_shared<UiMark>(UiMark::Shape::Cancel, cancel);
            Controls::Grid::SetColumn(*back, 0);
            rail->Children.Add(back);
            auto go = std::make_shared<UiMark>(UiMark::Shape::Accept, accept);
            go->HorizontalAlignment(Layout::HorizontalAlignment::Right);
            Controls::Grid::SetColumn(*go, 2);
            rail->Children.Add(go);
            Controls::Grid::SetRow(*rail, 3);
            grid->Children.Add(rail);
            return grid;
        }

        [[nodiscard]] Controls::ControlPtr NameOver(
            std::string name, std::string detail, const Controls::ControlPtr& extra,
            double pad)
        {
            auto stack = std::make_shared<Controls::StackPanel>();
            stack->Spacing(4);
            stack->HorizontalAlignment(Layout::HorizontalAlignment::Right);
            stack->VerticalAlignment(Layout::VerticalAlignment::Bottom);
            stack->Margin(Thickness(0, 0, pad, 34));
            stack->Width(360);
            auto title = Text(std::move(name), 26, Solid(GuiTheme::Text), true);
            title->TextAlignment(Media::TextAlignment::Right);
            stack->Children.Add(title);
            auto caption = Text(std::move(detail), 12.5, GuiTheme::TextDimBrush);
            caption->TextAlignment(Media::TextAlignment::Right);
            caption->Margin(Thickness(0, 0, 0, extra == nullptr ? 0 : 18));
            stack->Children.Add(caption);
            if (extra != nullptr)
            {
                stack->Children.Add(extra);
            }
            auto panel = std::make_shared<Controls::Panel>();
            auto wash = std::make_shared<Controls::Border>();
            wash->Height(320);
            wash->VerticalAlignment(Layout::VerticalAlignment::Bottom);
            auto gradient = std::make_shared<Media::LinearGradientBrush>();
            gradient->StartPoint = RelativePoint(0, 0, RelativeUnit::Relative);
            gradient->EndPoint = RelativePoint(0, 1, RelativeUnit::Relative);
            gradient->GradientStops = {
                Media::GradientStop(Media::Color::FromArgb(0, 0, 0, 0), 0),
                Media::GradientStop(Media::Color::FromArgb(225, 0, 0, 0), 1)
            };
            wash->Background(gradient);
            panel->Children.Add(wash);
            panel->Children.Add(stack);
            return panel;
        }

        class SplitDesign final : public Design
        {
        public:
            std::string_view Id() const override { return "c-split"; }
            std::string_view Title() const override
            {
                return "the chosen map fills the frame, launcher photo dropped; everything you press in a solid left column";
            }
            Controls::ControlPtr PlayOffline() const override
            {
                return Frame(BigPreview(), NameOver("Combat Hall",
                    "MP3 PROVING GROUND  ·  battle  ·  3 bots", OfflineOptions(300), Pad),
                    SplitColumn("play", Nav(PlayTabs, 1), MapList(), "back", "start", Pad, 26));
            }
            Controls::ControlPtr PlayOnline() const override
            {
                return Frame(BigPreview(), NameOver("net.livetek.fr",
                    "MP3 PROVING GROUND  ·  battle  ·  3/8  ·  41 ms", OnlineOptions(300), Pad),
                    SplitColumn("play", Nav(PlayTabs, 0), ServerTable(true), "back", "join", Pad, 26));
            }
            Controls::ControlPtr Settings() const override
            {
                auto scroll = std::make_shared<Controls::ScrollViewer>();
                scroll->Content(SettingRows(360));
                scroll->HorizontalScrollBarVisibility(Controls::ScrollBarVisibility::Disabled);
                scroll->VerticalScrollBarVisibility(Controls::ScrollBarVisibility::Auto);
                return Frame(nullptr, nullptr,
                    SplitColumn("settings", Nav(SettingsTabs, 0), scroll, "cancel", "save", Pad, 26));
            }
            Controls::ControlPtr Pause() const override
            {
                auto menu = std::make_shared<Controls::StackPanel>();
                menu->Spacing(12);
                for (const std::string& entry : PauseEntries)
                {
                    menu->Children.Add(Word(entry));
                }
                return Frame(nullptr, nullptr,
                    SplitColumn("paused", nullptr, menu, "leave match", "resume", Pad, 26), true);
            }

        private:
            static constexpr double ColumnWidth = 430;
            static constexpr double Pad = 44;

            static std::shared_ptr<Controls::Image> BigPreview()
            {
                const std::shared_ptr<Imaging::Bitmap> picture = RoomPicture(RoomKey);
                return picture == nullptr ? nullptr : ImageOf(picture);
            }

            static std::shared_ptr<Controls::Panel> Frame(
                const Controls::ControlPtr& rightPicture,
                const Controls::ControlPtr& rightOverlay,
                const Controls::ControlPtr& column, bool overGame = false)
            {
                auto root = std::make_shared<Controls::Panel>();
                if (rightPicture != nullptr)
                {
                    root->Children.Add(rightPicture);
                }
                else if (overGame)
                {
                    root->Children.Add(MatchBehind());
                }
                else
                {
                    root->Children.Add(Backdrop());
                }
                if (rightOverlay != nullptr)
                {
                    root->Children.Add(rightOverlay);
                }
                root->Children.Add(SplitColumnWash(ColumnWidth));
                column->Width(ColumnWidth);
                column->HorizontalAlignment(Layout::HorizontalAlignment::Left);
                root->Children.Add(column);
                return root;
            }
        };

        [[nodiscard]] Controls::ControlPtr Opened(
            const Controls::ControlPtr& picture, const Controls::ControlPtr& options)
        {
            auto inner = std::make_shared<Controls::Grid>();
            inner->ColumnDefinitions(Controls::ColumnDefinitions("Auto,*"));
            if (picture != nullptr)
            {
                picture->Margin(Thickness(0, 0, 28, 0));
                Controls::Grid::SetColumn(*picture, 0);
                inner->Children.Add(picture);
            }
            Controls::Grid::SetColumn(*options, 1);
            inner->Children.Add(options);
            auto border = std::make_shared<Controls::Border>();
            border->BorderBrush(GuiTheme::AccentBrush);
            border->BorderThickness(Thickness(2, 0, 0, 0));
            border->Padding(Thickness(22, 12, 0, 18));
            border->Margin(Thickness(0, 2, 0, 10));
            border->Child(inner);
            return border;
        }

        [[nodiscard]] Controls::ControlPtr Chosen(std::string title, std::string detail)
        {
            auto grid = std::make_shared<Controls::Grid>();
            grid->ColumnDefinitions(Controls::ColumnDefinitions("Auto,*,Auto"));
            grid->Height(32);
            auto caret = std::make_shared<Controls::Border>();
            caret->Width(3);
            caret->Background(GuiTheme::AccentBrush);
            caret->Margin(Thickness(0, 6, 11, 6));
            Controls::Grid::SetColumn(*caret, 0);
            grid->Children.Add(caret);
            auto name = Text(std::move(title), 14, GuiTheme::AccentBrush, true);
            name->VerticalAlignment(Layout::VerticalAlignment::Center);
            Controls::Grid::SetColumn(*name, 1);
            grid->Children.Add(name);
            auto key = Text(std::move(detail), 12, GuiTheme::TextDimBrush);
            key->VerticalAlignment(Layout::VerticalAlignment::Center);
            Controls::Grid::SetColumn(*key, 2);
            grid->Children.Add(key);
            return grid;
        }

        class StackDesign final : public Design
        {
        public:
            std::string_view Id() const override { return "d-stack"; }
            std::string_view Title() const override
            {
                return "one full-width column, the chosen row opens in place -- the only one that also works in portrait";
            }
            Controls::ControlPtr PlayOffline() const override
            {
                auto stack = std::make_shared<Controls::StackPanel>();
                stack->Spacing(1);
                stack->Children.Add(Chosen("Combat Hall", "MP3 PROVING GROUND"));
                auto preview = Preview(150, 266);
                preview->Margin(Thickness(0));
                stack->Children.Add(Opened(preview, OfflineOptions(320)));
                for (const Room& room : MoreRooms)
                {
                    stack->Children.Add(std::make_shared<UiListRow>(std::string(room.Title), std::string(room.Key)));
                }
                auto scroll = std::make_shared<Controls::ScrollViewer>();
                scroll->Content(stack);
                scroll->HorizontalScrollBarVisibility(Controls::ScrollBarVisibility::Disabled);
                scroll->VerticalScrollBarVisibility(Controls::ScrollBarVisibility::Auto);
                return Page("play", Nav(PlayTabs, 1), scroll, "back", "start");
            }
            Controls::ControlPtr PlayOnline() const override
            {
                auto stack = std::make_shared<Controls::StackPanel>();
                stack->Spacing(1);
                for (std::size_t i = 0; i < Servers.size(); ++i)
                {
                    stack->Children.Add(Server(i));
                    if (i != 0)
                    {
                        continue;
                    }
                    auto strip = std::make_shared<Controls::StackPanel>();
                    strip->Orientation(Layout::Orientation::Horizontal);
                    strip->Spacing(28);
                    strip->Margin(Thickness(22, 12, 0, 14));
                    auto preview = Preview(104, 184);
                    preview->Margin(Thickness(0));
                    strip->Children.Add(preview);
                    strip->Children.Add(OnlineOptions(330));
                    auto border = std::make_shared<Controls::Border>();
                    border->BorderBrush(GuiTheme::AccentBrush);
                    border->BorderThickness(Thickness(2, 0, 0, 0));
                    border->Child(strip);
                    stack->Children.Add(border);
                }
                auto scroll = std::make_shared<Controls::ScrollViewer>();
                scroll->Content(stack);
                scroll->HorizontalScrollBarVisibility(Controls::ScrollBarVisibility::Disabled);
                scroll->VerticalScrollBarVisibility(Controls::ScrollBarVisibility::Auto);
                return Page("play", Nav(PlayTabs, 0), scroll, "back", "join");
            }
            Controls::ControlPtr Settings() const override
            {
                return Page("settings", Nav(SettingsTabs, 0), SettingRows(std::nan("")), "cancel", "save");
            }
            Controls::ControlPtr Pause() const override
            {
                auto menu = std::make_shared<Controls::StackPanel>();
                menu->Spacing(12);
                for (const std::string& entry : PauseEntries)
                {
                    menu->Children.Add(Word(entry));
                }
                return Page("paused", nullptr, menu, "leave match", "resume", true);
            }

        private:
            static constexpr double Pad = 96;

            static std::shared_ptr<Controls::Panel> Page(std::string heading,
                const Controls::ControlPtr& nav, const Controls::ControlPtr& content,
                std::string cancel, std::string accept, bool overGame = false)
            {
                auto root = overGame ? MatchBehind() : Backdrop();
                auto wash = std::make_shared<Controls::Border>();
                auto gradient = std::make_shared<Media::LinearGradientBrush>();
                gradient->StartPoint = RelativePoint(0, 0, RelativeUnit::Relative);
                gradient->EndPoint = RelativePoint(1, 0.4, RelativeUnit::Relative);
                gradient->GradientStops = {
                    Media::GradientStop(Media::Color::FromArgb(240, 10, 12, 16), 0),
                    Media::GradientStop(Media::Color::FromArgb(205, 10, 12, 16), 0.7),
                    Media::GradientStop(Media::Color::FromArgb(150, 10, 12, 16), 1)
                };
                wash->Background(gradient);
                root->Children.Add(wash);
                auto grid = std::make_shared<Controls::Grid>();
                grid->RowDefinitions(Controls::RowDefinitions("Auto,*,Auto"));
                grid->Margin(Thickness(Pad, 38, Pad, 30));
                auto head = std::make_shared<Controls::StackPanel>();
                head->Spacing(10);
                head->Margin(Thickness(0, 0, 0, 16));
                head->Children.Add(Text(Runtime::ToUpperInvariant(heading), 18, Solid(GuiTheme::Text), true));
                if (nav != nullptr)
                {
                    head->Children.Add(nav);
                }
                Controls::Grid::SetRow(*head, 0);
                grid->Children.Add(head);
                Controls::Grid::SetRow(*content, 1);
                grid->Children.Add(content);
                auto rail = std::make_shared<Controls::Grid>();
                rail->ColumnDefinitions(Controls::ColumnDefinitions("Auto,*,Auto"));
                rail->Margin(Thickness(0, 18, 0, 0));
                auto back = std::make_shared<UiMark>(UiMark::Shape::Cancel, cancel);
                Controls::Grid::SetColumn(*back, 0);
                rail->Children.Add(back);
                auto go = std::make_shared<UiMark>(UiMark::Shape::Accept, accept);
                go->HorizontalAlignment(Layout::HorizontalAlignment::Right);
                Controls::Grid::SetColumn(*go, 2);
                rail->Children.Add(go);
                Controls::Grid::SetRow(*rail, 2);
                grid->Children.Add(rail);
                root->Children.Add(grid);
                return root;
            }
        };

        class ShellDesign final : public Design
        {
        public:
            std::string_view Id() const override { return "e-shell"; }
            std::string_view Title() const override
            {
                return "a permanent left rail; Play and Settings become panes of one window, one press apart";
            }
            Controls::ControlPtr PlayOffline() const override
            {
                return Frame(0, Pane(Nav(PlayTabs, 1),
                    MakeListAndOptionsSplit(MapList(), Preview(158), OfflineOptions(), 290, 30)), "back", "start");
            }
            Controls::ControlPtr PlayOnline() const override
            {
                return Frame(0, Pane(Nav(PlayTabs, 0),
                    MakeListAndOptionsSplit(ServerTable(), Preview(158), OnlineOptions(), 290, 30)), "back", "join");
            }
            Controls::ControlPtr Settings() const override
            {
                auto scroll = std::make_shared<Controls::ScrollViewer>();
                scroll->Content(SettingRows(560));
                scroll->HorizontalScrollBarVisibility(Controls::ScrollBarVisibility::Disabled);
                scroll->VerticalScrollBarVisibility(Controls::ScrollBarVisibility::Auto);
                return Frame(1, Pane(Nav(SettingsTabs, 0), scroll), "cancel", "save");
            }
            Controls::ControlPtr Pause() const override
            {
                auto menu = std::make_shared<Controls::StackPanel>();
                menu->Spacing(13);
                for (std::string entry : {"Resume", "Vote map", "Spectate", "Fullscreen", "Record demo"})
                {
                    menu->Children.Add(Word(std::move(entry)));
                }
                auto note = Text(std::string(MatchText), 12, GuiTheme::TextDimBrush);
                note->Margin(Thickness(0, 28, 0, 0));
                menu->Children.Add(note);
                return Frame(-1, Pane(nullptr, menu), "quit", "resume", true);
            }

        private:
            static constexpr double RailWidth = 215;

            static Controls::ControlPtr Pane(const Controls::ControlPtr& nav,
                const Controls::ControlPtr& content)
            {
                auto grid = std::make_shared<Controls::Grid>();
                grid->RowDefinitions(Controls::RowDefinitions("Auto,*"));
                if (nav != nullptr)
                {
                    nav->Margin(Thickness(0, 0, 0, 18));
                    Controls::Grid::SetRow(*nav, 0);
                    grid->Children.Add(nav);
                }
                Controls::Grid::SetRow(*content, 1);
                grid->Children.Add(content);
                return grid;
            }

            static std::shared_ptr<Controls::Panel> Frame(
                std::int32_t railIndex, const Controls::ControlPtr& content,
                std::string cancel, std::string accept, bool overGame = false)
            {
                auto root = overGame ? MatchBehind() : Backdrop();
                auto veil = std::make_shared<Controls::Border>();
                veil->Background(Solid(Media::Color::FromArgb(
                    static_cast<std::uint8_t>(overGame ? 120 : 196),
                    GuiTheme::Ink.R, GuiTheme::Ink.G, GuiTheme::Ink.B)));
                root->Children.Add(veil);
                auto grid = std::make_shared<Controls::Grid>();
                grid->ColumnDefinitions(Controls::ColumnDefinitions("Auto,*"));

                auto rail = std::make_shared<Controls::StackPanel>();
                rail->Spacing(0);
                rail->Width(RailWidth);
                rail->Margin(Thickness(38, 44, 0, 0));
                auto mark = UiLayout::Wordmark();
                mark->Width(132);
                mark->HorizontalAlignment(Layout::HorizontalAlignment::Left);
                mark->VerticalAlignment(Layout::VerticalAlignment::Top);
                mark->Margin(Thickness(0, 0, 0, 42));
                rail->Children.Add(mark);
                constexpr std::array<std::string_view, 3> places{"Play", "Settings", "Profile"};
                for (std::size_t i = 0; i < places.size(); ++i)
                {
                    auto line = std::make_shared<Controls::Grid>();
                    line->ColumnDefinitions(Controls::ColumnDefinitions("Auto,*"));
                    line->Margin(Thickness(0, 0, 0, 15));
                    auto tick = std::make_shared<Controls::Border>();
                    tick->Width(2);
                    tick->Background(i == static_cast<std::size_t>(railIndex)
                        ? GuiTheme::AccentBrush : Media::Brushes::Transparent());
                    tick->Margin(Thickness(0, 2, 16, 2));
                    Controls::Grid::SetColumn(*tick, 0);
                    line->Children.Add(tick);
                    auto word = Word(std::string(places[i]), 19);
                    word->Selected(static_cast<std::int32_t>(i) == railIndex);
                    Controls::Grid::SetColumn(*word, 1);
                    line->Children.Add(word);
                    rail->Children.Add(line);
                }
                auto rule = std::make_shared<Controls::Border>();
                rule->Height(1);
                rule->Background(GuiTheme::EdgeBrush);
                rule->Margin(Thickness(18, 16, 30, 20));
                rail->Children.Add(rule);
                auto last = std::make_shared<Controls::StackPanel>();
                last->Margin(Thickness(18, 0, 0, 0));
                last->Children.Add(Word(overGame ? "Leave match" : "Quit", 16, GuiTheme::TextDim));
                rail->Children.Add(last);
                Controls::Grid::SetColumn(*rail, 0);
                grid->Children.Add(rail);

                auto pane = std::make_shared<Controls::Grid>();
                pane->RowDefinitions(Controls::RowDefinitions("*,Auto"));
                pane->Margin(Thickness(34, 44, 52, 32));
                Controls::Grid::SetRow(*content, 0);
                pane->Children.Add(content);
                auto bar = std::make_shared<Controls::Grid>();
                bar->ColumnDefinitions(Controls::ColumnDefinitions("Auto,*,Auto"));
                bar->Margin(Thickness(0, 16, 0, 0));
                auto back = std::make_shared<UiMark>(UiMark::Shape::Cancel, cancel);
                Controls::Grid::SetColumn(*back, 0);
                bar->Children.Add(back);
                auto go = std::make_shared<UiMark>(UiMark::Shape::Accept, accept);
                go->HorizontalAlignment(Layout::HorizontalAlignment::Right);
                Controls::Grid::SetColumn(*go, 2);
                bar->Children.Add(go);
                Controls::Grid::SetRow(*bar, 1);
                pane->Children.Add(bar);
                auto ground = std::make_shared<Controls::Border>();
                ground->Background(Solid(Media::Color::FromArgb(216,
                    GuiTheme::Panel.R, GuiTheme::Panel.G, GuiTheme::Panel.B)));
                ground->BorderBrush(GuiTheme::EdgeBrush);
                ground->BorderThickness(Thickness(1, 0, 0, 0));
                ground->Child(pane);
                Controls::Grid::SetColumn(*ground, 1);
                grid->Children.Add(ground);
                root->Children.Add(grid);
                return root;
            }
        };

        [[nodiscard]] std::shared_ptr<Controls::Grid> Over(
            const Controls::ControlPtr& preview, const Controls::ControlPtr& list,
            const Controls::ControlPtr& options)
        {
            auto grid = std::make_shared<Controls::Grid>();
            grid->RowDefinitions(Controls::RowDefinitions("Auto,*"));
            grid->ColumnDefinitions(Controls::ColumnDefinitions("*,Auto"));
            preview->Margin(Thickness(0, 0, 24, 16));
            Controls::Grid::SetRow(*preview, 0);
            Controls::Grid::SetColumn(*preview, 0);
            grid->Children.Add(preview);
            Controls::Grid::SetRow(*options, 0);
            Controls::Grid::SetColumn(*options, 1);
            grid->Children.Add(options);
            Controls::Grid::SetRow(*list, 1);
            Controls::Grid::SetColumnSpan(*list, 2);
            grid->Children.Add(list);
            return grid;
        }

        class CentreDesign final : public Design
        {
        public:
            std::string_view Id() const override { return "f-centre"; }
            std::string_view Title() const override
            {
                return "a centred well with the two marks together beneath it -- CHOSEN, and now what the launcher does";
            }
            Controls::ControlPtr PlayOffline() const override
            {
                return Page(840, "play", std::make_shared<UiTabs>(PlayTabs, 1),
                    Over(Preview(168, 420), MapList(), OfflineOptions(340)), "back", "start");
            }
            Controls::ControlPtr PlayOnline() const override
            {
                return Page(880, "play", std::make_shared<UiTabs>(PlayTabs, 0),
                    Over(Preview(150, 300), ServerTable(), OnlineOptions(320)), "back", "join");
            }
            Controls::ControlPtr Settings() const override
            {
                auto scroll = std::make_shared<Controls::ScrollViewer>();
                scroll->Content(SettingRows(std::nan("")));
                scroll->HorizontalScrollBarVisibility(Controls::ScrollBarVisibility::Disabled);
                scroll->VerticalScrollBarVisibility(Controls::ScrollBarVisibility::Auto);
                return Page(660, "settings", std::make_shared<UiTabs>(SettingsTabs, 0),
                    scroll, "cancel", "save");
            }
            Controls::ControlPtr Pause() const override
            {
                auto menu = std::make_shared<Controls::StackPanel>();
                menu->Spacing(13);
                for (const std::string& entry : PauseEntries)
                {
                    auto word = Word(entry);
                    word->HorizontalAlignment(Layout::HorizontalAlignment::Center);
                    menu->Children.Add(word);
                }
                auto holder = std::make_shared<Controls::StackPanel>();
                holder->Spacing(0);
                holder->Children.Add(menu);
                auto note = Text(std::string(MatchText), 12, GuiTheme::TextDimBrush);
                note->HorizontalAlignment(Layout::HorizontalAlignment::Center);
                note->Margin(Thickness(0, 26, 0, 0));
                holder->Children.Add(note);
                return Page(460, "paused", nullptr, holder, "leave match", "resume", true);
            }

        private:
            static std::shared_ptr<Controls::Panel> Page(double wellWidth,
                std::string heading, const Controls::ControlPtr& nav,
                const Controls::ControlPtr& content, std::string cancel,
                std::string accept, bool overGame = false)
            {
                auto root = overGame ? MatchBehind() : Backdrop();
                auto wash = std::make_shared<Controls::Border>();
                auto gradient = std::make_shared<Media::LinearGradientBrush>();
                gradient->StartPoint = RelativePoint(0.5, 0, RelativeUnit::Relative);
                gradient->EndPoint = RelativePoint(0.5, 1, RelativeUnit::Relative);
                gradient->GradientStops = {
                    Media::GradientStop(Media::Color::FromArgb(150, 10, 12, 16), 0),
                    Media::GradientStop(Media::Color::FromArgb(225, 10, 12, 16), 0.45),
                    Media::GradientStop(Media::Color::FromArgb(225, 10, 12, 16), 0.62),
                    Media::GradientStop(Media::Color::FromArgb(150, 10, 12, 16), 1)
                };
                wash->Background(gradient);
                root->Children.Add(wash);
                auto well = std::make_shared<Controls::Grid>();
                well->RowDefinitions(Controls::RowDefinitions("Auto,Auto,*"));
                well->Width(wellWidth);
                well->HorizontalAlignment(Layout::HorizontalAlignment::Center);
                well->Margin(Thickness(0, 44, 0, 84));
                auto title = Text(Runtime::ToLowerInvariant(heading), UiLayout::HeadingSize,
                    GuiTheme::TextDimBrush);
                title->HorizontalAlignment(Layout::HorizontalAlignment::Center);
                title->Margin(Thickness(0, 0, 0, nav == nullptr ? 22 : 10));
                Controls::Grid::SetRow(*title, 0);
                well->Children.Add(title);
                if (nav != nullptr)
                {
                    nav->HorizontalAlignment(Layout::HorizontalAlignment::Center);
                    nav->Margin(Thickness(0, 0, 0, 22));
                    Controls::Grid::SetRow(*nav, 1);
                    well->Children.Add(nav);
                }
                Controls::Grid::SetRow(*content, 2);
                well->Children.Add(content);
                root->Children.Add(well);

                auto pair = std::make_shared<Controls::StackPanel>();
                pair->Orientation(Layout::Orientation::Horizontal);
                pair->Spacing(64);
                pair->HorizontalAlignment(Layout::HorizontalAlignment::Center);
                pair->VerticalAlignment(Layout::VerticalAlignment::Bottom);
                pair->Margin(Thickness(0, 0, 0, 28));
                pair->Children.Add(std::make_shared<UiMark>(UiMark::Shape::Cancel, cancel));
                pair->Children.Add(std::make_shared<UiMark>(UiMark::Shape::Accept, accept));
                root->Children.Add(pair);
                return root;
            }
        };

        const std::array<std::string_view, 4> ScreenNames{
            "play-offline", "play-online", "settings", "pause"
        };
    }

    std::int32_t UiDesigns::Run(UiDesignsAdapter& adapter,
        std::optional<std::string> directory)
    {
        if (!adapter.EnsureSetup())
        {
            adapter.ConsoleWriteLine(
                "[uidesign] no Avalonia backend on this machine; nothing captured");
            return 1;
        }
        if (!directory.has_value())
        {
            throw System::ArgumentNullException("path");
        }
        adapter.CreateDirectory(*directory);
        std::int32_t written = 0;
        const std::array<std::unique_ptr<Design>, 6> designs{
            std::make_unique<CurrentDesign>(),
            std::make_unique<SheetDesign>(),
            std::make_unique<SplitDesign>(),
            std::make_unique<StackDesign>(),
            std::make_unique<ShellDesign>(),
            std::make_unique<CentreDesign>()
        };
        adapter.InvokeUiThread([&]
        {
            for (const std::unique_ptr<Design>& design : designs)
            {
                adapter.ConsoleWriteLine("[uidesign] " + std::string(design->Id())
                    + " -- " + std::string(design->Title()));
                for (std::size_t i = 0; i < ScreenNames.size(); ++i)
                {
                    const Controls::ControlPtr view = design->Screen(i);
                    const std::string path = Runtime::PathCombine(*directory,
                        std::string(design->Id()) + "-" + std::string(ScreenNames[i]) + ".png");
                    if (adapter.Capture(view, path, CaptureSize))
                    {
                        ++written;
                        adapter.ConsoleWriteLine("[uidesign]   " + path);
                    }
                }
            }
        });
        adapter.ConsoleWriteLine("[uidesign] " + std::to_string(written)
            + " picture(s) written to " + *directory);
        return written > 0 ? 0 : 1;
    }

    std::int32_t UiDesigns::Run(std::optional<std::string> directory)
    {
        return Run(Detail::UiDesignsAdapterInstance(), std::move(directory));
    }
}
