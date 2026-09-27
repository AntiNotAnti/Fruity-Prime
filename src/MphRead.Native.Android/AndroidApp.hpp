#pragma once

#if !defined(__ANDROID__)
#error "AndroidApp is only valid for the Android native target."
#endif

#include "../MphRead.Native/NativeRuntime/Avalonia/Avalonia.hpp"

#include <exception>
#include <functional>
#include <memory>

namespace MphRead::Mods::Launcher
{
    class LaunchPlan;
}

namespace MphRead::Mods::Launcher::Gui
{
    class StartScreen;
}

namespace MphRead::Droid
{
    namespace Av = ::MphRead::NativeRuntime::Avalonia;

    class AndroidApp;

    struct AndroidActivityLifetime final
    {
        std::shared_ptr<void> Native{};

        [[nodiscard]] explicit operator bool() const noexcept
        {
            return Native != nullptr;
        }
    };

    struct AndroidSingleViewLifetime final
    {
        std::shared_ptr<void> Native{};

        [[nodiscard]] explicit operator bool() const noexcept
        {
            return Native != nullptr;
        }
    };

    using AndroidMainViewFactory =
        std::function<Av::Controls::ControlPtr()>;
    using AndroidUnhandledExceptionHandler =
        std::function<void(std::exception_ptr)>;

    // The owner supplies Android/Avalonia lifecycle calls. AndroidApp keeps
    // the branch order, deferred Home creation and view wrapping from the C#
    // application itself.
    class AndroidAppOwner
    {
    public:
        virtual ~AndroidAppOwner() = default;

        virtual void AddUnhandledExceptionRaiser(
            AndroidApp& application,
            AndroidUnhandledExceptionHandler handler) = 0;
        virtual void AddFluentTheme(AndroidApp& application) = 0;
        virtual void SetRequestedThemeVariantDark(AndroidApp& application) = 0;
        virtual void BaseInitialize(AndroidApp& application) = 0;

        [[nodiscard]] virtual AndroidActivityLifetime
            ActivityApplicationLifetime(AndroidApp& application) = 0;
        virtual void SetActivityMainViewFactory(
            const AndroidActivityLifetime& lifetime,
            AndroidMainViewFactory factory) = 0;

        [[nodiscard]] virtual AndroidSingleViewLifetime
            SingleViewApplicationLifetime(AndroidApp& application) = 0;
        virtual void SetSingleViewMainView(
            const AndroidSingleViewLifetime& lifetime,
            Av::Controls::ControlPtr mainView) = 0;

        virtual void BaseOnFrameworkInitializationCompleted(
            AndroidApp& application) = 0;

        virtual void FinishMainActivityIfPresent() = 0;
        virtual void StartMatchIfMainActivityPresent(
            const MphRead::Mods::Launcher::LaunchPlan& plan) = 0;
    };

    [[nodiscard]] AndroidAppOwner& GetAndroidAppOwner() noexcept;

    class AndroidApp final
    {
    public:
        AndroidApp() = default;
        ~AndroidApp() = default;

        AndroidApp(const AndroidApp&) = delete;
        AndroidApp& operator=(const AndroidApp&) = delete;
        AndroidApp(AndroidApp&&) = delete;
        AndroidApp& operator=(AndroidApp&&) = delete;

        [[nodiscard]] static std::shared_ptr<
            MphRead::Mods::Launcher::Gui::StartScreen> Home() noexcept;

        void Initialize();
        void OnFrameworkInitializationCompleted();

    private:
        [[nodiscard]] static std::shared_ptr<
            MphRead::Mods::Launcher::Gui::StartScreen> BuildHome();

        static std::shared_ptr<
            MphRead::Mods::Launcher::Gui::StartScreen> _home;
    };
}
