#include "BakedBackdrop.hpp"

#include "../../DebugLog.hpp"

#include <algorithm>
#include <cmath>
#include <exception>
#include <tuple>
#include <vector>

namespace MphRead::Mods::Launcher::Gui
{
    using namespace ::MphRead::NativeRuntime::Avalonia;

    namespace
    {
        using Cut = std::tuple<std::int32_t, std::int32_t, UiLayout::BackdropWash, UiLayout::BackdropPart>;

        [[nodiscard]] std::vector<std::pair<Cut, std::shared_ptr<Media::Imaging::RenderTargetBitmap>>>& Cache()
        {
            static std::vector<std::pair<Cut, std::shared_ptr<Media::Imaging::RenderTargetBitmap>>> cache;
            return cache;
        }
    }

    BakedBackdrop::BakedBackdrop(UiLayout::BackdropWash wash, UiLayout::BackdropPart part)
        : _wash(wash), _part(part)
    {
        // It is the ground, not a control.
        IsHitTestVisible(false);
    }

    Av::Size BakedBackdrop::ArrangeOverride(Av::Size finalSize)
    {
        _image = Fetch(finalSize, _wash, _part);
        return Control::ArrangeOverride(finalSize);
    }

    void BakedBackdrop::Render(Media::DrawingContext& context)
    {
        const std::shared_ptr<Media::Imaging::Bitmap> image = _image;
        if (image == nullptr || Bounds().Width <= 0 || Bounds().Height <= 0)
        {
            return;
        }
        context.DrawImage(*image, Rect(0, 0, Bounds().Width, Bounds().Height));
    }

    std::shared_ptr<Media::Imaging::Bitmap> BakedBackdrop::Fetch(Av::Size size, UiLayout::BackdropWash wash,
        UiLayout::BackdropPart part)
    {
        const double scale = UiLayout::BakeScale;
        const std::int32_t width = Round(size.Width * scale);
        const std::int32_t height = Round(size.Height * scale);
        if (width <= 0 || height <= 0)
        {
            return nullptr;
        }
        const Cut cut{width, height, wash, part};
        auto& cache = Cache();
        for (std::size_t i = 0; i < cache.size(); i++)
        {
            if (cache[i].first == cut)
            {
                // Most recently wanted goes last.
                auto hit = cache[i];
                cache.erase(cache.begin() + static_cast<std::ptrdiff_t>(i));
                cache.push_back(hit);
                return hit.second;
            }
        }
        std::shared_ptr<Media::Imaging::RenderTargetBitmap> baked = Bake(width, height, wash, part);
        if (baked == nullptr)
        {
            return nullptr;
        }
        while (cache.size() >= Kept)
        {
            cache.erase(cache.begin());
        }
        cache.emplace_back(cut, baked);
        return baked;
    }

    std::int32_t BakedBackdrop::Round(double value)
    {
        const auto pixels = static_cast<std::int32_t>(std::ceil(value));
        if (pixels <= 0)
        {
            return 0;
        }
        return std::min((pixels + Grain - 1) / Grain * Grain, 8192);
    }

    std::shared_ptr<Media::Imaging::RenderTargetBitmap> BakedBackdrop::Bake(std::int32_t width, std::int32_t height,
        UiLayout::BackdropWash wash, UiLayout::BackdropPart part)
    {
        try
        {
            const std::shared_ptr<Controls::Panel> layers = UiLayout::BackdropLayers(wash, part);
            // Measured and arranged by hand: it is in no visual tree.
            layers->Width(width);
            layers->Height(height);
            layers->Measure(Av::Size{static_cast<double>(width), static_cast<double>(height)});
            layers->Arrange(Rect(0, 0, width, height));
            auto target = std::make_shared<Media::Imaging::RenderTargetBitmap>(PixelSize{width, height}, Vector{96, 96});
            target->Render(*layers);
            return target;
        }
        catch (const std::exception& ex)
        {
            DebugLog::Line("ui", "backdrop bake failed at " + std::to_string(width) + "x" + std::to_string(height) + ": "
                + ex.what());
            return nullptr;
        }
    }

    void BakedBackdrop::Forget()
    {
        Cache().clear();
    }
}
