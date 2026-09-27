#pragma once

#include "GamepadState.hpp"
#include "PadAction.hpp"

#include <optional>
#include <string>
#include <utility>

namespace MphRead::Mods::Input
{
    enum class UiAction : std::int32_t;

    class InputPrompt final
    {
    public:
        InputPrompt() = default;
        InputPrompt(GamepadButtons button, std::string label,
            GamepadButtons modifier = GamepadButtons::None)
            : _button(button), _label(std::move(label)), _modifier(modifier)
        {
        }

        [[nodiscard]] static InputPrompt For(UiAction action);
        [[nodiscard]] static InputPrompt For(PadAction action);
        [[nodiscard]] GamepadButtons Button() const noexcept { return _button; }
        [[nodiscard]] const std::optional<std::string>& Label() const noexcept { return _label; }
        [[nodiscard]] GamepadButtons Modifier() const noexcept { return _modifier; }
        [[nodiscard]] std::string Glyph() const;
        [[nodiscard]] std::string ToString() const;

        friend bool operator==(const InputPrompt&, const InputPrompt&) = default;

    private:
        GamepadButtons _button = GamepadButtons::None;
        std::optional<std::string> _label{};
        GamepadButtons _modifier = GamepadButtons::None;
    };
}
