#include "UiCapture.hpp"

#include "ConfirmScreen.hpp"
#include "CreateServerScreen.hpp"
#include "Deck.hpp"
#include "EndPanelView.hpp"
#include "GuiLauncher.hpp"
#include "GuiTheme.hpp"
#include "PauseMenuView.hpp"
#include "PlayScreen.hpp"
#include "ServerRow.hpp"
#include "SettingsView.hpp"
#include "SetupScreen.hpp"
#include "StartScreen.hpp"
#include "UiTopLevel.hpp"
#include "UiDesigns.hpp"
#include "../../Branding.hpp"
#include "../../LogShare.hpp"
#include "../../Network/NetMaster.hpp"
#include "../../Network/NetStatus.hpp"
#include "../../../Menu.hpp"
#include "../../../Metadata/Rooms.hpp"
#include "../../../NativeRuntime/Stb/Image.hpp"
#include "../../../NativeRuntime/System/Console.hpp"
#include "../../../NativeRuntime/System/Encoding.hpp"
#include "../../../NativeRuntime/System/ExceptionText.hpp"
#include "../../../NativeRuntime/System/Exceptions.hpp"
#include "../../../NativeRuntime/System/Globalization.hpp"
#include "../../../NativeRuntime/System/IO.hpp"
#include "../../../NativeRuntime/System/Number.hpp"
#include "../../../NativeRuntime/System/Runtime.hpp"

#include <algorithm>
#include <cstdlib>
#include <cstdint>
#include <exception>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <typeinfo>
#include <utility>
#include <vector>

#if defined(__GNUG__)
#include <cxxabi.h>
#endif

namespace
{
    namespace Av = ::MphRead::NativeRuntime::Avalonia;
    namespace Runtime = ::MphRead::NativeRuntime;
    using ::MphRead::GameMode;
    using ::MphRead::Mods::Launcher::Gui::PlayScreen;
    using ::MphRead::Mods::Network::ServerStatus;

    constexpr ::MphRead::Mods::Launcher::Gui::UiCaptureSize WindowSize{940.0, 528.0};
    constexpr ::MphRead::Mods::Launcher::Gui::UiCaptureSize PhonePortrait{360.0, 800.0};
    constexpr ::MphRead::Mods::Launcher::Gui::UiCaptureSize PhoneLandscape{800.0, 360.0};

    struct BrowserRow final
    {
        std::string Name;
        std::string Endpoint;
        ServerStatus Status;
    };

    [[nodiscard]] const std::vector<BrowserRow>& BrowserRows()
    {
        static const std::vector<BrowserRow> rows = []
        {
            std::vector<BrowserRow> result;
            result.reserve(13);
            const auto add = [&result](std::string name, std::string endpoint,
                std::string room, GameMode mode, std::int32_t players,
                std::int32_t maxPlayers, std::int32_t ping)
            {
                ServerStatus status;
                status.Online = ping >= 0;
                status.RoomKey = std::move(room);
                status.Mode = mode;
                status.Players = players;
                status.MaxPlayers = maxPlayers;
                status.Latency = ping;
                result.push_back(BrowserRow{
                    std::move(name), std::move(endpoint), std::move(status)});
            };

            add("Combat Hall 24/7", "82.66.14.9:27888", "MP3 PROVING GROUND",
                GameMode::Battle, 4, 8, 24);
            add("Prime EU #1", "85.214.228.188:27888", "AD2 MAGMA VENTS",
                GameMode::BountyTeams, 7, 8, 48);
            add("Arcterra Pickup", "83.19.146.2:27890", "MP9 CRYOCHASM",
                GameMode::Survival, 2, 6, 91);
            add("MPH Speedrun", "51.140.44.19:27888", "AD1 TRANSFER LOCK DM",
                GameMode::PrimeHunter, 1, 4, 168);
            add("hunters.us.west", "34.94.2.11:27888", "MP8 FIRE CONTROL",
                GameMode::BattleTeams, 6, 8, 212);
            add("Sic Transit 24/7", "91.198.174.192:27888", "MP12 SIC TRANSIT",
                GameMode::Capture, 3, 8, 57);
            add("LAN - Pi 4", "192.168.1.42:27888", "MP2 HARVESTER",
                GameMode::Capture, 0, 8, 3);
            add("Ice Hive Rotation", "80.50.24.3:27888", "MP9 CRYOCHASM",
                GameMode::Nodes, 4, 8, 74);
            add("Head Shot only", "92.222.10.7:27888", "MP6 HEADSHOT",
                GameMode::Battle, 5, 6, 31);
            add("Elder Passage.CTF", "54.230.11.9:27888",
                "MP4 HIGHGROUND - EXPANDED", GameMode::Capture, 5, 8, 143);
            add("Data Shrine.1v1", "217.160.0.153:27888", "MP1 SANCTORUS",
                GameMode::Battle, 2, 2, 39);
            add("Fuel Stack Rotation", "51.148.31.4:27888", "MP13 ACCELERATOR",
                GameMode::Bounty, 2, 8, 66);
            add("old.vesper", "45.33.32.156:27888", "", GameMode::Battle,
                0, 0, -1);
            return result;
        }();
        return rows;
    }

