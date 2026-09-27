#include "UiTabs.hpp"

#include "../../../NativeRuntime/System/Managed.hpp"

#include <algorithm>
#include <cmath>

namespace MphRead::Mods::Launcher::Gui
{
    using namespace ::MphRead::NativeRuntime::Avalonia;

    UiTabs::UiTabs(const std::vector<std::string>& names, std::int32_t index)
    {
        // Faces, not words with dots between them.
        Orientation(Layout::Orientation::Horizontal);
        VerticalAlignment(Layout::VerticalAlignment::Center);
        HorizontalAlignment(Layout::HorizontalAlignment::Center);
        _index = names.empty() ? 0 : std::clamp(index, 0, static_cast<std::int32_t>(names.size()) - 1);
        for (std::size_t i = 0; i < names.size(); i++)
        {
            // The reference's .tab: font-size 1.05em, padding .38em .8em, the
            // default six-point edge.
            auto tab = std::make_shared<DeckButton>(names[i], Deck::Face::Slate(), 1.05, 0.8, 0.38, 6);
            const auto target = static_cast<std::int32_t>(i);
            tab->Click += [this, target](DeckButton&) { Index(target); };
            _tabs.push_back(tab);
            Children.Add(tab);
        }
        Mark();
    }

    void UiTabs::Index(std::int32_t value)
    {
        const std::int32_t clamped = _tabs.empty() ? 0 : std::clamp(value, 0, static_cast<std::int32_t>(_tabs.size()) - 1);
        if (clamped == _index)
        {
            return;
        }
        _index = clamped;
        Mark();
        Changed(*this);
    }

    bool UiTabs::HandleKey(Input::Key key)
    {
        if (key == Input::Key::Left)
        {
            Step(-1);
            return true;
        }
        if (key == Input::Key::Right)
        {
            Step(1);
            return true;
        }
        return false;
    }

    void UiTabs::FocusSelected()
    {
        if (_index >= 0 && _index < static_cast<std::int32_t>(_tabs.size()))
        {
            _tabs[static_cast<std::size_t>(_index)]->Focus();
        }
    }

    Size UiTabs::MeasureOverride(Size availableSize)
    {
        const double gap = ::MphRead::NativeRuntime::RoundToEven(Deck::GetEm(*this) * 0.4);
        if (std::abs(gap - Spacing()) > 0.01)
        {
            Spacing(gap);
        }
        return StackPanel::MeasureOverride(availableSize);
    }

    void UiTabs::Step(std::int32_t direction)
    {
        if (_tabs.empty())
        {
            return;
        }
        const auto count = static_cast<std::int32_t>(_tabs.size());
        _index = (_index + direction + count) % count;
        Mark();
        Changed(*this);
    }

    void UiTabs::Mark()
    {
        for (std::size_t i = 0; i < _tabs.size(); i++)
        {
            const bool up = static_cast<std::int32_t>(i) == _index;
            _tabs[i]->Wear(up ? Deck::Face::Rust() : Deck::Face::Slate(), up);
        }
    }
}
