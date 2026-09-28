#pragma once

#include "Deck.hpp"
#include "Tap.hpp"

#include <any>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace MphRead::Mods::Launcher::Gui
{
    // One line of a list: what it is on the left, what is worth knowing about
    // it on the right.
    class UiListRow final : public Av::Controls::Control
    {
    public:
        explicit UiListRow(std::string title, std::string detail = "");

        Av::Event<UiListRow&> Clicked;
        // Chosen rather than merely pointed at: Enter, Space, or a second
        // click on the row that is already selected.
        Av::Event<UiListRow&> Activated;

        // What choosing this line means, for the screen holding the list.
        std::any Choice{};

        [[nodiscard]] bool IsSelected() const noexcept { return _selected; }
        void IsSelected(bool value);

        [[nodiscard]] const std::string& Detail() const noexcept { return _detail; }
        void Detail(std::string value);

        void Render(Av::Media::DrawingContext& context) override;

    protected:
        void OnPointerEntered(Av::Input::PointerEventArgs& e) override;
        void OnPointerExited(Av::Input::PointerEventArgs& e) override;
        void OnPointerPressed(Av::Input::PointerPressedEventArgs& e) override;
        void OnPointerMoved(Av::Input::PointerEventArgs& e) override;
        void OnPointerReleased(Av::Input::PointerReleasedEventArgs& e) override;
        void OnPointerCaptureLost(Av::Input::PointerCaptureLostEventArgs& e) override;
        void OnKeyDown(Av::Input::KeyEventArgs& e) override;
        void OnGotFocus(Av::Input::GotFocusEventArgs& e) override;
        void OnLostFocus(Av::Input::FocusChangedEventArgs& e) override;

    private:
        void LayOut(bool lit);

        const std::string _title;
        std::string _detail;
        bool _hot = false;
        bool _selected = false;
        Tap _tap;
        std::shared_ptr<Av::Media::FormattedText> _titleText;
        std::shared_ptr<Av::Media::FormattedText> _detailText;
        double _laidOutAt = -1;
        bool _laidOutLit = false;
        std::string _laidOutDetail;
    };

    // The one list in the launcher: the scrolling and the up and down keys
    // that every list shares.
    class UiList final : public Av::Controls::Decorator
    {
    public:
        UiList();

        // The row the keyboard is on, or the last one pressed.
        [[nodiscard]] const Av::Controls::ControlPtr& Selected() const noexcept { return _selected; }

        // The gap between rows, in frame ems, or zero for the flat one point.
        double SpacingEms = 0;
        // Whether the first row to arrive becomes the selection.
        bool AutoSelectFirst = true;

        // Raised when a row is pressed or Enter is taken on it.
        Av::Event<UiList&, const Av::Controls::ControlPtr&> Activated;
        // Raised when the keyboard merely lands on a different row.
        Av::Event<UiList&, const Av::Controls::ControlPtr&> SelectionChanged;

        // Column headings, outside the scroller so they stay put.
        void SetHeader(const Av::Controls::ControlPtr& header);
        void Clear();
        void Add(const Av::Controls::ControlPtr& row, std::function<void(const Av::Controls::ControlPtr&)> activate = nullptr);
        // A line that is not a choice: an empty list saying so.
        void AddNote(const std::string& text, std::optional<Av::Media::Color> colour = std::nullopt);
        bool HandleKey(Av::Input::Key key);
        // Put the keyboard on a row, and scroll it into view.
        void Focus(const Av::Controls::ControlPtr& row);
        // The row the list opens on, once the tree exists to focus in.
        void FocusFirst();
        // Choose by what a row stands for, rather than by which control it is.
        void SelectTag(const std::any& choice);

    protected:
        Av::Size MeasureOverride(Av::Size availableSize) override;

    private:
        void Fire(const Av::Controls::ControlPtr& row, const std::function<void(const Av::Controls::ControlPtr&)>& activate,
            bool activated);
        void Select(const Av::Controls::ControlPtr& row);
        void MarkSelection();

        std::shared_ptr<Av::Controls::StackPanel> _rows;
        std::shared_ptr<Av::Controls::Panel> _header;
        std::shared_ptr<Av::Controls::ScrollViewer> _scroll;
        std::vector<Av::Controls::ControlPtr> _focusable;
        Av::Controls::ControlPtr _selected;
    };
}
