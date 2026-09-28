#pragma once

#include "../../../NativeRuntime/Avalonia/Avalonia.hpp"
#include "../../../Mods/Input/GamepadManager.hpp"

#include <array>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace MphRead::Mods::Launcher::Gui
{
    namespace Av = ::MphRead::NativeRuntime::Avalonia;
    namespace PadInput = ::MphRead::Mods::Input;

    class ChoiceRow;

    class GamepadSettingsPanel final : public Av::Controls::StackPanel
    {
    public:
        GamepadSettingsPanel();

        void Reload();
        void RefreshLabels();

    private:
        void RefreshDevices();
        void PostReload();
        void Number(const std::string& id, const std::string& label, float value, float min, float max,
            std::function<void(float)> changed);
        void Flag(const std::string& id, const std::string& label, bool value,
            std::function<void(bool)> changed);
        void Choice(const std::string& id, const std::string& label, const std::vector<std::string>& options,
            std::int32_t selected, std::function<void(std::int32_t)> changed);

        Av::Threading::DispatcherTimer _timer;
        std::string _deviceList;
        std::shared_ptr<ChoiceRow> _devices;
        std::shared_ptr<ChoiceRow> _presetRow;
        bool _refreshing = false;
        PadInput::GamepadFamily _shownFamily{};
        std::int64_t _shownBindings = -1;
        std::int64_t _profileRevision = 0;
        std::int64_t _runtimeRevision = -1;
        Av::Controls::StackPanel* _target = nullptr;
        bool _advancedOpen = false;
        std::shared_ptr<std::uint8_t> _lifetime = std::make_shared<std::uint8_t>(0);

        inline static constexpr std::array<std::string_view, 5> Presets{
            "Default", "Bumper Jumper", "Southpaw", "Classic", "Custom"};
    };
}
