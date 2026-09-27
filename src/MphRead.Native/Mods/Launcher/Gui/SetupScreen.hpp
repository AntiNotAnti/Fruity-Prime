#pragma once

#include "../../../NativeRuntime/Avalonia/Avalonia.hpp"

#include <functional>
#include <memory>
#include <string>

namespace MphRead::Mods::Launcher
{
    class SetupProgress;
}

namespace MphRead::Mods::Launcher::Gui
{
    namespace Av = ::MphRead::NativeRuntime::Avalonia;
    class Note;
    class ProgressRow;
    class UiMark;
    class UiWord;

    // The one screen a fresh install needs before anything else can happen.
    class SetupScreen final : public Av::Controls::UserControl
    {
    public:
        SetupScreen();

        Av::Event<SetupScreen&> Closed;

    protected:
        void OnKeyDown(Av::Input::KeyEventArgs& e) override;

    private:
        [[nodiscard]] std::shared_ptr<SetupScreen> Self();
        void ChooseRom();
        void RunSetup(std::string path);
        void ReportSetupLine(
            const std::shared_ptr<::MphRead::Mods::Launcher::SetupProgress>& progress, std::string line);
        void FinishSetup(
            const std::shared_ptr<::MphRead::Mods::Launcher::SetupProgress>& progress, bool ok);
        void RenderPreviews();
        void RenderMissing(const std::shared_ptr<::MphRead::Mods::Launcher::SetupProgress>& progress,
            std::function<void()> completed);
        void RefreshPreviewEntry();

        [[nodiscard]] static std::string Tail(const std::string& existing, const std::string& line);

        std::shared_ptr<Note> _log;
        std::shared_ptr<ProgressRow> _progress;
        std::shared_ptr<UiMark> _choose;
        std::shared_ptr<UiMark> _back;
        std::shared_ptr<UiWord> _previews;
    };
}
