#pragma once

#include "Deck.hpp"

#include <vector>

namespace MphRead::Mods::Launcher::Gui
{
    // Moving backdrop for heads where the OpenGL launcher photograph is not
    // drawn below the UI. The effect is composed from GPU-backed vector
    // gradients; it never creates a CPU pixel buffer.
    class MovingBackdrop final : public Av::Controls::Control
    {
    public:
        MovingBackdrop();
        ~MovingBackdrop() override;

        [[nodiscard]] static bool Suspended() noexcept { return _suspended; }
        static void Suspended(bool value);

        void Render(Av::Media::DrawingContext& context) override;

    protected:
        void OnAttachedToVisualTree() override;
        void OnDetachedFromVisualTree() override;

    private:
        static constexpr double FrameSeconds = 0.033;
        static constexpr double PhaseStep = 0.011;

        static void Arbitrate();
        void Tick();

        Av::Threading::DispatcherTimer _timer;
        double _phase = 0;

        static inline std::vector<MovingBackdrop*> _live{};
        static inline bool _suspended = false;
    };
}