    [[nodiscard]] std::vector<PlayScreen::SampleServer> PlaySamples()
    {
        std::vector<PlayScreen::SampleServer> result;
        result.reserve(BrowserRows().size());
        for (const BrowserRow& row : BrowserRows())
        {
            result.push_back(PlayScreen::SampleServer{
                row.Name, row.Endpoint, row.Status});
        }
        return result;
    }

    [[nodiscard]] std::vector<::MphRead::Mods::Network::HostCandidate> Fleet()
    {
        using ::MphRead::Mods::Network::HostCandidate;
        return {
            HostCandidate{"net.livetek.fr", "net.livetek.fr", 27889,
                true, true, 3},
            HostCandidate{"Fruity Prime - West Europe", "20.16.135.109", 27889,
                true, std::nullopt, 39},
            HostCandidate{"Fruity Prime - Japan", "13.78.14.98", 27889,
                false, std::nullopt, -1}
        };
    }

    [[nodiscard]] std::vector<std::string> RoomList()
    {
        std::vector<std::string> rooms;
        try
        {
            for (const auto& entry : ::MphRead::Metadata::RoomMetadata)
            {
                const std::shared_ptr<::MphRead::RoomMetadata>& meta = entry.second;
                if (meta == nullptr)
                {
                    throw ::System::NullReferenceException();
                }
                if (meta->Multiplayer)
                {
                    rooms.push_back(meta->Name);
                }
            }
        }
        catch (const std::exception&)
        {
            // Match C#'s catch around the metadata foreach: keep rooms already
            // collected and still sort that partial list.
        }
        std::sort(rooms.begin(), rooms.end(), Runtime::OrdinalIgnoreCaseLess{});
        return rooms;
    }

    class CaptureLogShare final : public ::MphRead::Mods::ILogShare
    {
    public:
        [[nodiscard]] std::u16string StagingPath(std::u16string_view fileName) override
        {
            const std::string temp = Runtime::PathGetTempPath();
            const std::string file = Runtime::Utf16ToUtf8(fileName);
            return Runtime::Utf8ToUtf16(Runtime::PathCombine(temp, file));
        }

        [[nodiscard]] bool Share(std::u16string_view path,
            std::u16string_view subject, std::u16string& error) override
        {
            (void)path;
            (void)subject;
            error = u"there is nothing to share to on this platform";
            return false;
        }
    };

    [[nodiscard]] std::string TypeName(const Av::Visual& visual)
    {
        std::string typeName = typeid(visual).name();
#if defined(__GNUG__)
        int status = 0;
        std::unique_ptr<char, decltype(&std::free)> demangled(
            abi::__cxa_demangle(typeName.c_str(), nullptr, nullptr, &status), &std::free);
        if (status == 0 && demangled != nullptr)
        {
            typeName = demangled.get();
        }
#endif
        if (typeName.starts_with("class "))
        {
            typeName.erase(0, 6);
        }
        else if (typeName.starts_with("struct "))
        {
            typeName.erase(0, 7);
        }
        const std::size_t namespaceEnd = typeName.rfind("::");
        if (namespaceEnd != std::string::npos)
        {
            typeName.erase(0, namespaceEnd + 2);
        }
        return typeName;
    }

