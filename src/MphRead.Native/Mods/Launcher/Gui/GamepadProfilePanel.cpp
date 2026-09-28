#include "GamepadProfilePanel.hpp"

#include "ControllerNav.hpp"
#include "Rows.hpp"
#include "UiWord.hpp"
#include "../../../Mods/Input/GamepadManager.hpp"
#include "../../../Mods/Input/GamepadProfiles.hpp"
#include "../Portable/LauncherPrefs.hpp"
#include "../../../NativeRuntime/System/Exceptions.hpp"
#include "../../../NativeRuntime/System/IO.hpp"
#include "../../../NativeRuntime/System/Runtime.hpp"

#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace MphRead::Mods::Launcher::Gui
{
    namespace Runtime = ::MphRead::NativeRuntime;
    namespace PadInput = ::MphRead::Mods::Input;

    GamepadProfilePanel::GamepadProfilePanel(std::function<void()> changed)
    {
        Spacing(8);
        PadInput::GamepadProfiles::Initialize();
        Children.Add(std::make_shared<Note>("Controller profiles — " + PadInput::GamepadProfiles::ActiveName()));

        std::vector<std::string> names;
        for (const PadInput::GamepadProfile& profile : PadInput::GamepadProfiles::Profiles())
        {
            names.push_back(profile.Name);
        }
        const auto choice = std::make_shared<ChoiceRow>("Saved profile",
            names.empty() ? std::vector<std::string>{"No saved profiles"} : std::move(names), 0);
        const auto name = std::make_shared<FieldRow>("Profile name", "My controller", 260);
        const auto file = std::make_shared<FieldRow>("Import / export file",
            Runtime::PathCombine(::MphRead::Mods::Launcher::LauncherPrefs::Directory(), "controller-profile.json"), 360);
        Children.Add(choice);
        Children.Add(name);
        Children.Add(file);
        const auto status = std::make_shared<Note>(PadInput::GamepadProfiles::Status());

        const auto button = [this, status](const std::string& id, const std::string& label,
            std::function<void()> action)
        {
            auto word = std::make_shared<UiWord>(label, 13);
            ControllerNav::Identify(*word, id);
            word->Click += [action = std::move(action), status](UiWord&)
            {
                try
                {
                    action();
                    status->Text("Done.");
                }
                catch (const System::IO::InvalidDataException& ex)
                {
                    status->Text(ex.what());
                }
                catch (const System::IO::IOException& ex)
                {
                    status->Text(ex.what());
                }
                catch (const System::UnauthorizedAccessException& ex)
                {
                    status->Text(ex.what());
                }
                catch (const System::ArgumentException& ex)
                {
                    status->Text(ex.what());
                }
                catch (const System::Text::Json::JsonException& ex)
                {
                    status->Text(ex.what());
                }
            };
            Children.Add(word);
        };

        button("profile.save_current_as_named_profile", "Save current as named profile",
            [name, changed]
            {
                PadInput::GamepadProfiles::Save(name->Value());
                changed();
            });
        button("profile.load_selected_profile", "Load selected profile", [choice, changed]
        {
            PadInput::GamepadProfiles::Load(choice->Value());
            changed();
        });
        button("profile.use_selected_profile_for_this_controller", "Use selected profile for this controller",
            [choice, changed]
            {
                const std::optional<PadInput::GamepadDeviceSnapshot> device = PadInput::GamepadManager::ActiveDevice();
                if (!device.has_value())
                {
                    throw System::IO::InvalidDataException("Connect and select a controller first.");
                }
                PadInput::GamepadProfiles::Assign(choice->Value(), *device);
                changed();
            });
        button("profile.remove_automatic_profile_assignment", "Remove automatic profile assignment", [changed]
        {
            const std::optional<PadInput::GamepadDeviceSnapshot> device = PadInput::GamepadManager::ActiveDevice();
            if (!device.has_value())
            {
                throw System::IO::InvalidDataException("Connect and select a controller first.");
            }
            PadInput::GamepadProfiles::Unassign(*device);
            changed();
        });
        button("profile.export_selected_profile_to_file", "Export selected profile to file", [choice, file]
        {
            PadInput::GamepadProfiles::Export(choice->Value(), file->Value());
        });
        button("profile.import_profile_from_file", "Import profile from file", [file, changed]
        {
            PadInput::GamepadProfiles::Import(file->Value());
            changed();
        });

        Children.Add(status);
        Children.Add(std::make_shared<Note>("Profiles contain controller settings only. Save replaces a profile with the same name. "
            "Import adds a profile; load it to apply. Desktop automatic selection identifies the controller model and firmware; "
            "identical controllers share that assignment."));
    }
}
