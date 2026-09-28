#pragma once

#include "../../../NativeRuntime/Avalonia/Avalonia.hpp"
#include "../../../Formats/Enums.hpp"
#include "../../../Formats/Formats.hpp"
#include "../../Network/NetMaster.hpp"
#include "../Portable/LaunchPlan.hpp"
#include "Tap.hpp"

#include <memory>
#include <cstdint>
#include <optional>
#include <stop_token>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace MphRead::Mods::Launcher::Gui
{
    namespace Av = ::MphRead::NativeRuntime::Avalonia;

    class ChoiceRow;
    class FieldRow;
    class Note;
    class ProgressRow;
    class UiList;
    class UiListRow;
    class UiMark;
    class PickRow;
    class HostPicker;

    // Create a lobby on a hosted server or on this machine, then join it in
    // the one game window.
    class CreateServerScreen final : public Av::Controls::UserControl
    {
    public:
        CreateServerScreen(const std::vector<std::string>& rooms,
            std::optional<std::string> firstMap = std::nullopt);

        Av::Event<CreateServerScreen&> Closed;
        Av::Event<CreateServerScreen&, ::MphRead::Mods::Launcher::LaunchPlan> Launched;

        [[nodiscard]] static bool CanRunHere();
        void ShowDedicated();

    protected:
        void OnKeyDown(Av::Input::KeyEventArgs& e) override;

    private:
        [[nodiscard]] std::shared_ptr<CreateServerScreen> Self();
        [[nodiscard]] bool Dedicated() const;
        void Leave();
        void Refresh();
        void Say(std::string text, Av::Media::Color colour);
        [[nodiscard]] std::string Describe() const;
        void AskDirectories();
        void Arrived(Network::HostCandidate candidate);
        void OpenHosts();
        void OpenMaps();
        void OpenPage(const Av::Controls::ControlPtr& page);
        void ClosePage();
        void Fetch();
        void Fail(std::optional<std::string> why);
        void Busy(bool busy, std::string label);
        void Go();
        void StartHere(std::string name, std::string player, MphRead::Hunter hunter,
            std::vector<std::pair<std::string, MphRead::GameMode>> maps);
        void StartOnServer(std::string name, std::string player, MphRead::Hunter hunter,
            MphRead::GameMode mode, std::vector<std::pair<std::string, MphRead::GameMode>> maps);
        void JoinedHere(std::string name, std::string player, MphRead::Hunter hunter,
            std::vector<std::pair<std::string, MphRead::GameMode>> maps,
            std::int32_t port, bool joined);
        void JoinedHosted(std::string name, std::string player, MphRead::Hunter hunter,
            MphRead::GameMode mode, Network::HostedGame game, bool joined);
        void Finish(::MphRead::Mods::Launcher::LaunchPlan plan);

        std::vector<std::string> _rooms;
        std::vector<std::string> _rotation;
        std::shared_ptr<Av::Controls::Panel> _root;
        std::shared_ptr<Av::Controls::StackPanel> _form;
        std::shared_ptr<Note> _note;
        std::shared_ptr<ProgressRow> _progress;
        std::shared_ptr<FieldRow> _name;
        std::shared_ptr<ChoiceRow> _mode;
        std::shared_ptr<ChoiceRow> _hunter;
        std::shared_ptr<ChoiceRow> _kind;
        std::shared_ptr<PickRow> _host;
        std::shared_ptr<PickRow> _maps;
        std::shared_ptr<UiMark> _back;
        std::shared_ptr<UiMark> _go;
        std::shared_ptr<UiMark> _fetch;
        Av::Controls::ControlPtr _page;
        std::vector<Network::HostCandidate> _candidates;
        std::optional<Network::HostCandidate> _chosen;
        std::shared_ptr<HostPicker> _picker;
        std::shared_ptr<std::stop_source> _work;
        bool _finished = false;
        bool _busy = false;
        bool _asking = true;
        bool _directoriesStarted = false;
    };

    // A row that opens a page for a larger-than-cyclic choice.
    class PickRow final : public Av::Controls::Control
    {
    public:
        explicit PickRow(std::string label);
        Av::Event<PickRow&> Clicked;
        void Set(std::string value);
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
        [[nodiscard]] std::shared_ptr<PickRow> Self();

        std::string _label;
        std::string _value;
        bool _hot = false;
        Tap _tap;
    };

    // The fleet of machines that can be asked to run a lobby.
    class HostPicker final : public Av::Controls::UserControl
    {
    public:
        HostPicker();
        Av::Event<HostPicker&, Network::HostCandidate> Done;
        Av::Event<HostPicker&> Cancelled;
        Av::Event<HostPicker&> RefreshRequested;
        void Show(const std::vector<Network::HostCandidate>& candidates, bool asking);

    protected:
        void OnKeyDown(Av::Input::KeyEventArgs& e) override;

    private:
        [[nodiscard]] std::shared_ptr<HostPicker> Self();
        std::shared_ptr<UiList> _list;
        std::shared_ptr<Note> _note;
        bool _focused = false;
    };

    // An ordered map rotation, with the one-map variant used by captures.
    class MapRotationPicker final : public Av::Controls::UserControl
    {
    public:
        MapRotationPicker(const std::vector<std::string>& rooms,
            const std::vector<std::string>& picked, bool single = false);
        Av::Event<MapRotationPicker&, std::vector<std::string>> Done;
        Av::Event<MapRotationPicker&> Cancelled;

    protected:
        void OnKeyDown(Av::Input::KeyEventArgs& e) override;

    private:
        [[nodiscard]] std::shared_ptr<MapRotationPicker> Self();
        void Fill();
        void Toggle(const std::string& room);
        void Mark();
        void Commit();

        std::shared_ptr<UiList> _list;
        std::shared_ptr<Note> _note;
        std::vector<std::string> _picked;
        std::vector<std::string> _rooms;
        std::unordered_map<std::string, std::shared_ptr<UiListRow>> _byRoom;
        bool _single = false;
    };
}
