#pragma once

#include "Rows.hpp"
#include "../../../Mods/Input/GamepadCalibration.hpp"
#include "../../../Mods/Input/GamepadMappingWizard.hpp"
#include "../../../NativeRuntime/Avalonia/Avalonia.hpp"

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>

namespace MphRead::Mods::Launcher::Gui
{
    namespace Av = ::MphRead::NativeRuntime::Avalonia;
    namespace PadInput = ::MphRead::Mods::Input;

    class UiWord;

    // The controller calibration and manual mapping workflow shown in Settings.
    class GamepadSetupPanel final : public Av::Controls::StackPanel
    {
    public:
        explicit GamepadSetupPanel(std::function<void()> changed,
            std::function<std::int64_t()> clock = {});

        void Tick();

    protected:
        void OnKeyDown(Av::Input::KeyEventArgs& e) override;

    private:
        void Start(bool mapping);
        void Apply();
        void Stop(const std::string& message);

        std::shared_ptr<Note> _status;
        Av::Threading::DispatcherTimer _timer;
        std::function<void()> _changed;
        std::function<std::int64_t()> _clock;
        PadInput::GamepadButtons _buttons = PadInput::GamepadButtons::None;
        std::shared_ptr<UiWord> _apply;
        std::unique_ptr<PadInput::GamepadCalibration> _calibration;
        std::unique_ptr<PadInput::GamepadMappingWizard> _mapping;
        std::optional<std::string> _device;
        std::int64_t _started = 0;
        std::int64_t _revision = 0;
        bool _mappingMode = false;
        bool _complete = false;
    };
}
