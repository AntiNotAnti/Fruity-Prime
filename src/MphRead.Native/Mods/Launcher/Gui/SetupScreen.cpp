#include "SetupScreen.hpp"

#include "../Portable/GameFiles.hpp"
#include "../Portable/NativeFilePicker.hpp"
#include "../Portable/SetupProgress.hpp"
#include "../../Branding.hpp"
#include "../../ThumbnailGenerator.hpp"
#include "../../ThumbnailHost.hpp"
#include "GuiTheme.hpp"
#include "ProgressRow.hpp"
#include "Rows.hpp"
#include "UiLayout.hpp"
#include "UiMark.hpp"
#include "UiWord.hpp"
#include "../../../NativeRuntime/Avalonia/Threading.hpp"

#include <chrono>
#include <future>
#include <memory>
#include <optional>
#include <sstream>
#include <utility>
#include <vector>

namespace MphRead::Mods::Launcher::Gui
{
    using namespace ::MphRead::NativeRuntime::Avalonia;

    namespace
    {
        template <typename Result, typename Continuation>
        void Await(std::shared_future<Result> task, Continuation continuation)
        {
            const auto ready = std::future_status::ready;
            if (task.wait_for(std::chrono::seconds(0)) == ready)
            {
                continuation(task.get());
                return;
            }

            Threading::DispatcherTimer::Run(
                [task = std::move(task), continuation = std::move(continuation)]() mutable
                {
                    if (task.wait_for(std::chrono::seconds(0)) != std::future_status::ready)
                    {
                        return true;
                    }
                    continuation(task.get());
                    return false;
                },
                Threading::TimeSpan(0.016), Threading::DispatcherPriority::Normal);
        }
    }

    SetupScreen::SetupScreen()
        : _log(std::make_shared<Note>("", std::nullopt, 0)),
          _progress(std::make_shared<ProgressRow>()),
          _choose(std::make_shared<UiMark>(UiMark::Shape::Accept, "choose your .nds file")),
          _back(std::make_shared<UiMark>(UiMark::Shape::Cancel, "back")),
          _previews(std::make_shared<UiWord>("Render map previews", 15, nullptr, GuiTheme::TextDim))
    {
        Background(Media::Brushes::Transparent());
        Focusable(true);

        auto body = std::make_shared<Controls::StackPanel>();
        body->Spacing(8);
        body->Children.Add(std::make_shared<Note>(
            std::string(::MphRead::Mods::Branding::Name)
                + " needs your own Metroid Prime Hunters cartridge dump. It unpacks what "
                + "it needs next to this program and leaves the file alone. No game data "
                + "is included in this download, and none is downloaded.",
            std::nullopt, 0));
        if (::MphRead::Mods::Launcher::GameFiles::InProcessSetup())
        {
            body->Children.Add(std::make_shared<Note>(
                "The unpacked files land in " + ::MphRead::Mods::Launcher::GameFiles::Root()
                    + " -- this device's own folder for the app, which shows up over USB "
                    + "under Android/data. Files already copied there are found without "
                    + "picking anything.",
                GuiTheme::TextDim, 0));
        }

        _previews->Click += [this](UiWord&)
        {
            RenderPreviews();
        };
        body->Children.Add(_previews);

        _progress->IsVisible(false);
        body->Children.Add(_progress);

        auto logScroll = std::make_shared<Controls::ScrollViewer>();
        logScroll->Height(160);
        logScroll->Content(_log);
        logScroll->HorizontalScrollBarVisibility(Controls::ScrollBarVisibility::Disabled);
        body->Children.Add(logScroll);

        _choose->Click += [this](UiMark&)
        {
            ChooseRom();
        };
        _back->IsVisible(::MphRead::Mods::Launcher::GameFiles::Ready());
        _back->Click += [this](UiMark&)
        {
            const std::shared_ptr<SetupScreen> keepAlive = Self();
            keepAlive->Closed(*keepAlive);
        };

        auto holder = std::make_shared<Controls::ScrollViewer>();
        holder->Content(body);
        holder->HorizontalScrollBarVisibility(Controls::ScrollBarVisibility::Disabled);
        Content(UiLayout::Page(false, UiLayout::WellSettings, "game files", nullptr,
            holder, _back, _choose));
        RefreshPreviewEntry();

        const std::weak_ptr<UiMark> choose = _choose;
        AttachedToVisualTree += [choose](Controls::Control&)
        {
            Threading::Dispatcher::UIThread().Post(
                [choose]
                {
                    if (const std::shared_ptr<UiMark> mark = choose.lock())
                    {
                        mark->Focus();
                    }
                },
                Threading::DispatcherPriority::Background);
        };
    }

    std::shared_ptr<SetupScreen> SetupScreen::Self()
    {
        return std::static_pointer_cast<SetupScreen>(shared_from_this());
    }

    void SetupScreen::OnKeyDown(Input::KeyEventArgs& e)
    {
        if (e.Key == Input::Key::Escape && _back->IsVisible())
        {
            const std::shared_ptr<SetupScreen> keepAlive = Self();
            keepAlive->Closed(*keepAlive);
            e.Handled = true;
            return;
        }
        UserControl::OnKeyDown(e);
    }

