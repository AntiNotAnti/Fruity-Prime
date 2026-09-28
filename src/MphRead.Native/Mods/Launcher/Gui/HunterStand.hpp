#pragma once

#include "../../../Formats/Enums.hpp"
#include "../../../NativeRuntime/Avalonia/Avalonia.hpp"
#include "../../../NativeRuntime/System/Stopwatch.hpp"

#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace MphRead::Mods::Launcher::Gui
{
    namespace Av = ::MphRead::NativeRuntime::Avalonia;

    // The launcher's turntable: a real model supplied by the engine when one
    // is available, a cached shot on a windowless head, or the boxes below.
    class HunterStand final : public Av::Controls::Control
    {
    public:
        HunterStand();

        // The seven, in the order the pickers step through them.
        static std::array<std::string, 7> Names;
        [[nodiscard]] static Av::Media::Color TintOf(const std::string& name);

        [[nodiscard]] std::int32_t Suit() const noexcept { return _suit; }
        void Suit(std::int32_t value);
        [[nodiscard]] const std::string& Name2() const noexcept { return _who; }
        void Name2(std::string value);

        void Render(Av::Media::DrawingContext& context) override;

    protected:
        void OnAttachedToVisualTree() override;
        void OnDetachedFromVisualTree() override;
        void OnPointerPressed(Av::Input::PointerPressedEventArgs& e) override;
        void OnPointerMoved(Av::Input::PointerEventArgs& e) override;
        void OnPointerReleased(Av::Input::PointerReleasedEventArgs& e) override;
        void OnPointerCaptureLost(Av::Input::PointerCaptureLostEventArgs& e) override;

    private:
        struct Box final
        {
            float X;
            float Y;
            float Z;
            float W;
            float H;
            float D;
            Av::Media::Color Colour;
        };

        struct Hunter final
        {
            Av::Media::Color Tint;
            std::vector<Box> Boxes;
        };

        [[nodiscard]] static const std::unordered_map<std::string, Hunter>& HunterData();
        [[nodiscard]] static Av::Media::Color Rgb(std::uint32_t hex) noexcept;
        [[nodiscard]] static const std::array<std::array<std::int32_t, 4>, 6>& Faces() noexcept;
        [[nodiscard]] static const std::array<std::array<double, 3>, 6>& Normals() noexcept;
        [[nodiscard]] static const std::array<double, 3>& Light() noexcept;

        [[nodiscard]] MphRead::Hunter Asked() const noexcept;
        [[nodiscard]] bool EnginePainting() const noexcept;
        [[nodiscard]] Av::Media::Color SuitTint(Av::Media::Color baseTint) const;
        [[nodiscard]] static Av::Media::Color Wear(Av::Media::Color box,
            Av::Media::Color baseTint, Av::Media::Color suit) noexcept;
        [[nodiscard]] static Av::Media::Color Shade(Av::Media::Color color, double factor) noexcept;
        void AskForShot();
        static std::int32_t Grain(std::int32_t pixels) noexcept;
        void Take(std::vector<std::uint8_t> pixels, std::int32_t width, std::int32_t height,
            const std::string& key);
        void Beat();
        void PublishInFrame();
#if defined(MPHREAD_SHELL)
        [[nodiscard]] bool Publish();
#endif

        std::int32_t _suit = 0;
        std::string _who = "Samus";
        std::int32_t _swapping = 0;
        double _spin = 0;
        bool _dragging = false;
        double _dragX = 0;
        double _dragBase = 0;
        ::MphRead::NativeRuntime::Stopwatch _clock =
            ::MphRead::NativeRuntime::Stopwatch::StartNew();
        ::MphRead::NativeRuntime::TimeSpan _lastFrame{};
        Av::Threading::DispatcherTimer _turn;
        Av::Media::Imaging::BitmapPtr _shot;
        std::string _shotIs;
        std::string _shotAsked;
    };
}
