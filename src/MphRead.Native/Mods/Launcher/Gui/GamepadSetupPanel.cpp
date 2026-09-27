#include "GamepadSetupPanel.hpp"

#include "ControllerNav.hpp"
#include "UiWord.hpp"
#include "../../../Mods/Input/GamepadManager.hpp"
#include "../../../Mods/Input/GamepadMappings.hpp"
#include "../../../Mods/Input/GamepadUiRouter.hpp"
#include "../../../NativeRuntime/System/Exceptions.hpp"
#include "../../../NativeRuntime/System/Runtime.hpp"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>

namespace MphRead::Mods::Launcher::Gui
{
    namespace Runtime = ::MphRead::NativeRuntime;

    GamepadSetupPanel::GamepadSetupPanel(std::function<void()> changed,
        std::function<std::int64_t()> clock)
        : _status(std::make_shared<Note>(
            "Calibration measures drift and trigger travel. Manual mapping is available on desktop.")),
          _changed(std::move(changed)),
          _clock(clock ? std::move(clock) : std::function<std::int64_t()>(Runtime::EnvironmentTickCount64))
    {
        _timer.Interval(Av::Threading::TimeSpan(0.05));
        _timer.Tick += [this](Av::Threading::DispatcherTimer&) { Tick(); };
        Spacing(8);
        const auto button = [this](const std::string& id, const std::string& label, std::function<void()> action)
        {
            auto word = std::make_shared<UiWord>(label, 13);
            ControllerNav::Identify(*word, id);
            word->Click += [action = std::move(action)](UiWord&) { action(); };
            Children.Add(word);
        };

        button("setup.calibrate_sticks_and_triggers", "Calibrate sticks and triggers",
            [this] { Start(false); });
        if (!Runtime::IsAndroid())
        {
            button("setup.map_controller_buttons_and_axes", "Map controller buttons and axes",
                [this] { Start(true); });
            button("setup.reset_custom_controller_mappings", "Reset custom controller mappings", [this]
            {
                try
                {
                    PadInput::GamepadMappings::ResetOverrides();
                    Stop("Custom mappings reset. Restart the game to restore platform mappings.");
                }
                catch (const System::IO::IOException& ex)
                {
                    _status->Text(ex.what());
                }
                catch (const System::UnauthorizedAccessException& ex)
                {
                    _status->Text(ex.what());
                }
            });
        }

        _apply = std::make_shared<UiWord>("Apply measured setup", 13);
        _apply->IsEnabled(false);
        ControllerNav::Identify(*_apply, "setup.apply");
        _apply->Click += [this](UiWord&) { Apply(); };
        Children.Add(_apply);
        button("setup.cancel_setup", "Cancel setup", [this]
        {
            Stop("Setup canceled. Settings unchanged.");
        });
        Children.Add(_status);
        DetachedFromVisualTree += [this](Av::Controls::Control&) { Stop(""); };
    }

    void GamepadSetupPanel::Start(bool mapping)
    {
        Stop("");
        const PadInput::GamepadSnapshot snapshot = PadInput::GamepadManager::Snapshot();
        if (!snapshot.DeviceId.has_value())
        {
            _status->Text("Connect and select a controller first.");
            return;
        }
        _buttons = snapshot.State.Buttons;
        _device = snapshot.DeviceId;
        _revision = snapshot.Revision;
        _mappingMode = mapping;
        _started = _clock();
        _complete = false;
        _calibration = mapping ? nullptr : std::make_unique<PadInput::GamepadCalibration>();
        _mapping.reset();
        PadInput::GamepadContexts::Capturing(true);
        if (mapping)
        {
            PadInput::GamepadMappingWizard::Latest.reset();
            PadInput::GamepadMappingWizard::RequestedDevice = _device;
        }
        _status->Text("Release all controls. Keep both sticks centered. Esc or Cancel stops setup.");
        _timer.Start();
    }