    [[nodiscard]] std::string LabelOf(const Av::Visual& visual)
    {
        if (const auto* text = dynamic_cast<const Av::Controls::TextBlock*>(&visual))
        {
            return text->Text();
        }
        if (const auto* text = dynamic_cast<const Av::Controls::TextBox*>(&visual))
        {
            return text->Text();
        }
        return {};
    }

    [[nodiscard]] std::string Escape(std::string_view text)
    {
        std::string escaped;
        escaped.reserve(text.size());
        for (char ch : text)
        {
            switch (ch)
            {
            case '\\': escaped += "\\\\"; break;
            case '"': escaped += "\\\""; break;
            case '\n': escaped += ' '; break;
            case '\r': break;
            default: escaped += ch; break;
            }
        }
        return escaped;
    }

    [[nodiscard]] std::string Number(double value)
    {
        // Math.Round(value, 1) uses midpoint-to-even; MathRoundToInt32 is the
        // shared NativeRuntime equivalent of that rounding step.
        const std::int32_t tenths = Runtime::MathRoundToInt32(value * 10.0);
        return Runtime::ToString(static_cast<double>(tenths) / 10.0);
    }

    void WalkBounds(const Av::Visual& node, const Av::Visual& root,
        std::string& json, bool& first)
    {
        for (const std::shared_ptr<Av::Visual>& child : node.VisualChildren())
        {
            const Av::Rect bounds = child->Bounds();
            if (bounds.Width > 0 && bounds.Height > 0)
            {
                const Av::Point origin = child->TranslatePoint(Av::Point{0, 0}, &root)
                    .value_or(Av::Point{0, 0});
                if (!first)
                {
                    json += ",\n";
                }
                first = false;
                json += " {\"type\":\"" + TypeName(*child)
                    + "\",\"label\":\"" + Escape(LabelOf(*child))
                    + "\",\"x\":" + Number(origin.X)
                    + ",\"y\":" + Number(origin.Y)
                    + ",\"w\":" + Number(bounds.Width)
                    + ",\"h\":" + Number(bounds.Height) + "}";
            }
            WalkBounds(*child, root, json, first);
        }
    }

    void DumpBounds(const Av::Visual& root, const std::string& path)
    {
        std::string json = "[\n";
        bool first = true;
        WalkBounds(root, root, json, first);
        json += "\n]\n";

        std::string jsonPath = path;
        const std::string extension = Runtime::PathGetExtension(path);
        if (extension.empty())
        {
            jsonPath += ".json";
        }
        else
        {
            jsonPath.resize(jsonPath.size() - extension.size());
            jsonPath += ".json";
        }
        Runtime::FileWriteAllText(jsonPath, json);
    }

    void AppendPng(void* context, void* data, int size)
    {
        auto& bytes = *static_cast<std::vector<std::uint8_t>*>(context);
        const auto* begin = static_cast<const std::uint8_t*>(data);
        bytes.insert(bytes.end(), begin, begin + size);
    }

    [[nodiscard]] std::vector<std::uint8_t> EncodePng(
        const std::uint8_t* pixels, std::int32_t width, std::int32_t height)
    {
        std::vector<std::uint8_t> bytes;
        ::stbi_flip_vertically_on_write(0);
        const int encoded = ::stbi_write_png_to_func(AppendPng, &bytes,
            width, height, 4, pixels, width * 4);
        if (encoded == 0)
        {
            throw std::runtime_error("PNG encoding failed.");
        }
        return bytes;
    }
}

namespace MphRead::Mods::Launcher::Gui
{
    namespace Av = ::MphRead::NativeRuntime::Avalonia;
    namespace Runtime = ::MphRead::NativeRuntime;

