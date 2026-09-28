#pragma once

#include "DeckButton.hpp"

#include <memory>
#include <string>
#include <vector>

namespace MphRead::Mods::Launcher::Gui
{
    // The strip that says which of a screen's few faces is up.
    class UiTabs final : public Av::Controls::StackPanel
    {
    public:
        explicit UiTabs(const std::vector<std::string>& names, std::int32_t index = 0);

        // Raised after Index has already moved.
        Av::Event<UiTabs&> Changed;

        [[nodiscard]] std::int32_t Index() const noexcept { return _index; }
        void Index(std::int32_t value);

        // The left and right keys, wherever they were pressed on the screen.
        bool HandleKey(Av::Input::Key key);
        // Put the keyboard on the name that is up.
        void FocusSelected();

    protected:
        Av::Size MeasureOverride(Av::Size availableSize) override;

    private:
        void Step(std::int32_t direction);
        void Mark();

        std::vector<std::shared_ptr<DeckButton>> _tabs;
        std::int32_t _index = 0;
    };
}
