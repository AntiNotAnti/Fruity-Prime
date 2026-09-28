#include "UiList.hpp"

#include "Rows.hpp"
#include "ServerRow.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace MphRead::Mods::Launcher::Gui
{
    using namespace ::MphRead::NativeRuntime::Avalonia;

    namespace
    {
        // object.Equals for what a row's Choice holds: strings, slots and
        // numbers compared by value, anything else never equal.
        [[nodiscard]] bool AnyEquals(const std::any& a, const std::any& b)
        {
            if (!a.has_value() || !b.has_value() || a.type() != b.type())
            {
                return false;
            }
            if (a.type() == typeid(std::string))
            {
                return std::any_cast<const std::string&>(a) == std::any_cast<const std::string&>(b);
            }
            if (a.type() == typeid(std::uint8_t))
            {
                return std::any_cast<std::uint8_t>(a) == std::any_cast<std::uint8_t>(b);
            }
            if (a.type() == typeid(std::int32_t))
            {
                return std::any_cast<std::int32_t>(a) == std::any_cast<std::int32_t>(b);
            }
            return false;
        }
    }

    // ---------------------------------------------------------- UiListRow

    UiListRow::UiListRow(std::string title, std::string detail)
        : _title(std::move(title)), _detail(std::move(detail))
    {
        Height(30);
        Focusable(true);
        Cursor(std::make_shared<Input::Cursor>(Input::StandardCursorType::Hand));
        // A second click on the same row means "this one, go".
        DoubleTapped += [this](Input::InputElement&, Interactivity::RoutedEventArgs&)
        {
            Clicked(*this);
            Activated(*this);
        };
    }

    void UiListRow::IsSelected(bool value)
    {
        if (_selected == value)
        {
            return;
        }
        _selected = value;
        InvalidateVisual();
    }

    void UiListRow::Detail(std::string value)
    {
        _detail = std::move(value);
        InvalidateVisual();
    }

    void UiListRow::OnPointerEntered(Input::PointerEventArgs& e)
    {
        _hot = true;
        InvalidateVisual();
        Control::OnPointerEntered(e);
    }

    void UiListRow::OnPointerExited(Input::PointerEventArgs& e)
    {
        _hot = false;
        _tap.Cancel();
        InvalidateVisual();
        Control::OnPointerExited(e);
    }

    void UiListRow::OnPointerPressed(Input::PointerPressedEventArgs& e)
    {
        _tap.Press(e, *this);
        Control::OnPointerPressed(e);
    }

    void UiListRow::OnPointerMoved(Input::PointerEventArgs& e)
    {
        _tap.Moved(e, *this);
        Control::OnPointerMoved(e);
    }

    void UiListRow::OnPointerReleased(Input::PointerReleasedEventArgs& e)
    {
        // A release is not a choice on its own: the press has to have landed
        // here and stayed.
        if (_tap.Release(e, *this))
        {
            Focus();
            Clicked(*this);
        }
        Control::OnPointerReleased(e);
    }

    void UiListRow::OnPointerCaptureLost(Input::PointerCaptureLostEventArgs& e)
    {
        _tap.Cancel();
        Control::OnPointerCaptureLost(e);
    }

    void UiListRow::OnKeyDown(Input::KeyEventArgs& e)
    {
        if (e.Key == Input::Key::Enter || e.Key == Input::Key::Space)
        {
            Clicked(*this);
            Activated(*this);
            e.Handled = true;
            return;
        }
        Control::OnKeyDown(e);
    }

    void UiListRow::OnGotFocus(Input::GotFocusEventArgs& e)
    {
        InvalidateVisual();
        Control::OnGotFocus(e);
    }

    void UiListRow::OnLostFocus(Input::FocusChangedEventArgs& e)
    {
        InvalidateVisual();
        Control::OnLostFocus(e);
    }

    void UiListRow::LayOut(bool lit)
    {
        if (_titleText != nullptr && _laidOutAt == Bounds().Width && _laidOutLit == lit && _laidOutDetail == _detail)
        {
            return;
        }
        _laidOutAt = Bounds().Width;
        _laidOutLit = lit;
        _laidOutDetail = _detail;
        double right = Bounds().Width;
        if (!_detail.empty())
        {
            _detailText = std::make_shared<Media::FormattedText>(_detail, Media::InvariantCulture,
                Media::FlowDirection::LeftToRight, GuiTheme::Face(false), 12, GuiTheme::TextDimBrush);
            _detailText->MaxTextWidth(std::max(40.0, Bounds().Width * 0.45));
            _detailText->MaxTextHeight(20);
            _detailText->Trimming(Media::TextTrimming::CharacterEllipsis);
            right = Bounds().Width - _detailText->Width() - 14;
        }
        else
        {
            _detailText = nullptr;
        }
        _titleText = std::make_shared<Media::FormattedText>(_title, Media::InvariantCulture,
            Media::FlowDirection::LeftToRight, GuiTheme::Face(true), 14,
            std::make_shared<Media::SolidColorBrush>(lit ? GuiTheme::Accent : GuiTheme::Text));
        _titleText->MaxTextWidth(std::max(40.0, right - 14));
        _titleText->MaxTextHeight(22);
        _titleText->Trimming(Media::TextTrimming::CharacterEllipsis);
    }

    void UiListRow::Render(Media::DrawingContext& context)
    {
        const Rect full(0, 0, Bounds().Width, Bounds().Height);
        context.FillRectangle(Media::Brushes::Transparent(), full);
        const bool lit = _hot || IsFocused() || _selected;
        if (lit)
        {
            // A caret rather than a fill: the list sits on a photograph.
            context.FillRectangle(GuiTheme::AccentBrush, Rect(0, 6, 3, Bounds().Height - 12));
        }
        LayOut(lit);
        if (_detailText != nullptr)
        {
            context.DrawText(*_detailText, Point{Bounds().Width - _detailText->Width() - 4,
                (Bounds().Height - _detailText->Height()) / 2});
        }
        context.DrawText(*_titleText, Point{14, (Bounds().Height - _titleText->Height()) / 2});
    }

    // ------------------------------------------------------------- UiList

    UiList::UiList()
        : _rows(std::make_shared<Controls::StackPanel>()), _header(std::make_shared<Controls::Panel>()),
          _scroll(std::make_shared<Controls::ScrollViewer>())
    {
        // Room either side for the ring a selected row wears.
        _rows->Spacing(1);
        _rows->Margin(Thickness(3, 0, 3, 0));
        _scroll->Content(_rows);
        _scroll->HorizontalScrollBarVisibility(Controls::ScrollBarVisibility::Disabled);
        _scroll->VerticalScrollBarVisibility(Controls::ScrollBarVisibility::Auto);
        // scrollbar-width: thin; scrollbar-color: var(--edge) transparent --
        // the two Styles on ScrollBar and Thumb.
        _scroll->ScrollBarWidth = 6.0;
        _scroll->ScrollBarFixedWidth = true;
        _scroll->ScrollBarBackground = Media::Brushes::Transparent();
        _scroll->ThumbBackground = GuiTheme::EdgeBrush;
        _scroll->ThumbCornerRadius = CornerRadius(3);
        auto dock = std::make_shared<Controls::DockPanel>();
        dock->LastChildFill(true);
        Controls::DockPanel::SetDock(*_header, Controls::Dock::Top);
        dock->Children.Add(_header);
        dock->Children.Add(_scroll);
        Child(dock);
    }

    void UiList::SetHeader(const Controls::ControlPtr& header)
    {
        _header->Children.Clear();
        if (header != nullptr)
        {
            _header->Children.Add(header);
        }
    }

    void UiList::Clear()
    {
        _rows->Children.Clear();
        _focusable.clear();
        _selected = nullptr;
    }

    void UiList::Add(const Controls::ControlPtr& row, std::function<void(const Controls::ControlPtr&)> activate)
    {
        _rows->Children.Add(row);
        if (!row->Focusable())
        {
            return;
        }
        _focusable.push_back(row);
        if (_selected == nullptr && AutoSelectFirst)
        {
            // The first row to arrive is the selection, with its side effect.
            _selected = row;
            MarkSelection();
            if (activate)
            {
                activate(row);
            }
            SelectionChanged(*this, row);
        }
        std::weak_ptr<Controls::Control> weak = row;
        row->GotFocus += [this, weak](Input::InputElement&, Input::GotFocusEventArgs&)
        {
            if (const auto held = weak.lock())
            {
                Select(held);
            }
        };
        if (auto* line = dynamic_cast<UiListRow*>(row.get()))
        {
            line->Clicked += [this, weak, activate](UiListRow&)
            {
                if (const auto held = weak.lock())
                {
                    Fire(held, activate, false);
                }
            };
            line->Activated += [this, weak, activate](UiListRow&)
            {
                if (const auto held = weak.lock())
                {
                    Fire(held, activate, true);
                }
            };
        }
        else if (auto* server = dynamic_cast<ServerRow*>(row.get()))
        {
            server->Clicked += [this, weak, activate](ServerRow&)
            {
                if (const auto held = weak.lock())
                {
                    Fire(held, activate, false);
                }
            };
            server->Activated += [this, weak, activate](ServerRow&)
            {
                if (const auto held = weak.lock())
                {
                    Fire(held, activate, true);
                }
            };
        }
    }

    void UiList::AddNote(const std::string& text, std::optional<Media::Color> colour)
    {
        _rows->Children.Add(std::make_shared<Note>(text, colour));
    }

    void UiList::Fire(const Controls::ControlPtr& row, const std::function<void(const Controls::ControlPtr&)>& activate,
        bool activated)
    {
        Select(row);
        if (activate)
        {
            activate(row);
        }
        if (activated)
        {
            Activated(*this, row);
        }
    }

    void UiList::Select(const Controls::ControlPtr& row)
    {
        if (_selected == row)
        {
            return;
        }
        _selected = row;
        MarkSelection();
        SelectionChanged(*this, row);
    }

    void UiList::MarkSelection()
    {
        for (const Controls::ControlPtr& row : _focusable)
        {
            const bool on = row == _selected;
            if (auto* line = dynamic_cast<UiListRow*>(row.get()))
            {
                line->IsSelected(on);
            }
            else if (auto* server = dynamic_cast<ServerRow*>(row.get()))
            {
                server->IsSelected(on);
            }
        }
    }

    Av::Size UiList::MeasureOverride(Av::Size availableSize)
    {
        if (SpacingEms > 0)
        {
            // Not rounded, and no layout rounding on the column: a fraction
            // of a point per row compounds down a list.
            _rows->UseLayoutRounding(false);
            const double gap = Deck::GetEm(*this) * SpacingEms;
            if (std::abs(gap - _rows->Spacing()) > 0.01)
            {
                _rows->Spacing(gap);
            }
        }
        return Decorator::MeasureOverride(availableSize);
    }

    bool UiList::HandleKey(Input::Key key)
    {
        if (_focusable.empty())
        {
            return false;
        }
        const int step = key == Input::Key::Down ? 1 : key == Input::Key::Up ? -1 : 0;
        if (step == 0)
        {
            return false;
        }
        const auto found = std::find(_focusable.begin(), _focusable.end(), _selected);
        const int at = _selected == nullptr || found == _focusable.end() ? -1
                                                                         : static_cast<int>(found - _focusable.begin());
        const int count = static_cast<int>(_focusable.size());
        const int next = at < 0 ? (step > 0 ? 0 : count - 1) : std::clamp(at + step, 0, count - 1);
        Focus(_focusable[static_cast<std::size_t>(next)]);
        return true;
    }

    void UiList::Focus(const Controls::ControlPtr& row)
    {
        Select(row);
        row->Focus();
        row->BringIntoView();
    }

    void UiList::FocusFirst()
    {
        if (_focusable.empty())
        {
            return;
        }
        const Controls::ControlPtr row
            = _selected != nullptr && std::find(_focusable.begin(), _focusable.end(), _selected) != _focusable.end()
            ? _selected
            : _focusable[0];
        Threading::Dispatcher::UIThread().Post([this, row] { Focus(row); }, Threading::DispatcherPriority::Background);
    }

    void UiList::SelectTag(const std::any& choice)
    {
        if (!choice.has_value())
        {
            return;
        }
        for (const Controls::ControlPtr& row : _focusable)
        {
            if (auto* line = dynamic_cast<UiListRow*>(row.get()); line != nullptr && AnyEquals(line->Choice, choice))
            {
                Select(row);
                return;
            }
        }
    }
}
