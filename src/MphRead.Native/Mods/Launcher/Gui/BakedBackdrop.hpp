#pragma once

#include "UiLayout.hpp"

#include <memory>

namespace MphRead::Mods::Launcher::Gui
{
    // The backdrop's layers rendered once into one bitmap at the device
    // resolution, re-cut only when the size changes, and blitted.
    class BakedBackdrop final : public Av::Controls::Control
    {
    public:
        explicit BakedBackdrop(UiLayout::BackdropWash wash, UiLayout::BackdropPart part = UiLayout::BackdropPart::All);

        void Render(Av::Media::DrawingContext& context) override;
        static void Forget();

    protected:
        Av::Size ArrangeOverride(Av::Size finalSize) override;

    private:
        // Rounded up to a multiple of this, so dragging an edge re-cuts every
        // few points rather than every one.
        static constexpr std::int32_t Grain = 32;
        static constexpr std::size_t Kept = 6;

        [[nodiscard]] static std::shared_ptr<Av::Media::Imaging::Bitmap> Fetch(Av::Size size, UiLayout::BackdropWash wash,
            UiLayout::BackdropPart part);
        [[nodiscard]] static std::int32_t Round(double value);
        [[nodiscard]] static std::shared_ptr<Av::Media::Imaging::RenderTargetBitmap> Bake(std::int32_t width,
            std::int32_t height, UiLayout::BackdropWash wash, UiLayout::BackdropPart part);

        const UiLayout::BackdropWash _wash;
        const UiLayout::BackdropPart _part;
        std::shared_ptr<Av::Media::Imaging::Bitmap> _image;
    };
}
