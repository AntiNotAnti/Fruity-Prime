#pragma once

#include "Deck.hpp"
#include "../../Render/NoiseField.hpp"

#include <memory>
#include <vector>

namespace MphRead::Mods::Launcher::Gui
{
    // The reference's moving layer, drawn by the toolkit where nothing under
    // the screens draws it: a small noise field magnified with hard cells
    // and overlaid on the photograph.
    class MovingBackdrop final : public Av::Controls::Control
    {
    public:
        MovingBackdrop();
        ~MovingBackdrop() override;

        // Stop every layer, for a head that has put the screens away.
        [[nodiscard]] static bool Suspended() noexcept { return _suspended; }
        static void Suspended(bool value);

        void Render(Av::Media::DrawingContext& context) override;

    protected:
        void OnAttachedToVisualTree() override;
        void OnDetachedFromVisualTree() override;
        Av::Size ArrangeOverride(Av::Size finalSize) override;

    private:
        static constexpr double Strength = 0.62;
        static constexpr std::uint8_t Alpha = static_cast<std::uint8_t>(Strength * 255);

        static void Arbitrate();
        void Tick() { Step(Bounds().Width, Bounds().Height); }
        void Step(double w, double h);
        [[nodiscard]] std::shared_ptr<Av::Media::Imaging::Bitmap> Cut();

        ::MphRead::Mods::Render::NoiseField _field;
        Av::Threading::DispatcherTimer _timer;
        std::shared_ptr<Av::Media::Imaging::Bitmap> _bitmap;
        std::vector<std::uint8_t> _rgba;

        static inline std::vector<MovingBackdrop*> _live{};
        static inline bool _suspended = false;
    };
}
