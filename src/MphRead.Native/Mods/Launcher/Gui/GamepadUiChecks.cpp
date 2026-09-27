#include "GamepadUiChecks.hpp"

#include "ConfirmScreen.hpp"
#include "ControllerNav.hpp"
#include "ControllerKeyboard.hpp"
#include "DeckButton.hpp"
#include "FocusNavigator.hpp"
#include "GamepadMonitor.hpp"
#include "GamepadNavigation.hpp"
#include "GamepadSettingsPanel.hpp"
#include "GuiLauncher.hpp"
#include "GuiTheme.hpp"
#include "KeyRow.hpp"
#include "PadRow.hpp"
#include "PauseMenuView.hpp"
#include "Rows.hpp"
#include "SettingsView.hpp"
#include "SliderRow.hpp"
#include "UiTopLevel.hpp"
#include "UiWord.hpp"
#include "../../../Menu.hpp"
#include "../../Input/GamepadChecks.hpp"
#include "../../Input/GamepadManager.hpp"
#include "../../Input/GamepadOptions.hpp"
#include "../../Input/PadBindings.hpp"
#include "../../InputSettings.hpp"
#include "../../Network/MapVote.hpp"
#include "../../Network/NetProtocol.hpp"
#include "../../../NativeRuntime/Avalonia/Threading.hpp"
#include "../../../NativeRuntime/Stb/Image.hpp"
#include "../../../NativeRuntime/System/IO.hpp"

