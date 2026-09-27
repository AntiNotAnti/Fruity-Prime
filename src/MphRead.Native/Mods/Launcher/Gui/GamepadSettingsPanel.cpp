#include "GamepadSettingsPanel.hpp"

#include "ControllerNav.hpp"
#include "DeckButton.hpp"
#include "Deck.hpp"
#include "FocusNavigator.hpp"
#include "GamepadMonitor.hpp"
#include "GamepadProfilePanel.hpp"
#include "GamepadSetupPanel.hpp"
#include "GuiTheme.hpp"
#include "PadRow.hpp"
#include "Rows.hpp"
#include "SettingsView.hpp"
#include "SliderRow.hpp"
#include "../../../Mods/Input/GamepadHaptics.hpp"
#include "../../../Mods/Input/GamepadOptions.hpp"
#include "../../../Mods/Input/GamepadProfiles.hpp"
#include "../../../Mods/Input/GamepadUiRouter.hpp"
#include "../../../Mods/Input/PadBindings.hpp"
#include "../../../NativeRuntime/System/Globalization.hpp"
#include "../../../NativeRuntime/System/Managed.hpp"
#include "../../../NativeRuntime/Avalonia/TopLevel.hpp"
#include "../../../NativeRuntime/Avalonia/Threading.hpp"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace MphRead::Mods::Launcher::Gui
{
    namespace PadInput = ::MphRead::Mods::Input;
    namespace Runtime = ::MphRead::NativeRuntime;
    using namespace Runtime::Avalonia;

    GamepadSettingsPanel::GamepadSettingsPanel()
    {
        Spacing(8);
        Reload();
        _timer.Interval(Threading::TimeSpan(0.1));
        _timer.Tick += [this](Threading::DispatcherTimer&)
        {
            if (!IsEffectivelyVisible())
            {
                return;
            }
            const PadInput::GamepadSnapshot snapshot = PadInput::GamepadManager::Snapshot();
            if ((_profileRevision != PadInput::GamepadProfiles::Revision()
                || _runtimeRevision != snapshot.Revision) && !PadInput::GamepadContexts::Capturing())
            {
                Reload();
            }
            RefreshDevices();
            RefreshLabels();
        };
        _timer.Start();
        AttachedToVisualTree += [this](Controls::Control&) { _timer.Start(); };
        DetachedFromVisualTree += [this](Controls::Control&) { _timer.Stop(); };
    }

    void GamepadSettingsPanel::Reload()
    {
        Controls::Control* focused = FocusNavigator::Focused(*this);
        const std::optional<std::string> focusedId = focused != nullptr
            ? focused->GetValue(ControllerNav::NavIdProperty) : std::nullopt;
        Children.Clear();
        _deviceList.clear();
        _devices.reset();
        _presetRow.reset();
        _profileRevision = PadInput::GamepadProfiles::Revision();
        _runtimeRevision = PadInput::GamepadManager::Snapshot().Revision;
        RefreshDevices();

        const auto presetIndex = []
        {
            const std::string current = PadInput::PadBindings::Preset();
            for (std::size_t i = 0; i < Presets.size(); i++)
            {
                if (current == Presets[i])
                {
                    return static_cast<std::int32_t>(i);
                }
            }
            return -1;
        };
        std::vector<std::string> presets;
        for (const std::string_view preset : Presets)
        {
            presets.emplace_back(preset);
        }
        Choice("controller.control_layout", "Control layout", presets, presetIndex(), [this](std::int32_t index)
        {
            PadInput::PadBindings::ApplyPreset(std::string(Presets[static_cast<std::size_t>(index)]));
            RefreshLabels();
            // Rebuild after the choice event finishes, as the C# dispatcher post does.
            PostReload();
        });

        Number("controller.horizontal_sensitivity", "Horizontal sensitivity", PadInput::GamepadOptions::LookX(), .1F, 5,
            [](float value) { PadInput::GamepadOptions::LookX(value); });
        Number("controller.vertical_sensitivity", "Vertical sensitivity", PadInput::GamepadOptions::LookY(), .1F, 5,
            [](float value) { PadInput::GamepadOptions::LookY(value); });
        Number("controller.stick_deadzone", "Stick dead zone",
            std::max(PadInput::GamepadOptions::LeftInner(), PadInput::GamepadOptions::RightInner()), 0, .9F,
            [](float value)
            {
                PadInput::GamepadOptions::LeftInner(value);
                PadInput::GamepadOptions::RightInner(value);
            });
        Flag("controller.invert_vertical", "Invert vertical aim", PadInput::GamepadOptions::InvertY(),
            [](bool value) { PadInput::GamepadOptions::InvertY(value); });
        Flag("controller.vibration", "Vibration", PadInput::GamepadOptions::Vibration(), [](bool value)
        {
            PadInput::GamepadOptions::Vibration(value);
            if (!value)
            {
                PadInput::GamepadHaptics::Stop();
            }
        });

        auto advanced = std::make_shared<Controls::StackPanel>();
        advanced->Spacing(8);
        advanced->IsVisible(_advancedOpen);
        auto advancedButton = std::make_shared<DeckButton>("Advanced", Deck::Face::Slate(), .9, .8, .38, 3);
        advancedButton->HorizontalAlignment(Layout::HorizontalAlignment::Left);
        ControllerNav::Identify(*advancedButton, "controller.advanced");
        advancedButton->Click += [this, advanced](DeckButton&)
        {
            _advancedOpen = !_advancedOpen;
            advanced->IsVisible(_advancedOpen);
            if (_advancedOpen)
            {
                Threading::Dispatcher::UIThread().Post([advanced] { FocusNavigator::Ensure(*advanced); },
                    Threading::DispatcherPriority::Background);
            }
        };
        Children.Add(advancedButton);
        Children.Add(advanced);
        _target = advanced.get();

        advanced->Children.Add(std::make_shared<GamepadMonitor>());
        std::vector<std::string> families{"Automatic", "Xbox", "PlayStation", "Nintendo", "Generic"};
        Choice("controller.button_labels", "Button labels", families,
            static_cast<std::int32_t>(PadInput::GamepadOptions::GlyphStyle()),
            [](std::int32_t value) { PadInput::GamepadOptions::GlyphStyle(static_cast<PadInput::GamepadFamily>(value)); });
        auto explanation = std::make_shared<Controls::TextBlock>();
        explanation->Text("Advanced settings are per controller. Calibration, profiles and mappings stay with the selected device.");
        explanation->FontSize(11);
        explanation->Foreground(GuiTheme::TextDimBrush);
        explanation->TextWrapping(Media::TextWrapping::Wrap);
        advanced->Children.Add(explanation);
        Number("controller.scoped_horizontal_multiplier", "Scoped horizontal multiplier", PadInput::GamepadOptions::ScopedX(), .1F, 3,
            [](float value) { PadInput::GamepadOptions::ScopedX(value); });
        Number("controller.scoped_vertical_multiplier", "Scoped vertical multiplier", PadInput::GamepadOptions::ScopedY(), .1F, 3,
            [](float value) { PadInput::GamepadOptions::ScopedY(value); });
        Flag("controller.toggle_weapon_wheel", "Toggle weapon wheel", PadInput::GamepadOptions::WheelToggle(),
            [](bool value) { PadInput::GamepadOptions::WheelToggle(value); });
        Number("controller.wheel_selection_threshold", "Wheel selection threshold", PadInput::GamepadOptions::WheelThreshold(), .1F, .95F,
            [](float value) { PadInput::GamepadOptions::WheelThreshold(value); });

        std::vector<std::string> modifiers;
        for (const PadInput::GamepadButtons button : PadInput::GamepadButtonValues)
        {
            modifiers.push_back(PadInput::PadBindings::Describe(button));
        }
        const auto currentModifier = std::find(PadInput::GamepadButtonValues.begin(), PadInput::GamepadButtonValues.end(),
            PadInput::GamepadOptions::BindingModifier());
        const std::int32_t modifierIndex = currentModifier == PadInput::GamepadButtonValues.end() ? -1
            : static_cast<std::int32_t>(std::distance(PadInput::GamepadButtonValues.begin(), currentModifier));
        Choice("controller.modifier_for_new_bindings", "Modifier for new bindings", modifiers, modifierIndex,
            [](std::int32_t value) { PadInput::GamepadOptions::BindingModifier(PadInput::GamepadButtonValues[static_cast<std::size_t>(value)]); });
        advanced->Children.Add(std::make_shared<Note>("Hold the modifier first, then press the action button. Modifier combinations are reserved during gameplay."));

        const std::vector<std::string> weapons{"Volt Driver", "Battlehammer", "Imperialist", "Judicator", "Magmaul", "Shock Coil"};
        for (std::int32_t position = 0; position < 6; position++)
        {
            Choice("controller.wheel." + std::to_string(position), "Wheel position " + std::to_string(position + 1), weapons,
                (*PadInput::GamepadOptions::WheelOrder())[static_cast<std::size_t>(position)], [this, position](std::int32_t slot)
                {
                    PadInput::GamepadOptions::SetWheelSlot(position, slot);
                    // Reload keeps every wheel selector and its ordering in sync.
                    PostReload();
                });
        }
        Number("controller.left_inner_deadzone", "Left inner deadzone", PadInput::GamepadOptions::LeftInner(), 0, .9F,
            [](float value) { PadInput::GamepadOptions::LeftInner(value); });
        Number("controller.left_outer_deadzone", "Left outer deadzone", PadInput::GamepadOptions::LeftOuter(), 0, .5F,
            [](float value) { PadInput::GamepadOptions::LeftOuter(value); });
        Number("controller.right_inner_deadzone", "Right inner deadzone", PadInput::GamepadOptions::RightInner(), 0, .9F,
            [](float value) { PadInput::GamepadOptions::RightInner(value); });
        Number("controller.right_outer_deadzone", "Right outer deadzone", PadInput::GamepadOptions::RightOuter(), 0, .5F,
            [](float value) { PadInput::GamepadOptions::RightOuter(value); });

        std::vector<std::string> curves{"Linear", "Classic", "Precision", "Dynamic"};
        Choice("controller.aim_curve", "Aim curve", curves, static_cast<std::int32_t>(PadInput::GamepadOptions::Curve()),
            [](std::int32_t value) { PadInput::GamepadOptions::Curve(static_cast<PadInput::GamepadCurve>(value)); });
        Flag("controller.invert_horizontal", "Invert horizontal aim", PadInput::GamepadOptions::InvertX(),
            [](bool value) { PadInput::GamepadOptions::InvertX(value); });
        Flag("controller.southpaw", "Southpaw sticks", PadInput::GamepadOptions::Southpaw(),
            [](bool value) { PadInput::GamepadOptions::Southpaw(value); });
        Number("controller.trigger_actuation", "Gameplay trigger actuation", PadInput::GamepadOptions::TriggerThreshold(), .05F, .95F,
            [](float value) { PadInput::GamepadOptions::TriggerThreshold(value); });
        Number("controller.device_activity_threshold", "Device activity threshold", PadInput::GamepadOptions::ActivityThreshold(), .2F, .95F,
            [](float value) { PadInput::GamepadOptions::ActivityThreshold(value); });
        Number("controller.vibration_strength", "Vibration strength", PadInput::GamepadOptions::VibrationStrength(), 0, 1,
            [](float value)
            {
                PadInput::GamepadOptions::VibrationStrength(value);
                if (value <= 0)
                {
                    PadInput::GamepadHaptics::Stop();
                }
            });
        advanced->Children.Add(std::make_shared<GamepadSetupPanel>([this] { PostReload(); }));
        advanced->Children.Add(std::make_shared<GamepadProfilePanel>([this] { PostReload(); }));
        _target = nullptr;

        if (focusedId.has_value())
        {
            if (TopLevel* top = TopLevel::GetTopLevel(this))
            {
                top->UpdateLayout();
            }
            FocusNavigator::Focus(ControllerNav::Find(*this, focusedId));
        }
    }

    void GamepadSettingsPanel::RefreshDevices()
    {
        const std::vector<PadInput::GamepadDeviceSnapshot> devices = PadInput::GamepadManager::Devices();
        std::string signature;
        bool first = true;
        for (const PadInput::GamepadDeviceSnapshot& device : devices)
        {
            if (!first)
            {
                signature += "|";
            }
            signature += device.DeviceId();
            first = false;
        }
        if (const std::optional<std::string> selected = PadInput::GamepadManager::SelectedDeviceId(); selected.has_value())
        {
            signature += *selected;
        }
        if (_devices != nullptr && signature == _deviceList)
        {
            return;
        }
        _deviceList = signature;
        const bool focused = _devices != nullptr && _devices->IsFocused();
        if (_devices != nullptr)
        {
            Children.Remove(_devices);
        }
        std::vector<std::string> labels{"Automatic (last used)"};
        for (const PadInput::GamepadDeviceSnapshot& device : devices)
        {
            labels.push_back(device.Name());
        }
        std::int32_t selected = 0;
        const std::optional<std::string> selectedId = PadInput::GamepadManager::SelectedDeviceId();
        for (std::size_t i = 0; i < devices.size(); i++)
        {
            if (selectedId == std::optional<std::string>(devices[i].DeviceId()))
            {
                selected = static_cast<std::int32_t>(i + 1);
            }
        }
        _devices = std::make_shared<ChoiceRow>("Controller", std::move(labels), selected);
        ControllerNav::Identify(*_devices, "controller.device");
        const auto row = _devices;
        row->Changed += [devices, row](ChoiceRow&)
        {
            PadInput::GamepadManager::SelectDevice(row->Index() == 0
                ? std::nullopt : std::optional<std::string>(Runtime::ManagedAt(devices, row->Index() - 1).DeviceId()));
        };
        Children.Insert(0, _devices);
        if (focused)
        {
            _devices->Focus();
        }
    }

    void GamepadSettingsPanel::PostReload()
    {
        const std::weak_ptr<std::uint8_t> lifetime = _lifetime;
        Threading::Dispatcher::UIThread().Post([this, lifetime]
        {
            if (!lifetime.expired())
            {
                Reload();
            }
        });
    }

    void GamepadSettingsPanel::RefreshLabels()
    {
        if (_presetRow != nullptr && _presetRow->Value() != PadInput::PadBindings::Preset())
        {
            _refreshing = true;
            const std::string preset = PadInput::PadBindings::Preset();
            std::int32_t index = 0;
            for (std::size_t i = 0; i < Presets.size(); i++)
            {
                if (preset == Presets[i])
                {
                    index = static_cast<std::int32_t>(i);
                    break;
                }
            }
            _presetRow->Index(index);
            _refreshing = false;
        }
        const std::optional<PadInput::GamepadDeviceSnapshot> active = PadInput::GamepadManager::ActiveDevice();
        const PadInput::GamepadFamily family = PadInput::GamepadOptions::GlyphStyle() == PadInput::GamepadFamily::Unknown
            ? active.has_value() ? active->Family() : PadInput::GamepadFamily::Generic
            : PadInput::GamepadOptions::GlyphStyle();
        if (family == _shownFamily && _shownBindings == PadInput::PadBindings::Revision())
        {
            return;
        }
        _shownFamily = family;
        _shownBindings = PadInput::PadBindings::Revision();
        for (Visual* ancestor : GetVisualAncestors())
        {
            auto* settings = dynamic_cast<SettingsView*>(ancestor);
            if (settings == nullptr)
            {
                continue;
            }
            for (Visual* descendant : settings->GetVisualDescendants())
            {
                if (auto* row = dynamic_cast<PadRow*>(descendant); row != nullptr)
                {
                    row->InvalidateVisual();
                }
            }
            break;
        }
    }

    void GamepadSettingsPanel::Number(const std::string& id, const std::string& label, float value,
        float min, float max, std::function<void(float)> changed)
    {
        auto row = std::make_shared<SliderRow>(label,
            Runtime::MathRoundToInt32(static_cast<double>(value) * 100.0),
            [](std::int32_t current) { return Runtime::ToString(static_cast<float>(current) / 100.0F, "0.00"); },
            210, static_cast<std::int32_t>(min * 100), static_cast<std::int32_t>(max * 100), 1);
        row->ValueChanged += [row, changed = std::move(changed)](SliderRow&)
        {
            changed(static_cast<float>(row->Value()) / 100.0F);
        };
        ControllerNav::Identify(*row, id);
        if (_target != nullptr)
        {
            _target->Children.Add(row);
        }
        else
        {
            Children.Add(row);
        }
    }

    void GamepadSettingsPanel::Flag(const std::string& id, const std::string& label, bool value,
        std::function<void(bool)> changed)
    {
        auto row = std::make_shared<ToggleRow>(label, value);
        row->Changed += [row, changed = std::move(changed)](ToggleRow&)
        {
            changed(row->On());
        };
        ControllerNav::Identify(*row, id);
        if (_target != nullptr)
        {
            _target->Children.Add(row);
        }
        else
        {
            Children.Add(row);
        }
    }

    void GamepadSettingsPanel::Choice(const std::string& id, const std::string& label,
        const std::vector<std::string>& options, std::int32_t selected, std::function<void(std::int32_t)> changed)
    {
        auto row = std::make_shared<ChoiceRow>(label, options, selected);
        if (label == "Control layout")
        {
            _presetRow = row;
        }
        row->Changed += [this, row, changed = std::move(changed)](ChoiceRow&)
        {
            if (!_refreshing)
            {
                changed(row->Index());
            }
        };
        ControllerNav::Identify(*row, id);
        if (_target != nullptr)
        {
            _target->Children.Add(row);
        }
        else
        {
            Children.Add(row);
        }
    }
}
