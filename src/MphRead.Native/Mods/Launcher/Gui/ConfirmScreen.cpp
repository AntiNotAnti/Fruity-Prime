#include "ConfirmScreen.hpp"

#include "ControllerNav.hpp"
#include "GuiTheme.hpp"
#include "UiLayout.hpp"
#include "UiMark.hpp"

#include <utility>

namespace MphRead::Mods::Launcher::Gui
{
    using namespace ::MphRead::NativeRuntime::Avalonia;

    ConfirmScreen::ConfirmScreen(std::string question, std::string yes, std::string no, bool overGame)
        : _no(std::make_shared<UiMark>(UiMark::Shape::Cancel, no))
    {
        SetValue(ControllerNav::NavScopeProperty, std::optional<std::string>("confirmation"));
        SetValue(ControllerNav::ModalProperty, true);
        Background(Media::Brushes::Transparent());
        Focusable(true);

        auto prompt = std::make_shared<Controls::TextBlock>();
        prompt->Text(std::move(question));
        prompt->FontFamily(GuiTheme::Display());
        prompt->FontSize(26);
        prompt->Foreground(GuiTheme::TextBrush);
        prompt->TextWrapping(Media::TextWrapping::Wrap);
        prompt->TextAlignment(Media::TextAlignment::Center);
        prompt->HorizontalAlignment(Layout::HorizontalAlignment::Center);

        ControllerNav::Identify(*_no, "confirmation.cancel", true);
        _no->Click += [this](UiMark&) { Answered(*this, false); };
        auto ok = std::make_shared<UiMark>(UiMark::Shape::Accept, yes);
        ok->Click += [this](UiMark&) { Answered(*this, true); };

        Content(UiLayout::Page(overGame, UiLayout::WellShort, "", {}, prompt, _no, ok, true));
        AttachedToVisualTree += [noMark = _no](Controls::Control&)
        {
            Threading::Dispatcher::UIThread().Post([noMark] { noMark->Focus(); },
                Threading::DispatcherPriority::Background);
        };
    }

    void ConfirmScreen::OnKeyDown(Av::Input::KeyEventArgs& e)
    {
        if (e.Key == Av::Input::Key::Escape)
        {
            Answered(*this, false);
            e.Handled = true;
            return;
        }
        UserControl::OnKeyDown(e);
    }
}
