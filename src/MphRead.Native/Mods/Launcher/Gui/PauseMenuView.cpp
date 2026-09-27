#include "PauseMenuView.hpp"

#include "../../Network/DemoPlayback.hpp"
#include "../../Network/DemoRecorder.hpp"
#include "../../Network/MapVote.hpp"
#include "../../Network/NetSession.hpp"
#include "../../SpectatorMode.hpp"
#include "../../WindowMode.hpp"
#include "UiLayout.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <functional>
#include <utility>

namespace MphRead::Mods::Launcher::Gui
{
    using namespace ::MphRead::NativeRuntime::Avalonia;

    PauseMenuView::PauseMenuView(bool offerWindowMode)
        : _menu(std::make_shared<Controls::StackPanel>()),
          _scaler(std::make_shared<Controls::LayoutTransformControl>())
    {
        Background(Media::Brushes::Transparent());
        Focusable(true);

        _menu->Spacing(6);
        _menu->Width(230);
        _resume = Add(_menu, "Resume", [this] { Resumed(*this); }, Deck::Face::Moss());
        _voteYes = Add(_menu, "Accept map vote", [this] { AnswerVote(true); }, Deck::Face::Moss());
        _voteNo = Add(_menu, "Deny map vote", [this] { AnswerVote(false); }, Deck::Face::Rust());
        RefreshVote();

        _voteTimer.Interval(Threading::TimeSpan(0.2));
        _voteTimer.Tick += [this](Threading::DispatcherTimer&) { RefreshVote(); };
        AttachedToVisualTree += [this](Controls::Control&) { _voteTimer.Start(); };
        DetachedFromVisualTree += [this](Controls::Control&) { _voteTimer.Stop(); };

        if (!Network::DemoPlayback::IsActive() && Network::NetSession::Active())
        {
            Add(_menu, "Vote map", [this] { VoteMapRequested(*this); });
        }
        if (!Network::DemoPlayback::IsActive())
        {
            if (SpectatorMode::IsSpectating())
            {
                Add(_menu, "Rejoin match", [this] { RejoinRequested(*this); });
            }
            else if (SpectatorMode::CanSpectate())
            {
                Add(_menu, "Spectate", [this] { SpectateRequested(*this); });
            }
        }
        if (offerWindowMode)
        {
            Add(_menu, WindowLabel(), [this] { FullscreenRequested(*this); });
        }
        if (!Network::DemoPlayback::IsActive() && Network::NetSession::Active())
        {
            Add(_menu, Network::DemoRecorder::IsRecording() ? "Stop recording" : "Record demo",
                [this] { RecordToggleRequested(*this); });
        }
        Add(_menu, "Settings", [this] { SettingsRequested(*this); });
            Add(_menu, "Leave match", [this] { LeaveRequested(*this); }, Deck::Face::Brass());
        Add(_menu, "Quit", [this] { QuitRequested(*this); }, Deck::Face::Rust());

        for (const Controls::ControlPtr& child : _menu->Children)
        {
            child->HorizontalAlignment(Layout::HorizontalAlignment::Center);
        }

        _scaler->Child(_menu);
        _scaler->HorizontalAlignment(Layout::HorizontalAlignment::Center);
        _scaler->VerticalAlignment(Layout::VerticalAlignment::Center);
        Content(UiLayout::Page(true, UiLayout::WellShort, "", nullptr, _scaler,
            nullptr, nullptr, true));
        SizeChanged += [this](Controls::Control&, Size size) { FitToHost(size.Height); };
    }

    double PauseMenuView::NeededHeight() const
    {
        std::size_t count = 0;
        for (const Controls::ControlPtr& child : _menu->Children)
        {
            if (child->IsVisible())
            {
                count++;
            }
        }
        const std::size_t gaps = count > 0 ? count - 1 : 0;
        return count * 26 + gaps * 6
            + UiLayout::WellTop + UiLayout::WellBottom + 70;
    }

    void PauseMenuView::RefreshVote()
    {
        const bool visible = Network::MapVote::Active()
            && !Network::MapVote::Answered() && !Network::DemoPlayback::IsActive();
        if (_voteYes->IsVisible() == visible && _voteNo->IsVisible() == visible)
        {
            return;
        }
        const bool refocus = !visible && (_voteYes->IsFocused() || _voteNo->IsFocused());
        _voteYes->IsVisible(visible);
        _voteNo->IsVisible(visible);
        if (refocus)
        {
            _resume->Focus();
        }
        FitToHost(Bounds().Height);
    }

    void PauseMenuView::AnswerVote(bool yes)
    {
        Network::MapVote::Cast(yes);
        RefreshVote();
        Resumed(*this);
    }

    void PauseMenuView::FitToHost(double height)
    {
        if (height <= 0)
        {
            return;
        }
        const double scale = std::clamp(height / NeededHeight(), 0.5, 1.0);
        const Media::TransformPtr transform = _scaler->LayoutTransform();
        const auto current = std::dynamic_pointer_cast<Media::ScaleTransform>(transform);
        if (current != nullptr && std::abs(current->ScaleY - scale) < 0.001)
        {
            return;
        }
        _scaler->LayoutTransform(scale >= 1
            ? Media::TransformPtr{}
            : std::make_shared<Media::ScaleTransform>(scale, scale));
    }

    void PauseMenuView::FocusResume()
    {
        const std::shared_ptr<DeckButton> resume = _resume;
        Threading::Dispatcher::UIThread().Post([resume] { resume->Focus(); },
            Threading::DispatcherPriority::Background);
    }

    void PauseMenuView::OnKeyDown(Av::Input::KeyEventArgs& e)
    {
        if (e.Key == Av::Input::Key::Escape)
        {
            Resumed(*this);
            e.Handled = true;
            return;
        }
        UserControl::OnKeyDown(e);
    }

    std::string PauseMenuView::WindowLabel()
    {
        return WindowMode::IsFullscreen() ? "Windowed" : "Fullscreen";
    }

    std::shared_ptr<DeckButton> PauseMenuView::Add(
        const std::shared_ptr<Controls::StackPanel>& menu,
        std::string text, std::function<void()> action, Deck::Face face)
    {
        auto entry = std::make_shared<DeckButton>(std::move(text), face, 1.2, 0.8, 0.5, 4);
        entry->HorizontalAlignment(Layout::HorizontalAlignment::Stretch);
        entry->Click += [action = std::move(action)](DeckButton&) { action(); };
        menu->Children.Add(entry);
        return entry;
    }
}