    std::int32_t UiCapture::Run(std::optional<std::string> directory)
    {
        if (!GuiLauncher::EnsureSetup())
        {
            Runtime::ConsoleWriteLine(
                "[uishot] no Avalonia backend on this machine; nothing captured");
            return 1;
        }
        if (!directory.has_value())
        {
            throw ::System::ArgumentNullException("path");
        }
        Runtime::DirectoryCreateDirectory(*directory);

        if (!Mods::LogShare::Current())
        {
            Mods::LogShare::Current(std::make_shared<CaptureLogShare>());
        }
        Deck::Still(true);

        std::int32_t written = 0;
        Av::Threading::Dispatcher::UIThread().Invoke([&]
        {
            auto settings = std::make_shared<::MphRead::MenuSettings>();
            const std::vector<std::string> rooms = RoomList();
            const auto capture = [&](std::string_view name, UiCaptureSize size,
                const std::function<Av::Controls::ControlPtr()>& create)
            {
                // C#'s iterator constructs the current control before the
                // foreach body changes Deck.Phone and PlayScreen.Sample.
                const Av::Controls::ControlPtr view = create();
                const std::string path = Runtime::PathCombine(
                    *directory, std::string(name) + ".png");
                Deck::Phone(size == PhonePortrait || size == PhoneLandscape);
                PlayScreen::Sample = PlaySamples();
                if (Capture(view, path, size))
                {
                    ++written;
                    Runtime::ConsoleWriteLine("[uishot] " + path);
                }
            };

            capture("start", WindowSize, [&]
            {
                return StartScreen::Create(settings, rooms);
            });
            capture("start-phone-portrait", PhonePortrait, [&]
            {
                return StartScreen::Create(settings, rooms);
            });
            capture("start-phone-landscape", PhoneLandscape, [&]
            {
                return StartScreen::Create(settings, rooms);
            });

            capture("play-online", WindowSize, [&]
            {
                return std::make_shared<PlayScreen>(settings, rooms, PlayScreen::Face::Online);
            });
            capture("play-online-phone-portrait", PhonePortrait, [&]
            {
                return std::make_shared<PlayScreen>(settings, rooms, PlayScreen::Face::Online);
            });
            capture("play-online-phone-landscape", PhoneLandscape, [&]
            {
                return std::make_shared<PlayScreen>(settings, rooms, PlayScreen::Face::Online);
            });
            capture("play-offline", WindowSize, [&]
            {
                return std::make_shared<PlayScreen>(settings, rooms, PlayScreen::Face::Offline);
            });
            capture("play-story", WindowSize, [&]
            {
                return std::make_shared<PlayScreen>(settings, rooms, PlayScreen::Face::Story);
            });
            capture("play-clips", WindowSize, [&]
            {
                return std::make_shared<PlayScreen>(settings, rooms, PlayScreen::Face::Clips);
            });
            capture("play-vote", WindowSize, [&]
            {
                return std::make_shared<PlayScreen>(settings, rooms,
                    PlayScreen::Face::Vote, true);
            });

            capture("create-server", WindowSize, [&]
            {
                return std::make_shared<CreateServerScreen>(rooms);
            });
            capture("create-server-dedicated", WindowSize, [&]
            {
                auto dedicated = std::make_shared<CreateServerScreen>(rooms);
                dedicated->ShowDedicated();
                return dedicated;
            });
            capture("create-server-maps", WindowSize, [&]
            {
                return std::make_shared<MapRotationPicker>(rooms,
                    std::vector<std::string>{});
            });
            capture("create-server-hosts", WindowSize, [&]
            {
                auto hosts = std::make_shared<HostPicker>();
                hosts->Show(Fleet(), false);
                return hosts;
            });

            capture("settings", WindowSize, [&]
            {
                return std::make_shared<SettingsView>(settings);
            });
            capture("settings-player", WindowSize, [&]
            {
                auto player = std::make_shared<SettingsView>(settings);
                player->ShowSection("Profile");
                return player;
            });
            capture("settings-controls", WindowSize, [&]
            {
                auto controls = std::make_shared<SettingsView>(settings);
                controls->ShowSection("Controls");
                return controls;
            });
            capture("settings-gamepad", WindowSize, [&]
            {
                auto gamepad = std::make_shared<SettingsView>(settings);
                gamepad->ShowSection("Controls", 1);
                return gamepad;
            });

            capture("end-panel", WindowSize, [&]
            {
                return std::make_shared<EndPanelView>();
            });
            capture("end-panel-hunter", WindowSize, [&]
            {
                auto endHunter = std::make_shared<EndPanelView>();
                endHunter->ShowHunter();
                return endHunter;
            });
            capture("setup", WindowSize, [&]
            {
                return std::make_shared<SetupScreen>();
            });
            capture("confirm", WindowSize, [&]
            {
                return std::make_shared<ConfirmScreen>(
                    std::string("Quit ") + std::string(Mods::Branding::Name) + "?");
            });

            capture("pausemenu", WindowSize, [&]
            {
                return std::make_shared<PauseMenuView>(true);
            });
            capture("pausemenu-small", UiCaptureSize{560.0, 320.0}, [&]
            {
                return std::make_shared<PauseMenuView>(true);
            });
            capture("pausemenu-phone", PhoneLandscape, [&]
            {
                auto paused = StartScreen::Create(settings, rooms);
                paused->ShowPauseMenu([] {}, [] {}, [] {});
                return paused;
            });
            capture("serverbrowser", WindowSize, [&]
            {
                auto stack = std::make_shared<Av::Controls::StackPanel>();
                stack->Spacing(18.0);
                stack->Margin(Av::Thickness(12.0));
                for (double width : {600.0, 400.0})
                {
                    auto list = std::make_shared<Av::Controls::StackPanel>();
                    list->Spacing(2.0);
                    list->Width(width);
                    for (const BrowserRow& sample : BrowserRows())
                    {
                        auto row = std::make_shared<ServerRow>(sample.Name, sample.Endpoint);
                        row->SetStatus(sample.Status);
                        list->Children.Add(row);
                    }
                    stack->Children.Add(list);
                }
                return stack;
            });
        });

        Deck::Phone(Runtime::IsAndroid());
        Deck::Still(false);
        PlayScreen::Sample.reset();
        Runtime::ConsoleWriteLine("[uishot] " + Runtime::ToString(written)
            + " screen(s) written to " + *directory);
        return written > 0 ? 0 : 1;
    }

