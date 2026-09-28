#include "UiBench.hpp"

#include "Deck.hpp"
#include "DeckTile.hpp"
#include "GuiLauncher.hpp"
#include "PauseMenuView.hpp"
#include "PlayScreen.hpp"
#include "SettingsView.hpp"
#include "StartScreen.hpp"
#include "UiLayout.hpp"
#include "UiSurface.hpp"
#include "UiTopLevel.hpp"
#include "../../Render/LauncherPhoto.hpp"
#include "../../../Menu.hpp"
#include "../../../Metadata/Rooms.hpp"
#include "../../../NativeRuntime/Avalonia/Scroll.hpp"
#include "../../../NativeRuntime/Avalonia/Threading.hpp"
#include "../../../NativeRuntime/Stb/Image.hpp"
#include "../../../NativeRuntime/System/Console.hpp"
#include "../../../NativeRuntime/System/Globalization.hpp"
#include "../../../NativeRuntime/System/IO.hpp"
#include "../../../NativeRuntime/System/Runtime.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <exception>
#include <iomanip>
#include <locale>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace MphRead::Mods::Launcher::Gui
{
    namespace Av = ::MphRead::NativeRuntime::Avalonia;
    namespace Runtime = ::MphRead::NativeRuntime;

    bool UiBench::FreeFrames = false;
    bool UiBench::Slow = false;
    std::optional<std::string> UiBench::OnlySize{};
    std::optional<std::string> UiBench::OnlyMove{};
    double UiBench::ScaleOverride = 0;
    std::optional<std::string> UiBench::Shot{};
    bool UiBench::AsAndroid = false;

    namespace
    {
        [[nodiscard]] std::size_t FrameBytes(const UiTopLevelImpl& topLevel);
        [[nodiscard]] double MillisecondsSince(std::chrono::steady_clock::time_point started);

        constexpr std::array<std::pair<std::int32_t, std::int32_t>, 5> Sizes{{
            {1280, 720}, {1600, 900}, {1920, 1080}, {2560, 1440}, {3840, 2160}}};

        enum class Move : std::int32_t { Still, Paint, Repaint, Scroll, Pointer, Wheel };

        [[nodiscard]] const char* MoveName(Move move) noexcept
        {
            switch (move)
            {
            case Move::Still: return "Still";
            case Move::Paint: return "Paint";
            case Move::Repaint: return "Repaint";
            case Move::Scroll: return "Scroll";
            case Move::Pointer: return "Pointer";
            case Move::Wheel: return "Wheel";
            }
            return "Still";
        }

        [[nodiscard]] std::string Number(double value, std::int32_t precision, bool trim = false)
        {
            std::ostringstream text;
            text.imbue(std::locale::classic());
            text << std::fixed << std::setprecision(precision) << value;
            std::string result = text.str();
            if (trim)
            {
                while (!result.empty() && result.back() == '0')
                {
                    result.pop_back();
                }
                if (!result.empty() && result.back() == '.')
                {
                    result.pop_back();
                }
            }
            return result;
        }

        [[nodiscard]] std::vector<std::uint8_t> EncodePng(
            const std::uint8_t* pixels, std::int32_t width, std::int32_t height)
        {
            std::vector<std::uint8_t> bytes;
            const auto append = [](void* context, void* data, int size)
            {
                auto& output = *static_cast<std::vector<std::uint8_t>*>(context);
                const auto* begin = static_cast<const std::uint8_t*>(data);
                output.insert(output.end(), begin, begin + size);
            };
            ::stbi_flip_vertically_on_write(0);
            if (::stbi_write_png_to_func(append, &bytes, width, height, 4, pixels, width * 4) == 0)
            {
                throw std::runtime_error("PNG encoding failed.");
            }
            return bytes;
        }

        void SavePixels(const std::string& path, const std::uint8_t* pixels,
            std::int32_t width, std::int32_t height)
        {
            if (pixels == nullptr)
            {
                return;
            }
            Runtime::FileWriteAllBytes(path, EncodePng(pixels, width, height));
        }

        class Rig
        {
        public:
            virtual ~Rig() = default;
            [[nodiscard]] virtual std::int32_t Width() const = 0;
            [[nodiscard]] virtual std::int32_t Height() const = 0;
            // NativeRuntime raises LayoutUpdated on the top level after a
            // completed pass, rather than on each descendant as Avalonia does.
            // Use that pass boundary for the benchmark's layout counter.
            [[nodiscard]] virtual Av::Layout::Layoutable& LayoutMetricsTarget() = 0;
            virtual void Pump() = 0;
            virtual void MouseMove(Av::Point point) = 0;
            virtual void MouseWheel(Av::Point point, Av::Vector delta) = 0;
            [[nodiscard]] virtual bool Take(std::vector<std::uint8_t>& sink,
                double& grab, double& upload) = 0;
            virtual void Save(const std::string& path) = 0;
        };

        class FastRig final : public Rig
        {
        public:
            FastRig(const Av::Controls::ControlPtr& content, std::int32_t width, std::int32_t height)
            : _root(_impl.Root())
            {
                // UiBench measures CPU frame copies by design.
                _impl.GpuRendering(false);
                _impl.SetClientSize(Av::Size{static_cast<double>(width), static_cast<double>(height)});
                _root.Content(content);
                _impl.Prepare();
                _impl.StartRendering();
            }

            [[nodiscard]] std::int32_t Width() const override { return _impl.PixelWidth(); }
            [[nodiscard]] std::int32_t Height() const override { return _impl.PixelHeight(); }
            [[nodiscard]] Av::Layout::Layoutable& LayoutMetricsTarget() override { return _impl.Root(); }

            void Pump() override
            {
                _drawn = _impl.Drawn();
                Av::Threading::Dispatcher::UIThread().RunJobs();
                UiRenderTimer::Pump(_impl);
            }

            void MouseMove(Av::Point point) override
            {
                _impl.MouseMove(point, Av::Input::RawInputModifiers::None);
            }

            void MouseWheel(Av::Point point, Av::Vector delta) override
            {
                _impl.MouseWheel(point, delta, Av::Input::RawInputModifiers::None);
            }

            [[nodiscard]] bool Take(std::vector<std::uint8_t>& sink,
                double& grab, double& upload) override
            {
                grab = 0;
                upload = 0;
                if (_impl.Drawn() == _drawn || _impl.Pixels() == nullptr)
                {
                    return false;
                }
                const std::size_t bytes = std::min(sink.size(), FrameBytes(_impl));
                const auto started = std::chrono::steady_clock::now();
                std::memcpy(sink.data(), _impl.Pixels(), bytes);
                upload = MillisecondsSince(started);
                return true;
            }

            void Save(const std::string& path) override
            {
                SavePixels(path, _impl.Pixels(), _impl.PixelWidth(), _impl.PixelHeight());
            }

        private:
            UiTopLevelImpl _impl;
            Av::EmbeddableControlRoot& _root;
            std::int32_t _drawn = 0;
        };

        // The old Avalonia Window returned a copied WriteableBitmap for every
        // render tick. NativeRuntime has no Window backend or managed collector,
        // so this rig measures the same full-frame copy over the in-tree root;
        // its one deferred frame follows C++ RAII instead of inventing GC data.
        class SlowRig final : public Rig
        {
        public:
            SlowRig(const Av::Controls::ControlPtr& content, std::int32_t width, std::int32_t height)
            : _root(_impl.Root())
            {
                // The slow rig also benchmarks copied software frames, not
                // the interactive Ganesh compositor.
                _impl.GpuRendering(false);
                _impl.SetClientSize(Av::Size{static_cast<double>(width), static_cast<double>(height)});
                _root.Content(content);
                _impl.Prepare();
                _impl.StartRendering();
            }

            [[nodiscard]] std::int32_t Width() const override { return _impl.PixelWidth(); }
            [[nodiscard]] std::int32_t Height() const override { return _impl.PixelHeight(); }
            [[nodiscard]] Av::Layout::Layoutable& LayoutMetricsTarget() override { return _impl.Root(); }

            void Pump() override
            {
                Av::Threading::Dispatcher::UIThread().RunJobs();
                _root.InvalidateRender();
                UiRenderTimer::Pump(_impl);
            }

            void MouseMove(Av::Point point) override
            {
                _impl.MouseMove(point, Av::Input::RawInputModifiers::None);
            }

            void MouseWheel(Av::Point point, Av::Vector delta) override
            {
                _impl.MouseWheel(point, delta, Av::Input::RawInputModifiers::None);
            }

            [[nodiscard]] bool Take(std::vector<std::uint8_t>& sink,
                double& grab, double& upload) override
            {
                if (_impl.Pixels() == nullptr)
                {
                    grab = 0;
                    upload = 0;
                    return false;
                }
                const std::size_t bytes = std::min(sink.size(), FrameBytes(_impl));
                const auto started = std::chrono::steady_clock::now();
                std::vector<std::uint8_t> frame(bytes);
                std::memcpy(frame.data(), _impl.Pixels(), bytes);
                grab = MillisecondsSince(started);
                const auto uploadStarted = std::chrono::steady_clock::now();
                std::memcpy(sink.data(), frame.data(), bytes);
                if (UiBench::FreeFrames)
                {
                    // The C# path disposes the WriteableBitmap before stopping
                    // its upload timer, so include native release here too.
                    std::vector<std::uint8_t>().swap(frame);
                }
                upload = MillisecondsSince(uploadStarted);
                if (!UiBench::FreeFrames)
                {
                    // Retain one frame until the next sample; the previous
                    // one is released after its timing window, like deferred
                    // finalization outside the measured C# copy.
                    _deferredFrame = std::move(frame);
                }
                return true;
            }

            void Save(const std::string& path) override
            {
                SavePixels(path, _impl.Pixels(), _impl.PixelWidth(), _impl.PixelHeight());
            }

        private:
            UiTopLevelImpl _impl;
            Av::EmbeddableControlRoot& _root;
            std::vector<std::uint8_t> _deferredFrame;
        };

        [[nodiscard]] std::size_t FrameBytes(const UiTopLevelImpl& topLevel)
        {
            return static_cast<std::size_t>(topLevel.PixelWidth())
                * static_cast<std::size_t>(topLevel.PixelHeight()) * 4;
        }

        [[nodiscard]] double MillisecondsSince(std::chrono::steady_clock::time_point started)
        {
            return std::chrono::duration<double, std::milli>(
                std::chrono::steady_clock::now() - started).count();
        }

        [[nodiscard]] std::unique_ptr<Rig> Stand(const Av::Controls::ControlPtr& content,
            std::int32_t width, std::int32_t height)
        {
            if (UiBench::Slow)
            {
                return std::make_unique<SlowRig>(content, width, height);
            }
            return std::make_unique<FastRig>(content, width, height);
        }

        [[nodiscard]] Av::Controls::ControlPtr Build(const std::string& screen,
            const std::shared_ptr<::MphRead::MenuSettings>& settings)
        {
            std::vector<std::string> rooms;
            try
            {
                for (const auto& entry : ::MphRead::Metadata::RoomMetadata)
                {
                    const std::shared_ptr<::MphRead::RoomMetadata>& meta = entry.second;
                    if (meta == nullptr)
                    {
                        throw std::runtime_error("Room metadata was null.");
                    }
                    if (meta->Multiplayer)
                    {
                        rooms.push_back(meta->Name);
                    }
                }
            }
            catch (const std::exception&)
            {
                // No game files: the screens still lay out, with empty lists.
            }
            std::sort(rooms.begin(), rooms.end(), Runtime::OrdinalIgnoreCaseLess{});
            if (screen == "start")
            {
                return StartScreen::Create(settings, rooms);
            }
            if (screen == "play")
            {
                return std::make_shared<PlayScreen>(settings, rooms, PlayScreen::Face::Online);
            }
            if (screen == "maps")
            {
                return std::make_shared<PlayScreen>(settings, rooms, PlayScreen::Face::Offline);
            }
            if (screen == "pause")
            {
                return std::make_shared<PauseMenuView>(true);
            }
            auto view = std::make_shared<SettingsView>(settings);
            view->ShowSection("Controls");
            return view;
        }

        [[nodiscard]] Av::Controls::ScrollViewer* Scroller(Av::Controls::Control& view)
        {
            Av::Controls::ScrollViewer* best = nullptr;
            for (Av::Visual* visual : view.GetVisualDescendants())
            {
                auto* scroll = dynamic_cast<Av::Controls::ScrollViewer*>(visual);
                if (scroll != nullptr && scroll->IsVisible()
                    && scroll->Extent().Height > scroll->Viewport().Height + 1
                    && (best == nullptr || scroll->Bounds().Height > best->Bounds().Height))
                {
                    best = scroll;
                }
            }
            return best;
        }

        [[nodiscard]] double Median(std::vector<double> values)
        {
            std::sort(values.begin(), values.end());
            return values[values.size() / 2];
        }

        void Row(Rig& rig, Av::Controls::Control& view, Av::Controls::ScrollViewer* scroller,
            Av::Point centre, std::vector<std::uint8_t>& sink, Move move,
            const std::string& windowLabel, const std::string& surfaceLabel, double factor)
        {
            constexpr std::int32_t Warm = 10;
            constexpr std::int32_t Runs = 61;
            std::vector<double> render(Runs);
            std::vector<double> grabs(Runs);
            std::vector<double> uploads(Runs);
            std::vector<double> inputs(Runs);
            std::int32_t draws = 0;
            std::int32_t layouts = 0;
            double step = 0;
            Av::Layout::Layoutable& layoutTarget = rig.LayoutMetricsTarget();
            const std::size_t layoutToken = layoutTarget.LayoutUpdated += [&layouts](Av::Layout::Layoutable&)
            {
                layouts++;
            };
            for (std::int32_t i = 0; i < Warm + Runs; ++i)
            {
                double input = 0;
                switch (move)
                {
                case Move::Scroll:
                    if (scroller != nullptr)
                    {
                        const double room = std::max(scroller->Extent().Height - scroller->Viewport().Height, 1.0);
                        step = std::fmod(step + 7.0, room);
                        const Av::Vector offset = scroller->Offset();
                        scroller->Offset(Av::Vector{offset.X, step});
                    }
                    break;
                case Move::Paint:
                    view.InvalidateVisual();
                    break;
                case Move::Repaint:
                    if (scroller != nullptr)
                    {
                        const Av::Controls::ControlPtr content = scroller->Content();
                        if (content != nullptr)
                        {
                            for (Av::Visual* child : content->GetVisualDescendants())
                            {
                                if (auto* control = dynamic_cast<Av::Controls::Control*>(child); control != nullptr)
                                {
                                    control->InvalidateVisual();
                                }
                            }
                        }
                    }
                    break;
                case Move::Pointer:
                case Move::Wheel:
                {
                    const auto started = std::chrono::steady_clock::now();
                    if (move == Move::Wheel)
                    {
                        rig.MouseWheel(centre, Av::Vector{0, i % 2 == 0 ? -1.0 : 1.0});
                    }
                    else
                    {
                        rig.MouseMove(Av::Point{centre.X + i % 7, centre.Y});
                    }
                    input = MillisecondsSince(started);
                    break;
                }
                case Move::Still:
                    break;
                }
                const auto started = std::chrono::steady_clock::now();
                rig.Pump();
                const double renderMs = MillisecondsSince(started);
                double grabMs = 0;
                double uploadMs = 0;
                const bool painted = rig.Take(sink, grabMs, uploadMs);
                if (i >= Warm)
                {
                    const std::size_t sample = static_cast<std::size_t>(i - Warm);
                    render[sample] = renderMs;
                    grabs[sample] = grabMs;
                    uploads[sample] = uploadMs;
                    inputs[sample] = input;
                    if (painted)
                    {
                        draws++;
                    }
                }
            }
            const double mRender = Median(std::move(render));
            const double mGrab = Median(std::move(grabs));
            const double mUpload = Median(std::move(uploads));
            const double mInput = Median(std::move(inputs));
            const double total = mRender + mGrab + mUpload + mInput;
            const char* label = move == Move::Still ? "still"
                : move == Move::Paint ? "paint"
                : move == Move::Repaint ? "repaint"
                : move == Move::Pointer ? "pointer"
                : move == Move::Wheel ? "wheel"
                : scroller == nullptr ? "scroll!" : "scroll";

            std::ostringstream line;
            line.imbue(std::locale::classic());
            line << "  " << std::left << std::setw(11) << windowLabel
                << ' ' << std::setw(11) << surfaceLabel << ' '
                << std::setw(5) << (factor < 0 ? std::string() : Number(factor, 3, true))
                << ' ' << std::setw(7) << label << "| "
                << std::right << std::setw(7) << Number(mInput, 2) << ' '
                << std::setw(8) << Number(mRender, 2) << ' '
                << std::setw(8) << Number(mGrab, 2) << ' '
                << std::setw(8) << Number(mUpload, 2) << " | "
                << std::setw(7) << Number(total, 2) << ' '
                << std::setw(7) << Number(1000.0 / std::max(total, .001), 0)
                << " | " << std::setw(6) << "n/a" << ' ' << std::setw(4) << "n/a" << " | "
                << std::setw(3) << draws << '/' << Runs << " drawn "
                << std::setw(4) << layouts << " layouts";
            Runtime::ConsoleWriteLine(line.str());
            layoutTarget.LayoutUpdated.Remove(layoutToken);
            if (DeckTile::ChromeAsks() > 0)
            {
                Runtime::ConsoleWriteLine("            card chrome: "
                    + std::to_string(DeckTile::ChromeBakes()) + " cut, "
                    + std::to_string(DeckTile::ChromeAsks()) + " asked for");
            }
        }

        [[nodiscard]] double Raster(std::int32_t width, std::int32_t height)
        {
            if (UiSurface::NativeRaster())
            {
                return 1;
            }
            const double fits = std::min(1920.0 / std::max(width, 1),
                1080.0 / std::max(height, 1));
            if (fits >= 1)
            {
                return 1;
            }
            return std::max(std::floor(fits * 16) / 16, .25);
        }

        void One(const std::string& screen, const std::shared_ptr<::MphRead::MenuSettings>& settings,
            std::int32_t width, std::int32_t height)
        {
            const double raster = Raster(width, height);
            const std::int32_t surfaceWidth = std::max(
                static_cast<std::int32_t>(std::nearbyint(width * raster)), 1);
            const std::int32_t surfaceHeight = std::max(
                static_cast<std::int32_t>(std::nearbyint(height * raster)), 1);
            double factor = UiLayout::Factor(width, height) * raster;
            if (UiBench::ScaleOverride > 0)
            {
                factor = UiBench::ScaleOverride;
            }
            UiLayout::BakeScale = factor;
            auto host = std::make_shared<Av::Controls::LayoutTransformControl>();
            host->LayoutTransform(std::make_shared<Av::Media::ScaleTransform>(factor, factor));
            host->HorizontalAlignment(Av::Layout::HorizontalAlignment::Stretch);
            host->VerticalAlignment(Av::Layout::VerticalAlignment::Stretch);
            Av::Controls::ControlPtr view = Build(screen, settings);
            view->HorizontalAlignment(Av::Layout::HorizontalAlignment::Stretch);
            view->VerticalAlignment(Av::Layout::VerticalAlignment::Stretch);
            host->Child(view);
            host->LayoutTransform(std::make_shared<Av::Media::ScaleTransform>(factor, factor));
            std::unique_ptr<Rig> rig = Stand(host, surfaceWidth, surfaceHeight);
            for (std::int32_t i = 0; i < 8; ++i)
            {
                rig->Pump();
            }
            view->Focus();
            for (std::int32_t i = 0; i < 4; ++i)
            {
                rig->Pump();
            }
            const Av::Point centre{surfaceWidth / 2.0, surfaceHeight / 2.0};
            rig->MouseMove(centre);
            Av::Controls::ScrollViewer* scroller = Scroller(*view);
            std::vector<std::uint8_t> sink(static_cast<std::size_t>(surfaceWidth)
                * static_cast<std::size_t>(surfaceHeight) * 4);
            bool first = true;
            constexpr std::array<Move, 6> moves{
                Move::Still, Move::Paint, Move::Repaint, Move::Scroll, Move::Pointer, Move::Wheel};
            for (const Move move : moves)
            {
                if (UiBench::OnlyMove.has_value() && !UiBench::OnlyMove->empty()
                    && !Runtime::StringEqualsOrdinalIgnoreCase(*UiBench::OnlyMove, MoveName(move)))
                {
                    continue;
                }
                Row(*rig, *view, scroller, centre, sink, move,
                    first ? std::to_string(width) + "x" + std::to_string(height) : std::string(),
                    first ? std::to_string(surfaceWidth) + "x" + std::to_string(surfaceHeight) : std::string(),
                    first ? factor : -1);
                first = false;
            }
            if (UiBench::Shot.has_value() && !UiBench::Shot->empty())
            {
                view->InvalidateVisual();
                rig->Pump();
                const std::string name = screen + "-" + std::to_string(surfaceWidth) + "x"
                    + std::to_string(surfaceHeight) + "-" + (UiBench::Slow ? "headless" : "fast") + ".png";
                Runtime::DirectoryCreateDirectory(*UiBench::Shot);
                const std::string path = Runtime::PathCombine(*UiBench::Shot, name);
                rig->Save(path);
                Runtime::ConsoleWriteLine("  [uibench] " + path);
            }
            for (std::int32_t i = 0; i < 4; ++i)
            {
                Av::Threading::Dispatcher::UIThread().RunJobs();
            }
        }
    }

    std::int32_t UiBench::Run(const std::optional<std::string>& what)
    {
        if (!GuiLauncher::EnsureSetup())
        {
            Runtime::ConsoleWriteLine("[uibench] no Avalonia backend on this machine");
            return 1;
        }
        Deck::Still(true);
        ::MphRead::Mods::Render::LauncherPhoto::Enabled(!AsAndroid);
        const std::string screen = !what.has_value() || what->empty() ? "settings" : *what;
        Av::Threading::Dispatcher::UIThread().Invoke([&screen] { Measure(screen); });
        Deck::Still(false);
        return 0;
    }

    void UiBench::Measure(const std::string& screen)
    {
        const auto settings = std::make_shared<::MphRead::MenuSettings>();
        const bool head = !OnlySize.has_value() || OnlySize->empty();
        if (head)
        {
            std::string heading = "[uibench] screen=" + screen + "  surface "
                + (Slow ? "native headless frame-copy rig" : "UiTopLevelImpl")
                + (AsAndroid ? "  backdrop as Android (animated layer in the tree)" : "")
                + (DeckTile::CacheChrome ? "" : "  card shadows blurred every frame")
                + (Slow ? (FreeFrames ? "  frames freed by RAII" : "  one frame retained by RAII") : "")
                + "  raster cap " + (UiSurface::NativeRaster() ? "off" : "on")
                + "  " + std::to_string(Runtime::EnvironmentProcessorCount()) + " cores";
            Runtime::ConsoleWriteLine(heading);
            Runtime::ConsoleWriteLine("");
            Runtime::ConsoleWriteLine("  window      surface     x     what   |"
                "   input   render     grab   upload |   total     fps |  gc ms  colls");
            Runtime::ConsoleWriteLine("  " + std::string(104, '-'));
        }
        for (const auto& [width, height] : Sizes)
        {
            if (!head && *OnlySize != std::to_string(width) + "x" + std::to_string(height))
            {
                continue;
            }
            One(screen, settings, width, height);
        }
        if (head)
        {
            Tail();
        }
    }

    void UiBench::Tail()
    {
        Runtime::ConsoleWriteLine("");
        Runtime::ConsoleWriteLine("  medians per redraw, milliseconds.");
        Runtime::ConsoleWriteLine("  input  = delivering one pointer or wheel event");
        Runtime::ConsoleWriteLine("  render = RunJobs() + one render tick: layout and Skia");
        Runtime::ConsoleWriteLine("  grab   = getting at the finished pixels "
            "(a second full-surface allocation and copy on the native copy path)");
        Runtime::ConsoleWriteLine("  upload = reading every byte of them (stands in for glTexSubImage2D)");
        Runtime::ConsoleWriteLine("  gc ms / colls = not applicable; NativeRuntime has no managed collector");
        Runtime::ConsoleWriteLine("");
    }
}
