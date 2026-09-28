#include "ControllerKeyboard.hpp"

#include "ControllerNav.hpp"
#include "FocusNavigator.hpp"
#include "GuiTheme.hpp"
#include "UiWord.hpp"
#include "../../../NativeRuntime/Avalonia/Threading.hpp"
#include "../../../NativeRuntime/Avalonia/TopLevel.hpp"
#include "../../../NativeRuntime/System/Encoding.hpp"

#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>

namespace MphRead::Mods::Launcher::Gui
{
    namespace Runtime = ::MphRead::NativeRuntime;
    using namespace Runtime::Avalonia;

    namespace
    {
        [[nodiscard]] std::u16string ToManagedUtf16(std::string_view value)
        {
            const std::wstring wide = Runtime::Wtf8ToWide(value);
            std::u16string result;
            result.reserve(wide.size());
            if constexpr (sizeof(wchar_t) == 2)
            {
                for (const wchar_t unit : wide)
                {
                    result.push_back(static_cast<char16_t>(static_cast<std::uint16_t>(unit)));
                }
            }
            else
            {
                for (const wchar_t unit : wide)
                {
                    char32_t scalar = static_cast<char32_t>(unit);
                    if (scalar > 0xFFFFU && scalar <= 0x10FFFFU)
                    {
                        scalar -= 0x10000U;
                        result.push_back(static_cast<char16_t>(0xD800U + (scalar >> 10)));
                        result.push_back(static_cast<char16_t>(0xDC00U + (scalar & 0x3FFU)));
                    }
                    else
                    {
                        result.push_back(static_cast<char16_t>(scalar));
                    }
                }
            }
            return result;
        }

        [[nodiscard]] std::string FromManagedUtf16(std::u16string_view value)
        {
            std::wstring wide;
            wide.reserve(value.size());
            if constexpr (sizeof(wchar_t) == 2)
            {
                for (const char16_t unit : value)
                {
                    wide.push_back(static_cast<wchar_t>(unit));
                }
            }
            else
            {
                for (std::size_t index = 0; index < value.size(); ++index)
                {
                    char32_t unit = static_cast<char32_t>(value[index]);
                    if (unit >= 0xD800U && unit <= 0xDBFFU && index + 1 < value.size())
                    {
                        const char32_t low = static_cast<char32_t>(value[index + 1]);
                        if (low >= 0xDC00U && low <= 0xDFFFU)
                        {
                            unit = 0x10000U + ((unit - 0xD800U) << 10) + (low - 0xDC00U);
                            ++index;
                        }
                    }
                    wide.push_back(static_cast<wchar_t>(unit));
                }
            }
            return Runtime::WideToWtf8(wide);
        }

        [[nodiscard]] std::size_t ManagedStringLength(std::string_view value)
        {
            return ToManagedUtf16(value).size();
        }
    }