    void GamepadSetupPanel::Tick()
    {
        if (!_device.has_value() || _complete)
        {
            return;
        }
        const PadInput::GamepadSnapshot snapshot = PadInput::GamepadManager::Snapshot();
        if (!PadInput::GamepadContexts::Focused() || snapshot.DeviceId != _device || snapshot.Revision != _revision)
        {
            Stop("Controller or focus changed. Restart setup.");
            return;
        }
        const std::int64_t elapsed = _clock() - _started;
        const PadInput::GamepadButtons pressed = snapshot.State.Buttons & ~_buttons;
        _buttons = snapshot.State.Buttons;
        if (!_mappingMode && PadInput::Any(pressed & PadInput::GamepadButtons::B))
        {
            Stop("Calibration canceled. Settings unchanged.");
            return;
        }

        if (_mappingMode)
        {
            const std::shared_ptr<PadInput::GamepadRawSample> sample = PadInput::GamepadMappingWizard::Latest;
            if (sample == nullptr || sample->DeviceId != *_device)
            {
                return;
            }
            if (_mapping == nullptr)
            {
                if (std::any_of(sample->Buttons.begin(), sample->Buttons.end(), [](bool value) { return value; })
                    || std::any_of(sample->Hats.begin(), sample->Hats.end(), [](std::uint8_t value) { return value != 0; }))
                {
                    _started = _clock();
                    return;
                }
                if (elapsed < 1500)
                {
                    return;
                }
                _mapping = std::make_unique<PadInput::GamepadMappingWizard>(*sample);
            }
            try
            {
                _mapping->Sample(*sample);
            }
            catch (const System::InvalidOperationException& ex)
            {
                Stop(ex.what());
                return;
            }
            _status->Text(_mapping->Prompt() + " Esc or Cancel stops setup.");
            _complete = _mapping->Complete();
        }
        else
        {
            const std::optional<PadInput::GamepadDeviceSnapshot> device = PadInput::GamepadManager::ActiveDevice();
            if (!device.has_value())
            {
                return;
            }
            // Allow the button that opened setup to be released before measuring rest.
            if (elapsed < 1000)
            {
                return;
            }
            _calibration->Sample(device->RawState, elapsed < 3500);
            if (elapsed < 3500)
            {
                _status->Text("Keep sticks and triggers released. Measuring rest… B cancels.");
            }
            else
            {
                const std::int64_t seconds = std::max<std::int64_t>(0, (10500 - elapsed) / 1000);
                _status->Text("Rotate both sticks fully and squeeze/release both triggers. "
                    + std::to_string(seconds) + " seconds remaining. B cancels.");
            }
            if (elapsed >= 10500)
            {
                _complete = true;
                _status->Text(_calibration->Summary());
            }
        }

        if (_complete)
        {
            _timer.Stop();
            PadInput::GamepadContexts::Capturing(false);
            PadInput::GamepadMappingWizard::RequestedDevice.reset();
            _apply->IsEnabled(_mappingMode || _calibration->Valid());
        }
    }

    void GamepadSetupPanel::Apply()
    {
        if (!_complete)
        {
            return;
        }
        if (PadInput::GamepadManager::Snapshot().DeviceId != _device)
        {
            Stop("Controller changed. Restart setup.");
            return;
        }
        try
        {
            if (_mappingMode)
            {
                PadInput::GamepadMappings::SaveOverride(_mapping->Mapping());
            }
            else
            {
                _calibration->Apply();
            }
            Stop("Setup applied. Save settings or a named profile to retain calibration.");
            _changed();
        }
        catch (const System::IO::IOException& ex)
        {
            _status->Text("Could not apply setup: " + std::string(ex.what()));
        }
        catch (const System::UnauthorizedAccessException& ex)
        {
            _status->Text("Could not apply setup: " + std::string(ex.what()));
        }
        catch (const System::ArgumentException& ex)
        {
            _status->Text("Could not apply setup: " + std::string(ex.what()));
        }
        catch (const System::InvalidOperationException& ex)
        {
            _status->Text("Could not apply setup: " + std::string(ex.what()));
        }
    }

    void GamepadSetupPanel::Stop(const std::string& message)
    {
        if (_device.has_value())
        {
            PadInput::GamepadContexts::Capturing(false);
        }
        _device.reset();
        _complete = false;
        _timer.Stop();
        _apply->IsEnabled(false);
        PadInput::GamepadMappingWizard::RequestedDevice.reset();
        PadInput::GamepadMappingWizard::Latest.reset();
        _status->Text(message);
    }

    void GamepadSetupPanel::OnKeyDown(Av::Input::KeyEventArgs& e)
    {
        if (_device.has_value() && e.Key == Av::Input::Key::Escape)
        {
            Stop("Setup canceled. Settings unchanged.");
            e.Handled = true;
        }
        StackPanel::OnKeyDown(e);
    }
}