    bool UiCapture::Capture(const Av::Controls::ControlPtr& view,
        std::string_view path, UiCaptureSize size)
    {
        try
        {
            UiTopLevelImpl topLevel;
            // Capture is intentionally software/off-screen. Normal launcher
            // frames remain Ganesh-backed and never read pixels back from the GPU.
            topLevel.GpuRendering(false);
            topLevel.SetClientSize(Av::Size{size.Width, size.Height});
            Av::EmbeddableControlRoot& root = topLevel.Root();
            root.Background(GuiTheme::PanelBrush);
            root.Content(view);

            for (std::int32_t i = 0; i < 8; ++i)
            {
                Av::Threading::Dispatcher::UIThread().RunJobs();
            }
            topLevel.Prepare();
            Av::Threading::Dispatcher::UIThread().RunJobs();
            topLevel.Prepare();
            topLevel.StartRendering();
            (void)topLevel.Render();

            const std::vector<std::uint8_t> png = EncodePng(
                topLevel.Pixels(), topLevel.PixelWidth(), topLevel.PixelHeight());
            Runtime::FileWriteAllBytes(std::string(path), png);
            DumpBounds(root, std::string(path));
            return true;
        }
        catch (...)
        {
            const std::string fileName = Runtime::PathGetFileName(std::string(path));
            const std::string message = Runtime::ExceptionMessage(std::current_exception());
            Runtime::ConsoleWriteLine("[shot] " + fileName
                + " could not be rendered: " + message);
            return false;
        }
    }

    namespace
    {
        class LiveUiDesignsAdapter final : public UiDesignsAdapter
        {
        public:
            bool EnsureSetup() override
            {
                return GuiLauncher::EnsureSetup();
            }

            void CreateDirectory(std::string_view directory) override
            {
                Runtime::DirectoryCreateDirectory(std::string(directory));
            }

            void InvokeUiThread(const std::function<void()>& action) override
            {
                Av::Threading::Dispatcher::UIThread().Invoke(action);
            }

            bool Capture(const Av::Controls::ControlPtr& view,
                std::string_view path, UiCaptureSize size) override
            {
                return UiCapture::Capture(view, path, size);
            }

            void ConsoleWriteLine(std::string_view text) override
            {
                Runtime::ConsoleWriteLine(std::string(text));
            }
        };
    }

    namespace Detail
    {
        UiDesignsAdapter& UiDesignsAdapterInstance()
        {
            static LiveUiDesignsAdapter adapter;
            return adapter;
        }
    }
}
