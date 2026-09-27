#include "AndroidApp.hpp"

#include "../MphRead.Native/GameState.hpp"
#include "../MphRead.Native/Mods/CrashReport.hpp"
#include "../MphRead.Native/Mods/DebugLog.hpp"
#include "../MphRead.Native/Mods/GameSettings.hpp"
#include "../MphRead.Native/Mods/InputSettings.hpp"
#include "../MphRead.Native/Mods/ThumbnailGenerator.hpp"
#include "../MphRead.Native/Mods/Launcher/Gui/StartScreen.hpp"
#include "../MphRead.Native/Mods/Launcher/Gui/UiScaleHost.hpp"
#include "../MphRead.Native/Mods/Launcher/Portable/GameFiles.hpp"
#include "../MphRead.Native/Mods/Launcher/Portable/LaunchPlan.hpp"
#include "../MphRead.Native/Mods/Launcher/Portable/LauncherPrefs.hpp"

#include <exception>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace
{
    void OnHomeDone(
        MphRead::Mods::Launcher::Gui::StartScreen& sender,
        MphRead::Mods::Launcher::LaunchPlan plan)
    {
        (void)sender;
        MphRead::Droid::AndroidAppOwner& owner =
            MphRead::Droid::GetAndroidAppOwner();
        if (plan.Kind() == MphRead::Mods::Launcher::LaunchKind::None)
        {
            owner.FinishMainActivityIfPresent();
            return;
        }
        owner.StartMatchIfMainActivityPresent(plan);
    }

    void OnHomeMatchRequested(
        MphRead::Mods::Launcher::Gui::StartScreen& sender,
        MphRead::Mods::Launcher::LaunchPlan plan)
    {
        (void)sender;
        MphRead::Droid::GetAndroidAppOwner()
            .StartMatchIfMainActivityPresent(plan);
    }
}

namespace MphRead::Droid
{
    std::shared_ptr<MphRead::Mods::Launcher::Gui::StartScreen>
        AndroidApp::_home{};

    std::shared_ptr<MphRead::Mods::Launcher::Gui::StartScreen>
        AndroidApp::Home() noexcept
    {
        return _home;
    }

    void AndroidApp::Initialize()
    {
        MphRead::Mods::CrashReport::Install();

        AndroidAppOwner& owner = GetAndroidAppOwner();
        owner.AddUnhandledExceptionRaiser(
            *this,
            [](std::exception_ptr exception)
            {
                MphRead::Mods::CrashReport::Report(exception, "android");
            });
        owner.AddFluentTheme(*this);
        owner.SetRequestedThemeVariantDark(*this);
        owner.BaseInitialize(*this);
    }

    void AndroidApp::OnFrameworkInitializationCompleted()
    {
        AndroidAppOwner& owner = GetAndroidAppOwner();
        AndroidActivityLifetime activity =
            owner.ActivityApplicationLifetime(*this);
        if (activity)
        {
            owner.SetActivityMainViewFactory(
                activity,
                []() -> Av::Controls::ControlPtr
                {
                    // Avalonia invokes this from MainActivity.OnCreate, after
                    // the Activity-only services used by BuildHome exist.
                    _home = BuildHome();
                    return std::make_shared<
                        MphRead::Mods::Launcher::Gui::UiScaleHost>(_home);
                });
        }
        else
        {
            AndroidSingleViewLifetime single =
                owner.SingleViewApplicationLifetime(*this);
            if (single)
            {
                _home = BuildHome();
                owner.SetSingleViewMainView(
                    single,
                    std::make_shared<
                        MphRead::Mods::Launcher::Gui::UiScaleHost>(_home));
            }
        }
        owner.BaseOnFrameworkInitializationCompleted(*this);
    }

    std::shared_ptr<MphRead::Mods::Launcher::Gui::StartScreen>
        AndroidApp::BuildHome()
    {
        using MphRead::Mods::Launcher::Gui::StartScreen;

        MphRead::Mods::Launcher::LauncherPrefs::Load();
        MphRead::Mods::InputSettings::Load();
        MphRead::Mods::DebugLog::Attach();

        std::shared_ptr<MphRead::MenuSettings> settings =
            MphRead::GameState::LoadSettings();
        MphRead::Mods::GameSettings::Apply(settings);

        std::vector<std::string> rooms;
        if (MphRead::Mods::Launcher::GameFiles::Ready())
        {
            MphRead::Mods::Launcher::GameFiles::ApplyPaths();
            rooms = MphRead::Mods::ThumbnailGenerator::MultiplayerRooms();
        }

        std::shared_ptr<StartScreen> home = StartScreen::Create(settings, rooms);
        home->Done += &OnHomeDone;
        home->MatchRequested += &OnHomeMatchRequested;
        return home;
    }
}