#include <algorithm>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace MphRead::Mods::Launcher::Gui
{
    namespace Av = ::MphRead::NativeRuntime::Avalonia;
    namespace Input = ::MphRead::Mods::Input;
    namespace Network = ::MphRead::Mods::Network;
    namespace Runtime = ::MphRead::NativeRuntime;

    namespace
    {
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
            if (::stbi_write_png_to_func(AppendPng, &bytes, width, height, 4,
                pixels, width * 4) == 0)
            {
                throw std::runtime_error("PNG encoding failed.");
            }
            return bytes;
        }

        void Pump(UiTopLevelImpl& topLevel)
        {
            topLevel.Root().UpdateLayout();
            Av::Threading::Dispatcher::UIThread().RunJobs();
            topLevel.Prepare();
            topLevel.Root().UpdateLayout();
            (void)topLevel.Render();
        }

        void SetContent(UiTopLevelImpl& topLevel, const Av::Controls::ControlPtr& content)
        {
            topLevel.Root().Content(content);
            Pump(topLevel);
        }

        void SaveFrame(UiTopLevelImpl& topLevel, const std::string& path)
        {
            Pump(topLevel);
            const std::vector<std::uint8_t> png = EncodePng(
                topLevel.Pixels(), topLevel.PixelWidth(), topLevel.PixelHeight());
            Runtime::FileWriteAllBytes(path, png);
        }

        template<typename T, typename TControl>
        [[nodiscard]] std::vector<T*> Descendants(TControl& control)
        {
            std::vector<T*> result;
            for (Av::Visual* visual : control.GetVisualDescendants())
            {
                if (auto* item = dynamic_cast<T*>(visual); item != nullptr)
                {
                    result.push_back(item);
                }
            }
            return result;
        }

        template<typename T, typename TControl>
        [[nodiscard]] T& First(TControl& control)
        {
            const std::vector<T*> items = Descendants<T>(control);
            if (items.empty())
            {
                throw std::runtime_error("Expected UI control was not found.");
            }
            return *items.front();
        }

        [[nodiscard]] std::shared_ptr<ChoiceRow> FindChoice(
            GamepadSettingsPanel& panel, std::string_view value)
        {
            for (const Av::Controls::ControlPtr& child : panel.Children)
            {
                const auto choice = std::dynamic_pointer_cast<ChoiceRow>(child);
                if (choice != nullptr && choice->Value() == value)
                {
                    return choice;
                }
            }
            throw std::runtime_error("Expected controller preset row was not found.");
        }

        [[nodiscard]] std::shared_ptr<ChoiceRow> FirstChoice(GamepadSettingsPanel& panel)
        {
            for (const Av::Controls::ControlPtr& child : panel.Children)
            {
                if (const auto choice = std::dynamic_pointer_cast<ChoiceRow>(child); choice != nullptr)
                {
                    return choice;
                }
            }
            throw std::runtime_error("Expected controller choice row was not found.");
        }

        [[nodiscard]] PadRow& FindPadRow(SettingsView& settings, Input::PadAction action)
        {
            for (PadRow* row : Descendants<PadRow>(settings))
            {
                if (row->Action() == action)
                {
                    return *row;
                }
            }
            throw std::runtime_error("Expected controller binding row was not found.");
        }

        [[nodiscard]] const ::MphRead::Mods::InputBindingProperty& Binding(std::string_view name)
        {
            const auto& bindings = InputSettings::Bindings();
            const auto found = std::find_if(bindings.begin(), bindings.end(), [name](const auto& property)
            {
                return property.Name == name;
            });
            if (found == bindings.end())
            {
                throw std::runtime_error("Expected keyboard binding was not found.");
            }
            return *found;
        }

        void UpdateDevice(const std::string& id, Input::GamepadState state,
            Input::GamepadFamily family = Input::GamepadFamily::Unknown,
            const std::optional<std::string>& mapping = std::nullopt)
        {
            Input::GamepadManager::UpdateDevice(id, std::move(state), true,
                family, Input::GamepadCapabilities::None, mapping);
        }
    }

    void GamepadUiChecks::Run(const std::optional<std::string>& shots)
    {
        Input::GamepadChecks::Check(GuiLauncher::EnsureSetup(), "headless UI initialization");

        UiTopLevelImpl topLevel;
        topLevel.SetClientSize(Av::Size{600, 400});
        Av::EmbeddableControlRoot& root = topLevel.Root();
        root.Background(GuiTheme::PanelBrush);
        topLevel.StartRendering();

        auto panel = std::make_shared<Av::Controls::StackPanel>();
        auto first = std::make_shared<UiWord>("First");
        auto hidden = std::make_shared<UiWord>("Hidden");
        hidden->IsVisible(false);
        auto disabled = std::make_shared<UiWord>("Disabled");
        disabled->IsEnabled(false);
        auto last = std::make_shared<UiWord>("Last");
        panel->Children.Add(first);
        panel->Children.Add(hidden);
        panel->Children.Add(disabled);
        panel->Children.Add(last);
        SetContent(topLevel, panel);

        FocusNavigator::Ensure(*panel);
        Input::GamepadChecks::Check(first->IsFocused(), "controller establishes focus");
        FocusNavigator::Move(*panel, Input::UiAction::Down);
        Input::GamepadChecks::Check(last->IsFocused(), "navigation skips hidden and disabled controls");
        std::int32_t clicked = 0;
        last->Click += [&clicked](UiWord&) { clicked++; };
        FocusNavigator::Key(*last, Av::Input::Key::Enter);
        Input::GamepadChecks::Check(clicked == 1, "controller activates existing UI control");

        auto choice = std::make_shared<ChoiceRow>("Option", std::vector<std::string>{"One", "Two"}, 0);
        panel->Children.Add(choice);
        Pump(topLevel);
        FocusNavigator::Focus(choice.get());
        FocusNavigator::Key(*choice, Av::Input::Key::Right);
        Input::GamepadChecks::Check(choice->Index() == 1, "choice row supports semantic arrows");

        auto text = std::make_shared<Av::Controls::TextBox>();
        text->Text("Player");
        text->Width(250);
        panel->Children.Add(text);
        Pump(topLevel);
        ControllerKeyboard keyboard(*text, [] {});
        Av::Threading::Dispatcher::UIThread().RunJobs();
        Input::GamepadChecks::Check(FocusNavigator::Ensure(keyboard.NavigationRoot()) != nullptr,
            "controller text entry has focus");
        keyboard.Close(false);
        Input::GamepadChecks::Check(text->Text() == "Player", "cancel text entry preserves value");

        auto confirm = std::make_shared<ConfirmScreen>("Leave the match?");
        std::optional<bool> answer;
        confirm->Answered += [&answer](ConfirmScreen&, bool value) { answer = value; };
        SetContent(topLevel, confirm);
        Av::Controls::Control* cancel = FocusNavigator::Ensure(*confirm);
        Input::GamepadChecks::Check(cancel != nullptr, "confirmation has focus");
        FocusNavigator::Key(*cancel, Av::Input::Key::Escape);
        Input::GamepadChecks::Check(answer == std::optional<bool>(false),
            "controller Back dismisses confirmation");

        auto menuSettings = std::make_shared<::MphRead::MenuSettings>();
        auto settings = std::make_shared<SettingsView>(menuSettings);
        topLevel.SetClientSize(Av::Size{960, 660});
        root.Content(settings);
        settings->ShowSection("Controls", 1);
        Pump(topLevel);
        std::vector<PadRow*> rows = Descendants<PadRow>(*settings);
        Input::GamepadChecks::Check(rows.size() == Input::PadBindings::Actions().size(),
            "every pad action appears in settings");
        FocusNavigator::Focus(rows.back());
        Pump(topLevel);
        Input::GamepadChecks::Check(rows.back()->IsFocused(), "last binding reachable through scrolling");
        const std::optional<Av::Point> rowPoint = rows.back()->TranslatePoint(Av::Point{}, &root);
        Input::GamepadChecks::Check(rowPoint.has_value() && rowPoint->Y >= 0
            && rowPoint->Y + rows.back()->Bounds().Height <= root.Bounds().Height,
            "focus scrolls binding inside the viewport");

        if (shots.has_value())
        {
            Runtime::DirectoryCreateDirectory(*shots);
            SaveFrame(topLevel, Runtime::PathCombine(*shots, "controller-bindings.png"));
            GamepadSettingsPanel& gamepad = First<GamepadSettingsPanel>(*settings);
            FocusNavigator::Focus(&First<SliderRow>(gamepad));
            Pump(topLevel);
            SaveFrame(topLevel, Runtime::PathCombine(*shots, "controller-settings.png"));
        }

        CheckControllerSettings(topLevel, settings, shots);

        auto pause = std::make_shared<PauseMenuView>(false);
        SetContent(topLevel, pause);
        Input::GamepadChecks::Check(FocusNavigator::Ensure(*pause) != nullptr,
            "pause menu is controller focusable");
        std::int32_t resumed = 0;
        pause->Resumed += [&resumed](PauseMenuView&) { resumed++; };
        GamepadNavigation navigation;
        UpdateDevice("pause-test", Input::GamepadState{.Name = std::string("Pause test")});
        navigation.Update(*pause);
        for (const Input::GamepadButtons button : {Input::GamepadButtons::B, Input::GamepadButtons::Start})
        {
            Input::GamepadState state{};
            state.Name = "Pause test";
            state.Buttons = button;
            UpdateDevice("pause-test", std::move(state));
            navigation.Update(*pause);
            UpdateDevice("pause-test", Input::GamepadState{.Name = std::string("Pause test")});
            navigation.Update(*pause);
        }
        Input::GamepadChecks::Check(resumed == 2, "B and Start resume the Android-hosted pause menu");
        Input::GamepadManager::RemoveDevice("pause-test");

        Network::VoteStatePacket voteState{};
        voteState.State = Network::VoteStatePacket::StateRunning;
        voteState.RoomKey = "test";
        voteState.Proposer = "Player";
        voteState.Seconds = 30;
        Network::MapVote::Apply(std::move(voteState));
        pause->RefreshVote();
        Pump(topLevel);
        DeckButton* vote = nullptr;
        for (DeckButton* button : Descendants<DeckButton>(*pause))
        {
            if (button->Text() == "Accept map vote")
            {
                vote = button;
                break;
            }
        }
        if (vote == nullptr)
        {
            throw std::runtime_error("Map-vote accept button was not found.");
        }
        FocusNavigator::Focus(vote);
        Input::GamepadChecks::Check(vote->IsVisible() && vote->IsFocused(),
            "active map vote can be reached with controller focus");
        Network::MapVote::Reset();
        pause->RefreshVote();
        Pump(topLevel);
        Input::GamepadChecks::Check(!vote->IsVisible() && !vote->IsFocused(),
            "expired vote restores pause focus");

        // Exercise binding through the same navigation pump used by the real UI.
        // The old test called PadRow::Check directly and therefore could not catch
        // the menu-path regression where physical presses never reached the row.
        Input::PadBindings::Reset();
        Input::PadBindings::Set(Input::PadAction::Chat, Input::GamepadButtons::None);
        auto routedBindings = std::make_shared<Av::Controls::StackPanel>();
        auto routedBinding = std::make_shared<PadRow>(Input::PadAction::Scan);
        routedBindings->Children.Add(routedBinding);
        SetContent(topLevel, routedBindings);
        Input::GamepadState routeNeutral{};
        routeNeutral.Connected = true;
        routeNeutral.Name = "Route test";
        UpdateDevice("route-test", routeNeutral);
        FocusNavigator::Focus(routedBinding.get());
        FocusNavigator::Key(*routedBinding, Av::Input::Key::Enter);
        navigation.Update(*routedBindings);
        Input::GamepadState routePressed = routeNeutral;
        routePressed.Buttons = Input::GamepadButtons::LeftThumb;
        UpdateDevice("route-test", routePressed);
        navigation.Update(*routedBindings);
        Input::GamepadChecks::Check(Input::PadBindings::Slot(Input::PadAction::Scan, 0)
                == Input::GamepadButtons::LeftThumb
            && !Input::GamepadContexts::Capturing(),
            "navigation pump delivers physical controller presses to binding capture");
        Input::GamepadManager::RemoveDevice("route-test");

        Input::PadBindings::Reset();
        auto binding = std::make_shared<PadRow>(Input::PadAction::Scan);
        SetContent(topLevel, binding);
        binding->Focus();
        const auto pad = [&binding](Input::GamepadButtons buttons)
        {
            Input::GamepadState state{};
            state.Connected = true;
            state.Name = "UI test";
            state.Buttons = buttons;
            UpdateDevice("ui-test", std::move(state));
            binding->Check();
        };
        pad(Input::GamepadButtons::A);
        FocusNavigator::Key(*binding, Av::Input::Key::Enter);
        binding->Check();
        Input::GamepadChecks::Check(Input::PadBindings::Get(Input::PadAction::Scan) == Input::GamepadButtons::X,
            "opening Accept cannot bind itself");
        pad(Input::GamepadButtons::None);
        pad(Input::GamepadButtons::RightBumper);
        Input::GamepadChecks::Check(Input::GamepadContexts::Capturing(), "binding conflict waits for a decision");
        pad(Input::GamepadButtons::None);
        pad(Input::GamepadButtons::B);
        Input::GamepadChecks::Check(Input::PadBindings::Get(Input::PadAction::Scan) == Input::GamepadButtons::X,
            "cancel conflict preserves mapping");
        pad(Input::GamepadButtons::None);
        FocusNavigator::Key(*binding, Av::Input::Key::Enter);
        pad(Input::GamepadButtons::Back);
        Input::GamepadChecks::Check(Input::PadBindings::Get(Input::PadAction::Scan) == Input::GamepadButtons::None,
            "controller can clear a binding");
        pad(Input::GamepadButtons::None);
        FocusNavigator::Key(*binding, Av::Input::Key::Enter);
        Input::GamepadManager::RemoveDevice("ui-test");
        binding->Check();
        Input::GamepadChecks::Check(!Input::GamepadContexts::Capturing(), "disconnect exits binding capture");

        root.Content(Av::Controls::ControlPtr{});
        Pump(topLevel);
    }

    void GamepadUiChecks::CheckControllerSettings(UiTopLevelImpl& topLevel,
        const std::shared_ptr<SettingsView>& settings, const std::optional<std::string>& shots)
    {
        GamepadSettingsPanel& panel = First<GamepadSettingsPanel>(*settings);
        Av::Controls::Control* advancedButton = ControllerNav::Find(panel,
            std::optional<std::string>("controller.advanced"));
        Input::GamepadChecks::Check(advancedButton != nullptr, "controller settings expose Advanced");
        GamepadMonitor* monitor = &First<GamepadMonitor>(panel);
        Input::GamepadChecks::Check(!monitor->IsEffectivelyVisible(),
            "advanced controller settings are collapsed by default");

        Input::PadBindings::ApplyPreset("Default");
        panel.Reload();
        Pump(topLevel);
        std::shared_ptr<ChoiceRow> preset = FindChoice(panel, "Default");
        FocusNavigator::Focus(preset.get());
        preset->Index(1);
        Av::Threading::Dispatcher::UIThread().RunJobs();
        Pump(topLevel);
        preset = FindChoice(panel, "Bumper Jumper");
        Input::GamepadChecks::Check(preset->IsFocused()
                && Input::PadBindings::Get(Input::PadAction::Jump) == Input::GamepadButtons::LeftBumper,
            "changing controller preset applies bindings and retains focus");
        FocusNavigator::Key(*preset, Av::Input::Key::Right);
        Av::Threading::Dispatcher::UIThread().RunJobs();
        Pump(topLevel);
        Input::GamepadChecks::Check(Input::GamepadOptions::Southpaw()
                && Input::PadBindings::Preset() == "Southpaw",
            "consecutive controller preset changes remain usable");
        Input::PadBindings::ApplyPreset("Default");
        panel.Reload();
        Pump(topLevel);

        GamepadNavigation navigation;
        const auto pad = [](Input::GamepadButtons buttons = Input::GamepadButtons::None, float rt = 0)
        {
            Input::GamepadState state{};
            state.Name = "Xbox Series controller";
            state.Buttons = buttons;
            state.RightTrigger = rt;
            UpdateDevice("settings-xbox", std::move(state), Input::GamepadFamily::Xbox,
                std::optional<std::string>("Xbox Bluetooth compatibility"));
        };
        pad();
        navigation.Update(*settings);
        settings->ShowSection("Controls", 0);
        Pump(topLevel);
        KeyRow* jumpKey = nullptr;
        for (KeyRow* row : Descendants<KeyRow>(*settings))
        {
            if (row->BindingName() == "Jump")
            {
                jumpKey = row;
                break;
            }
        }
        if (jumpKey == nullptr)
        {
            throw std::runtime_error("Jump keyboard binding row was not found.");
        }
        const auto& jumpProperty = Binding("Jump");
        const std::string keyboardBefore = InputSettings::Describe(InputSettings::Bind(jumpProperty));
        FocusNavigator::Focus(jumpKey);
        FocusNavigator::Key(*jumpKey, Av::Input::Key::Enter);
        pad(Input::GamepadButtons::None, 1);
        navigation.Update(*settings);
        Pump(topLevel);
        PadRow* jumpPad = &FindPadRow(*settings, Input::PadAction::Jump);
        Input::GamepadChecks::Check(jumpPad->IsFocused() && Input::GamepadContexts::Capturing()
                && !jumpKey->Listening(),
            "controller trigger in keyboard capture opens the matching controller action");
        pad();
        jumpPad->Check();
        pad(Input::GamepadButtons::A);
        jumpPad->Check();
        Input::GamepadChecks::Check(Input::PadBindings::Get(Input::PadAction::Jump)
                == Input::GamepadButtons::RightTrigger
                && Input::PadBindings::Get(Input::PadAction::Shoot) == Input::GamepadButtons::A
                && !Input::GamepadContexts::Capturing(),
            "Accept confirms the default Swap instead of silently cancelling a rebind");
        Input::GamepadChecks::Check(InputSettings::Describe(InputSettings::Bind(jumpProperty)) == keyboardBefore,
            "controller rebinding preserves the actual keyboard key");

        panel.RefreshLabels();
        bool customPreset = false;
        for (const Av::Controls::ControlPtr& child : panel.Children)
        {
            const auto row = std::dynamic_pointer_cast<ChoiceRow>(child);
            customPreset = customPreset || (row != nullptr && row->Value() == "Custom");
        }
        Input::GamepadChecks::Check(customPreset, "binding changes update the displayed controller preset");

        settings->ShowSection("Controls", 0);
        Pump(topLevel);
        jumpPad = &FindPadRow(*settings, Input::PadAction::Jump);
        FocusNavigator::Focus(jumpKey);
        pad();
        navigation.Update(*settings);
        pad(Input::GamepadButtons::A);
        navigation.Update(*settings);
        Input::GamepadChecks::Check(jumpPad->IsFocused() && Input::GamepadContexts::Capturing()
                && !jumpKey->Listening(),
            "controller Accept on a keyboard action enters controller capture");
        pad();
        jumpPad->Check();
        pad(Input::GamepadButtons::B);
        jumpPad->Check();
        Input::GamepadChecks::Check(!Input::GamepadContexts::Capturing(), "Back cancels redirected capture");

        settings->ShowSection("Controls", 0);
        Pump(topLevel);
        KeyRow* moveKey = nullptr;
        for (KeyRow* row : Descendants<KeyRow>(*settings))
        {
            if (row->BindingName() == "MoveUp")
            {
                moveKey = row;
                break;
            }
        }
        if (moveKey == nullptr)
        {
            throw std::runtime_error("MoveUp keyboard binding row was not found.");
        }
        const auto& moveProperty = Binding("MoveUp");
        const std::string movementBefore = InputSettings::Describe(InputSettings::Bind(moveProperty));
        FocusNavigator::Focus(moveKey);
        pad();
        navigation.Update(*settings);
        pad(Input::GamepadButtons::A);
        navigation.Update(*settings);
        Input::GamepadChecks::Check(!moveKey->Listening()
                && InputSettings::Describe(InputSettings::Bind(moveProperty)) == movementBefore,
            "keyboard-only rows never bind synthetic controller Enter");

        settings->ShowSection("Controls", 1);
        Pump(topLevel);
        advancedButton = ControllerNav::Find(panel,
            std::optional<std::string>("controller.advanced"));
        monitor = &First<GamepadMonitor>(panel);
        FocusNavigator::Focus(advancedButton);
        FocusNavigator::Key(*advancedButton, Av::Input::Key::Enter);
        Pump(topLevel);
        Input::GamepadChecks::Check(monitor->IsEffectivelyVisible(),
            "Advanced reveals controller diagnostics");
        pad(Input::GamepadButtons::None, 1);
        monitor->Refresh();
        Input::GamepadChecks::Check(monitor->Status().find("Xbox Bluetooth compatibility") != std::string::npos,
            "live controller test identifies hardware mapping");
        if (shots.has_value())
        {
            Av::Threading::Dispatcher::UIThread().RunJobs();
            Pump(topLevel);
            FocusNavigator::Focus(FirstChoice(panel).get());
            Pump(topLevel);
            SaveFrame(topLevel, Runtime::PathCombine(*shots, "controller-live-test.png"));
        }
        Input::GamepadManager::RemoveDevice("settings-xbox");
        Input::PadBindings::Reset();
        Input::GamepadOptions::Reset();
    }
}
