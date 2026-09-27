#include "MovingBackdrop.hpp"

#include "../../DebugLog.hpp"

#include <algorithm>
#include <exception>

namespace MphRead::Mods::Launcher::Gui
{
    using namespace ::MphRead::NativeRuntime::Avalonia;

    MovingBackdrop::MovingBackdrop()
        : _timer(Threading::TimeSpan(::MphRead::Mods::Render::NoiseField::Gap / 1000.0), Threading::DispatcherPriority::Render,
              [this] { Tick(); })
    {
        // It is the ground, not a control.
        IsHitTestVisible(false);
    }

    MovingBackdrop::~MovingBackdrop()
    {
        std::erase(_live, this);
    }

    void MovingBackdrop::Suspended(bool value)
    {
        if (_suspended == value)
        {
            return;
        }
        _suspended = value;
        Arbitrate();
    }

    void MovingBackdrop::OnAttachedToVisualTree()
    {
        Control::OnAttachedToVisualTree();
        _live.push_back(this);
        // One frame straight away, so the layer is there for the first
        // picture -- and so a capture, which never starts the timer, has one.
        Threading::Dispatcher::UIThread().Post([this] { Tick(); }, Threading::DispatcherPriority::Loaded);
        Arbitrate();
    }

    void MovingBackdrop::OnDetachedFromVisualTree()
    {
        // A screen that is not on the glass does not get a frame of CPU.
        std::erase(_live, this);
        _timer.Stop();
        Arbitrate();
        Control::OnDetachedFromVisualTree();
    }

    void MovingBackdrop::Arbitrate()
    {
        for (std::size_t i = 0; i < _live.size(); i++)
        {
            MovingBackdrop* layer = _live[i];
            if (i == _live.size() - 1 && !Deck::Still() && !_suspended)
            {
                layer->_timer.Start();
            }
            else
            {
                layer->_timer.Stop();
            }
        }
    }

    Av::Size MovingBackdrop::ArrangeOverride(Av::Size finalSize)
    {
        Step(finalSize.Width, finalSize.Height);
        return Control::ArrangeOverride(finalSize);
    }

    void MovingBackdrop::Step(double w, double h)
    {
        if (w <= 0 || h <= 0 || !_field.Step(w, h, Deck::Still()))
        {
            return;
        }
        const std::shared_ptr<Media::Imaging::Bitmap> cut = Cut();
        if (cut == nullptr)
        {
            return;
        }
        _bitmap = cut;
        InvalidateVisual();
    }

    void MovingBackdrop::Render(Media::DrawingContext& context)
    {
        const std::shared_ptr<Media::Imaging::Bitmap> bitmap = _bitmap;
        const double w = Bounds().Width;
        const double h = Bounds().Height;
        if (bitmap == nullptr || w <= 0 || h <= 0)
        {
            return;
        }
        // Magnified with hard cells, and overlaid.
        Media::RenderOptions options;
        options.BitmapInterpolationMode = Media::BitmapInterpolationMode::None;
        options.BitmapBlendingMode = Media::BitmapBlendingMode::Overlay;
        auto state = context.PushRenderOptions(options);
        context.DrawImage(*bitmap, Rect(0, 0, w, h));
    }

    std::shared_ptr<Media::Imaging::Bitmap> MovingBackdrop::Cut()
    {
        const std::int32_t w = _field.Width();
        const std::int32_t h = _field.Height();
        if (w <= 0 || h <= 0)
        {
            return nullptr;
        }
        try
        {
            const std::vector<std::uint8_t>& source = _field.Pixels();
            const std::int32_t stride = w * 4;
            const std::size_t size = static_cast<std::size_t>(stride) * static_cast<std::size_t>(h);
            if (_rgba.size() != size)
            {
                _rgba.assign(size, 0);
            }
            for (std::size_t i = 0, p = 0; i < _rgba.size(); i += 4, p += 3)
            {
                _rgba[i + 0] = static_cast<std::uint8_t>(source[p + 0] * Alpha / 255);
                _rgba[i + 1] = static_cast<std::uint8_t>(source[p + 1] * Alpha / 255);
                _rgba[i + 2] = static_cast<std::uint8_t>(source[p + 2] * Alpha / 255);
                _rgba[i + 3] = Alpha;
            }
            return Media::Imaging::Bitmap::FromPremultipliedRgba(_rgba.data(), PixelSize{w, h}, stride);
        }
        catch (const std::exception& ex)
        {
            DebugLog::Line("ui", std::string("the moving backdrop could not be drawn: ") + ex.what());
            _timer.Stop();
            return nullptr;
        }
    }
}
