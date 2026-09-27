#pragma once

#include <optional>
#include <memory>
#include <string>

namespace MphRead::Mods::Launcher::Gui
{
    class UiTopLevelImpl;
    class SettingsView;
}

namespace MphRead::Mods::Launcher::Gui
{
    class GamepadUiChecks final
    {
    public:
        GamepadUiChecks() = delete;

        static void Run(const std::optional<std::string>& shots = std::nullopt);

    private:
        static void CheckControllerSettings(UiTopLevelImpl& topLevel,
            const std::shared_ptr<SettingsView>& settings,
            const std::optional<std::string>& shots);
    };
}