    ControllerKeyboard::ControllerKeyboard(Controls::TextBox& target, std::function<void()> closed, bool captureOnly)
        : _target(std::dynamic_pointer_cast<Controls::TextBox>(target.shared_from_this())),
          _closed(std::move(closed)), _text(_target->Text())
    {
        auto panel = std::make_shared<Controls::StackPanel>();
        panel->Spacing(8);
        panel->Margin(Thickness(16));
        _preview = std::make_shared<Controls::TextBlock>();
        _preview->Text(_text);
        _preview->Foreground(GuiTheme::TextBrush);
        _preview->MaxWidth(540);
        _preview->TextWrapping(Media::TextWrapping::Wrap);
        panel->Children.Add(_preview);

        constexpr std::array<std::string_view, 5> rows{
            "1234567890", "qwertyuiop", "asdfghjkl", "zxcvbnm", ".:-_/@[]+"};
        for (const std::string_view rowText : rows)
        {
            auto keys = std::make_shared<Controls::StackPanel>();
            keys->Orientation(Layout::Orientation::Horizontal);
            keys->Spacing(12);
            for (const char character : rowText)
            {
                auto button = std::make_shared<UiWord>(std::string(1, character), 24);
                button->MinWidth(28);
                ControllerNav::Identify(*button, "keyboard.key." + std::to_string(static_cast<unsigned char>(character)),
                    character == '1');
                button->Click += [this, character](UiWord&)
                {
                    char value = character;
                    if (_upper && value >= 'a' && value <= 'z')
                    {
                        value = static_cast<char>(value - 'a' + 'A');
                    }
                    Append(std::string(1, value));
                };
                keys->Children.Add(button);
            }
            panel->Children.Add(keys);
        }

        auto commands = std::make_shared<Controls::StackPanel>();
        commands->Orientation(Layout::Orientation::Horizontal);
        commands->Spacing(16);
        const auto command = [this, commands](const std::string& label, std::function<void()> action)
        {
            auto word = std::make_shared<UiWord>(label, 19);
            word->Click += [action = std::move(action)](UiWord&) { action(); };
            commands->Children.Add(word);
        };
        command("Space", [this] { Append(" "); });
        command("Shift", [this] { _upper = !_upper; });
        command("Delete", [this]
        {
            std::u16string text = ToManagedUtf16(_text);
            if (!text.empty())
            {
                text.pop_back();
                _text = FromManagedUtf16(text);
                _preview->Text(_text);
            }
        });
        command("Done", [this] { Close(true); });
        command("Cancel", [this] { Close(false); });
        panel->Children.Add(commands);

        auto root = std::make_shared<Controls::Border>();
        root->Background(GuiTheme::PanelBrush);
        root->BorderBrush(GuiTheme::AccentBrush);
        root->BorderThickness(Thickness(1));
        root->Child(panel);
        root->SetValue(ControllerNav::NavScopeProperty, std::optional<std::string>("keyboard"));
        root->SetValue(ControllerNav::ModalProperty, true);
        if (captureOnly)
        {
            root->HorizontalAlignment(Layout::HorizontalAlignment::Center);
            root->VerticalAlignment(Layout::VerticalAlignment::Center);
        }
        _navigationRoot = root;

        _popup = std::make_shared<Controls::Popup>();
        _popup->PlacementTarget(_target.get());
        _popup->Placement(Controls::PlacementMode::Center);
        _popup->IsLightDismissEnabled(false);
        _popup->ShouldUseOverlayLayer(true);
        _popup->Child(captureOnly ? Controls::ControlPtr{} : _navigationRoot);
        for (Visual* ancestor : _target->GetVisualAncestors())
        {
            if (auto* parent = dynamic_cast<Controls::Panel*>(ancestor); parent != nullptr)
            {
                _parent = std::dynamic_pointer_cast<Controls::Panel>(parent->shared_from_this());
                break;
            }
        }
        if (_parent != nullptr)
        {
            _parent->Children.Add(_popup);
        }
        _popup->Closed += [this](Controls::Popup&) { Close(false); };
        _detachedToken = _target->DetachedFromVisualTree += [this](Controls::Control& sender) { TargetDetached(sender); };
        if (!captureOnly)
        {
            _popup->Open();
        }
        if (TopLevel* top = TopLevel::GetTopLevel(_target.get()))
        {
            top->UpdateLayout();
        }
        const std::shared_ptr<Controls::Control> navigationRoot = _navigationRoot;
        Threading::Dispatcher::UIThread().Post([navigationRoot]
        {
            FocusNavigator::Ensure(*navigationRoot);
        });
    }

    ControllerKeyboard::~ControllerKeyboard()
    {
        if (!_finished)
        {
            _finished = true;
            Cleanup();
        }
    }

    void ControllerKeyboard::Append(const std::string& value)
    {
        const std::size_t length = ManagedStringLength(_text);
        const std::size_t appendLength = ManagedStringLength(value);
        if (_target->MaxLength() > 0 && length + appendLength > static_cast<std::size_t>(_target->MaxLength()))
        {
            return;
        }
        if (length >= 1024)
        {
            return;
        }
        _text += value;
        _preview->Text(_text);
    }

    void ControllerKeyboard::TargetDetached(Controls::Control& sender)
    {
        if (&sender == _target.get())
        {
            Close(false);
        }
    }

    void ControllerKeyboard::UnsubscribeTarget()
    {
        if (_target != nullptr && _detachedToken != 0)
        {
            _target->DetachedFromVisualTree.Remove(_detachedToken);
            _detachedToken = 0;
        }
    }

    void ControllerKeyboard::Cleanup()
    {
        UnsubscribeTarget();
        if (_popup != nullptr)
        {
            _popup->Close();
            _popup->Child(Controls::ControlPtr{});
        }
        if (_parent != nullptr && _popup != nullptr)
        {
            _parent->Children.Remove(_popup);
        }
    }

    void ControllerKeyboard::Close(bool accept)
    {
        const std::shared_ptr<ControllerKeyboard> keepAlive = weak_from_this().lock();
        (void)keepAlive;
        if (_finished)
        {
            return;
        }
        _finished = true;
        UnsubscribeTarget();
        if (accept)
        {
            _target->SetCurrentValue(Controls::TextBox::TextProperty, _text);
        }
        Cleanup();
        if (TopLevel::GetTopLevel(_target.get()) != nullptr)
        {
            _target->Focus();
        }
        if (_closed)
        {
            _closed();
        }
    }
}