    void SetupScreen::ChooseRom()
    {
        if (GetVisualRoot() == nullptr)
        {
            return;
        }
        if (!::MphRead::Mods::Launcher::NativeFilePicker::Available())
        {
            _log->Text("This desktop has no file dialog to open. "
                "Install zenity or kdialog and press this again.");
            return;
        }

        const std::shared_ptr<SetupScreen> self = Self();
        auto picker = ::MphRead::Mods::Launcher::NativeFilePicker::OpenFile(
            "Your Metroid Prime Hunters cartridge dump", "Nintendo DS ROM", "nds");
        Await(std::move(picker), [self](std::optional<std::string> chosen)
        {
            if (chosen.has_value())
            {
                self->RunSetup(std::move(*chosen));
            }
        });
    }

    void SetupScreen::RunSetup(std::string path)
    {
        const std::shared_ptr<SetupScreen> self = Self();
        _choose->IsEnabled(false);
        _choose->Label("working...");
        _log->Text("");
        const auto progress = std::make_shared<::MphRead::Mods::Launcher::SetupProgress>();
        _progress->IsVisible(true);
        _progress->Set(0, "Starting");

        auto task = std::async(std::launch::async,
            [self, path = std::move(path), progress]
            {
                return ::MphRead::Mods::Launcher::GameFiles::RunSetup(path,
                    [self, progress](const std::string& line)
                    {
                        Threading::Dispatcher::UIThread().Post(
                            [self, progress, line]
                            {
                                self->ReportSetupLine(progress, line);
                            });
                    });
            }).share();

        Await(std::move(task), [self, progress](bool ok)
        {
            if (ok)
            {
                self->RenderMissing(progress, [self, progress, ok]
                {
                    self->FinishSetup(progress, ok);
                });
            }
            else
            {
                self->FinishSetup(progress, ok);
            }
        });
    }

    void SetupScreen::ReportSetupLine(
        const std::shared_ptr<::MphRead::Mods::Launcher::SetupProgress>& progress, std::string line)
    {
        _log->Text(Tail(_log->Text(), line));
        if (progress->Observe(line))
        {
            _progress->Set(progress->Fraction(), progress->Stage());
        }
    }

    void SetupScreen::FinishSetup(
        const std::shared_ptr<::MphRead::Mods::Launcher::SetupProgress>& progress, bool ok)
    {
        progress->Finish(ok);
        _progress->Set(progress->Fraction(), progress->Stage());
        _choose->IsEnabled(true);
        _choose->Label("choose your .nds file");
        _log->Text(Tail(_log->Text(), ok ? "Ready to play." : "Setup did not finish."));
        RefreshPreviewEntry();
        if (ok)
        {
            _progress->IsVisible(false);
            _back->IsVisible(true);
            const std::shared_ptr<SetupScreen> keepAlive = Self();
            keepAlive->Closed(*keepAlive);
        }
    }

    void SetupScreen::RenderPreviews()
    {
        const std::shared_ptr<SetupScreen> self = Self();
        _previews->IsEnabled(false);
        _previews->Text("Rendering...");
        RenderMissing(nullptr, [self]
        {
            self->_previews->IsEnabled(true);
            self->_previews->Text("Render map previews");
            self->RefreshPreviewEntry();
        });
    }

    void SetupScreen::RenderMissing(
        const std::shared_ptr<::MphRead::Mods::Launcher::SetupProgress>& progress,
        std::function<void()> completed)
    {
        if (!ThumbnailHost::CanRender())
        {
            completed();
            return;
        }

        const std::shared_ptr<SetupScreen> self = Self();
        _log->Text(Tail(_log->Text(), "Rendering map previews..."));
        auto task = ThumbnailHost::RenderMissingAsync(
            [self, progress](const std::string& line)
            {
                Threading::Dispatcher::UIThread().Post(
                    [self, progress, line]
                    {
                        self->_log->Text(Tail(self->_log->Text(), line));
                        if (progress != nullptr && progress->Observe(line))
                        {
                            self->_progress->Set(progress->Fraction(), progress->Stage());
                        }
                    });
            });
        Await(std::move(task), [completed = std::move(completed)](int)
        {
            completed();
        });
    }

    void SetupScreen::RefreshPreviewEntry()
    {
        if (!::MphRead::Mods::Launcher::GameFiles::Ready() || !ThumbnailHost::CanRender())
        {
            _previews->IsVisible(false);
            return;
        }
        const std::size_t missing = ThumbnailGenerator::MissingThumbnails().size();
        _previews->IsVisible(missing > 0);
        _previews->IsEnabled(missing > 0);
    }

    std::string SetupScreen::Tail(const std::string& existing, const std::string& line)
    {
        std::istringstream input(existing + "\n" + line);
        std::vector<std::string> lines;
        std::string current;
        while (std::getline(input, current))
        {
            if (!current.empty())
            {
                lines.push_back(std::move(current));
            }
        }
        if (lines.size() > 8)
        {
            lines.erase(lines.begin(), lines.end() - 8);
        }

        std::string result;
        for (const std::string& item : lines)
        {
            if (!result.empty())
            {
                result.push_back('\n');
            }
            result += item;
        }
        return result;
    }
}
