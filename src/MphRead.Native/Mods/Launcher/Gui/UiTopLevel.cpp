#include "UiTopLevel.hpp"

#include "../../../Mods/DebugLog.hpp"
#include "../../../NativeRuntime/System/Console.hpp"

#include <exception>
#include <string>
#include <utility>

namespace MphRead::Mods::Launcher::Gui
{
    namespace
    {
        bool RenderTimerInstalled = false;
    }

    void UiRenderTimer::Install() noexcept
    {
        RenderTimerInstalled = true;
    }

    void UiRenderTimer::Pump(UiTopLevelImpl& topLevel)
    {
        if (RenderTimerInstalled)
        {
            (void)topLevel.Render();
        }
    }

    std::unique_ptr<UiTopLevelImpl> UiTopLevelImpl::TryCreate()
    {
        try
        {
            return std::make_unique<UiTopLevelImpl>();
        }
        catch (const std::exception& ex)
        {
            ::MphRead::Mods::DebugLog::Exception("ui", ex);
            ::MphRead::NativeRuntime::ConsoleWriteLine(
                std::string("[launcher] the fast surface could not be built; falling back to the toolkit's own: ")
                    + ex.what());
            return nullptr;
        }
        catch (...)
        {
            ::MphRead::Mods::DebugLog::Exception("ui", std::current_exception());
            ::MphRead::NativeRuntime::ConsoleWriteLine(
                "[launcher] the fast surface could not be built; falling back to the toolkit's own: unknown error");
            return nullptr;
        }
    }

    void UiTopLevelImpl::MouseMove(Av::Point point, Av::Input::RawInputModifiers modifiers)
    {
        _root.MouseMove(point, modifiers);
    }

    void UiTopLevelImpl::MouseDown(Av::Point point, Av::Input::MouseButton button,
        Av::Input::RawInputModifiers modifiers)
    {
        _root.MouseDown(point, button, modifiers);
    }

    void UiTopLevelImpl::MouseUp(Av::Point point, Av::Input::MouseButton button,
        Av::Input::RawInputModifiers modifiers)
    {
        _root.MouseUp(point, button, modifiers);
    }

    void UiTopLevelImpl::MouseWheel(Av::Point point, Av::Vector delta, Av::Input::RawInputModifiers modifiers)
    {
        _root.MouseWheel(point, delta, modifiers);
    }

    void UiTopLevelImpl::TouchBegin(Av::Point point, std::int64_t id)
    {
        UiRenderTimer::Pump(*this);
        _root.TouchBegin(point, id);
    }

    void UiTopLevelImpl::TouchUpdate(Av::Point point, std::int64_t id)
    {
        _root.TouchUpdate(point, id);
    }

    void UiTopLevelImpl::TouchEnd(Av::Point point, std::int64_t id)
    {
        _root.TouchEnd(point, id);
    }

    void UiTopLevelImpl::KeyPress(Av::Input::Key key, Av::Input::RawInputModifiers modifiers,
        std::int32_t physicalKey, std::optional<std::string> keySymbol)
    {
        _root.KeyPress(key, modifiers, physicalKey, std::move(keySymbol));
    }

    void UiTopLevelImpl::KeyRelease(Av::Input::Key key, Av::Input::RawInputModifiers modifiers,
        std::int32_t physicalKey, std::optional<std::string> keySymbol)
    {
        _root.KeyRelease(key, modifiers, physicalKey, std::move(keySymbol));
    }

    void UiTopLevelImpl::TextInput(const std::string& text)
    {
        _root.TextInput(text);
    }
}
