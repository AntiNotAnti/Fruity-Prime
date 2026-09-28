#include "DeckTile.hpp"

#include "DeckButton.hpp"
#include "DeckText.hpp"
#include "MapShot.hpp"
#include "UiLayout.hpp"
#include "../../../NativeRuntime/System/Exceptions.hpp"
#include "../../../NativeRuntime/System/Globalization.hpp"
#include "../../../NativeRuntime/System/HashCode.hpp"
#include "../../../NativeRuntime/System/Managed.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>
#include <optional>
#include <tuple>
#include <utility>
#include <vector>

namespace MphRead::Mods::Launcher::Gui
{
    using namespace ::MphRead::NativeRuntime::Avalonia;
    using ::MphRead::NativeRuntime::ConvertToInt32Net9;
    using ::MphRead::NativeRuntime::MathClamp;
    using ::MphRead::NativeRuntime::MathMax;
    using ::MphRead::NativeRuntime::MathMin;
    using ::MphRead::NativeRuntime::RoundToEven;
    using ::MphRead::NativeRuntime::StringGetHashCode;
    using ::MphRead::NativeRuntime::UncheckedMultiply;

    namespace
    {
        using Cut = std::tuple<std::int32_t, std::int32_t, std::int32_t, std::uint32_t, bool>;

        struct TileRenderKey final
        {
            std::string RoomKey{};
            std::string Code{};
            std::string Blurb{};
            std::string Verb{};
            std::string ChosenVerb{};
            const Media::Imaging::Bitmap* Ground = nullptr;
            std::int32_t PixelWidth = 0;
            std::int32_t PixelHeight = 0;
            std::int32_t Tally = -1;
            double Width = 0;
            double Height = 0;
            double Em = 0;
            double BakeScale = 1;
            double ScaleX = 1;
            double ScaleY = 1;
            double Pop = 1;
            double Tilt = 0;
            double TiltX = 0;
            Media::BitmapInterpolationMode Interpolation = Media::BitmapInterpolationMode::Unspecified;
            Media::EdgeMode Edge = Media::EdgeMode::Unspecified;
            Media::TextRenderingMode Text = Media::TextRenderingMode::Unspecified;
            Media::BitmapBlendingMode Blending = Media::BitmapBlendingMode::Unspecified;
            std::optional<bool> RequiresFullOpacityHandling{};
            bool Chosen = false;
            bool Leader = false;
            bool Hovered = false;
            bool Focused = false;
            bool KeyboardDriving = false;
            bool CacheChrome = true;

            bool operator==(const TileRenderKey&) const = default;
        };

        struct TileRenderCacheEntry final
        {
            TileRenderKey Key{};
            std::shared_ptr<Media::Imaging::RenderTargetBitmap> Image{};
            std::size_t Bytes = 0;
        };

        constexpr std::size_t TileRenderCacheBudget = 16 * 1024 * 1024;

        [[nodiscard]] std::vector<TileRenderCacheEntry>& TileRenderCache()
        {
            static std::vector<TileRenderCacheEntry> cache;
            return cache;
        }

        std::size_t& TileRenderCacheBytes()
        {
            static std::size_t bytes = 0;
            return bytes;
        }

        [[nodiscard]] std::shared_ptr<Media::Imaging::RenderTargetBitmap> FindTileRender(
            const TileRenderKey& key)
        {
            auto& cache = TileRenderCache();
            for (std::size_t i = 0; i < cache.size(); i++)
            {
                if (cache[i].Key == key)
                {
                    std::shared_ptr<Media::Imaging::RenderTargetBitmap> image = cache[i].Image;
                    std::rotate(cache.begin() + static_cast<std::ptrdiff_t>(i),
                        cache.begin() + static_cast<std::ptrdiff_t>(i + 1), cache.end());
                    return image;
                }
            }
            return nullptr;
        }

        void StoreTileRender(TileRenderKey key,
            const std::shared_ptr<Media::Imaging::RenderTargetBitmap>& image, std::size_t bytes)
        {
            if (image == nullptr || bytes == 0 || bytes > TileRenderCacheBudget)
            {
                return;
            }
            const std::size_t keyBytes = key.RoomKey.size() + key.Code.size() + key.Blurb.size()
                + key.Verb.size() + key.ChosenVerb.size();
            if (keyBytes > TileRenderCacheBudget - bytes)
            {
                return;
            }
            bytes += keyBytes;
            auto& cache = TileRenderCache();
            std::size_t& cachedBytes = TileRenderCacheBytes();
            while (!cache.empty() && bytes > TileRenderCacheBudget - cachedBytes)
            {
                cachedBytes -= cache.front().Bytes;
                cache.erase(cache.begin());
            }
            cache.push_back(TileRenderCacheEntry{std::move(key), image, bytes});
            cachedBytes += bytes;
        }

        // Two sizes' worth of the four states a card can be in.
        [[nodiscard]] std::vector<std::pair<Cut, std::shared_ptr<Media::Imaging::RenderTargetBitmap>>>& ChromeCache()
        {
            static std::vector<std::pair<Cut, std::shared_ptr<Media::Imaging::RenderTargetBitmap>>> cache;
            return cache;
        }

        [[nodiscard]] Media::IBrushPtr Solid(Media::Color colour)
        {
            return std::make_shared<Media::SolidColorBrush>(colour);
        }
    }

    DeckTile::DeckTile(std::string roomKey, std::string code)
        : RoomKey(std::move(roomKey)), Code(std::move(code))
    {
        static const bool registered = []
        {
            AffectsRender<DeckTile>(Deck::EmProperty);
            return true;
        }();
        (void)registered;
        Focusable(true);
        Cursor(std::make_shared<Input::Cursor>(Input::StandardCursorType::Hand));
    }

    void DeckTile::Chosen(bool value)
    {
        if (_chosen == value)
        {
            return;
        }
        _chosen = value;
        InvalidateVisual();
    }

    Av::Size DeckTile::MeasureOverride(Av::Size availableSize)
    {
        // The grid gives the width; the ratio gives the height.
        const double side = std::isinf(availableSize.Width) ? Em() * 10 : availableSize.Width;
        return {side, RoundToEven(side / MathMax(0.1, Ratio))};
    }

    void DeckTile::Hover(bool over, Point at)
    {
        _over = over;
        _popTarget = over || (IsFocused() && Deck::KeyboardDriving()) ? 1.03 : 1;
        if (over && Bounds().Width > 0 && Bounds().Height > 0)
        {
            const double halfW = Bounds().Width / 2;
            const double halfH = Bounds().Height / 2;
            _tiltTarget = MathClamp((at.X - halfW) / halfW, -1.0, 1.0) * MaxTilt;
            _tiltXTarget = -MathClamp((at.Y - halfH) / halfH, -1.0, 1.0) * MaxTilt;
        }
        else
        {
            _tiltTarget = 0;
            _tiltXTarget = 0;
        }
        InvalidateVisual();
    }

    void DeckTile::OnKeyDown(Input::KeyEventArgs& e)
    {
        if (e.Key == Input::Key::Enter || e.Key == Input::Key::Space)
        {
            e.Handled = true;
            Click(*this);
            return;
        }
        Control::OnKeyDown(e);
    }

    void DeckTile::OnGotFocus(Input::GotFocusEventArgs& e)
    {
        _popTarget = 1.03;
        InvalidateVisual();
        Control::OnGotFocus(e);
    }

    void DeckTile::OnLostFocus(Input::FocusChangedEventArgs& e)
    {
        if (!_over)
        {
            _popTarget = 1;
        }
        InvalidateVisual();
        Control::OnLostFocus(e);
    }

    bool DeckTile::Settle()
    {
        const ::MphRead::NativeRuntime::TimeSpan now = _clock.Elapsed();
        const double dt = MathMin(0.05, (now - _last).TotalSeconds());
        _last = now;
        if (Deck::Still())
        {
            _pop = _popTarget;
            _popVelocity = 0;
            _tilt = _tiltTarget;
            _tiltX = _tiltXTarget;
            return false;
        }
        if (dt <= 0)
        {
            return false;
        }
        const double accel = (_popTarget - _pop) * Stiffness - _popVelocity * Damping;
        _popVelocity += accel * dt;
        _pop += _popVelocity * dt;
        const double ease = MathMin(1.0, dt * 14);
        _tilt += (_tiltTarget - _tilt) * ease;
        _tiltX += (_tiltXTarget - _tiltX) * ease;
        const bool moving = std::abs(_popTarget - _pop) > 0.0005 || std::abs(_popVelocity) > 0.0005
            || std::abs(_tiltTarget - _tilt) > 0.02 || std::abs(_tiltXTarget - _tiltX) > 0.02;
        if (!moving)
        {
            _pop = _popTarget;
            _popVelocity = 0;
            _tilt = _tiltTarget;
            _tiltX = _tiltXTarget;
        }
        return moving;
    }

    void DeckTile::Ask()
    {
        if (_framePending || Deck::Still())
        {
            return;
        }
        _framePending = true;
        const std::shared_ptr<DeckTile> self = std::static_pointer_cast<DeckTile>(shared_from_this());
        Deck::NextFrame(*this, [self]
        {
            self->_framePending = false;
            self->InvalidateVisual();
        });
    }

    void DeckTile::Render(Media::DrawingContext& context)
    {
        const bool moving = Settle();
        const double w = Bounds().Width;
        const double h = Bounds().Height;
        if (w <= 0 || h <= 0)
        {
            return;
        }
        const double em = Em();
        const double radius = em * 0.55;
        const bool hot = _over || (IsFocused() && Deck::KeyboardDriving());
        const Matrix& parentTransform = context.CurrentTransform();
        const bool axisAligned = parentTransform.M12 == 0 && parentTransform.M21 == 0;
        const double scaleX = std::abs(parentTransform.M11);
        const double scaleY = std::abs(parentTransform.M22);
        const bool canCache = !moving && CacheChrome && axisAligned
            && std::isfinite(scaleX) && std::isfinite(scaleY) && scaleX > 0 && scaleY > 0;

        const auto drawContent = [this, w, h, em, radius, hot](Media::DrawingContext& target)
        {
            // A rotation about the card's middle stands in for the reference's
            // two-axis 3D lean.
            auto transform = target.PushTransform(Matrix::CreateTranslation(-w / 2, -h / 2)
                * Matrix::CreateScale(_pop, _pop)
                * Matrix::CreateRotation((_tilt + _tiltX) * std::numbers::pi / 180 * 0.12)
                * Matrix::CreateTranslation(w / 2, h / 2));
            const RoundedRect face(Rect(0, 0, w, h), radius);
            const Media::Color ring = _chosen || Leader ? GuiTheme::Accent
                : hot ? Media::Color::FromRgb(0x4a, 0x6f, 0x8c) : GuiTheme::Edge;
            const bool raised = hot || _chosen;
            const std::shared_ptr<Media::Imaging::Bitmap> chrome = Chrome(w, h, radius, ring, raised);
            if (chrome != nullptr)
            {
                target.DrawImage(*chrome, Rect(-Bleed, -Bleed, w + Bleed * 2, h + Bleed * 2));
            }
            else
            {
                target.DrawRectangle(Solid(GuiTheme::PanelDeep), nullptr, face, Shadows(ring, raised));
            }
            {
                auto clip = target.PushClip(face);
                if (_ground != nullptr)
                {
                    target.DrawImage(*_ground, Rect(0, 0, w, h));
                }
                Info(target, w, h, em);
                Badge(target, w, em);
            }
        };

        if (canCache)
        {
            const Media::RenderOptions& options = context.CurrentRenderOptions();
            const double pixelsWide = (w + Bleed * 2) * scaleX;
            const double pixelsHigh = (h + Bleed * 2) * scaleY;
            if (std::isfinite(pixelsWide) && std::isfinite(pixelsHigh)
                && pixelsWide >= 1 && pixelsHigh >= 1
                && pixelsWide <= 4096 && pixelsHigh <= 4096)
            {
                const std::int32_t pixelWidth = static_cast<std::int32_t>(std::ceil(pixelsWide));
                const std::int32_t pixelHeight = static_cast<std::int32_t>(std::ceil(pixelsHigh));
                const std::size_t pixelCount = static_cast<std::size_t>(pixelWidth)
                    * static_cast<std::size_t>(pixelHeight);
                if (pixelCount <= TileRenderCacheBudget / 4)
                {
                    TileRenderKey key;
                    key.RoomKey = RoomKey;
                    key.Code = Code;
                    key.Blurb = Blurb;
                    key.Verb = Verb;
                    key.ChosenVerb = ChosenVerb;
                    key.Ground = _ground.get();
                    key.PixelWidth = pixelWidth;
                    key.PixelHeight = pixelHeight;
                    key.Tally = Tally;
                    key.Width = w;
                    key.Height = h;
                    key.Em = em;
                    key.BakeScale = UiLayout::BakeScale;
                    key.ScaleX = scaleX;
                    key.ScaleY = scaleY;
                    key.Pop = _pop;
                    key.Tilt = _tilt;
                    key.TiltX = _tiltX;
                    key.Interpolation = options.BitmapInterpolationMode;
                    key.Edge = options.EdgeMode;
                    key.Text = options.TextRenderingMode;
                    key.Blending = options.BitmapBlendingMode;
                    key.RequiresFullOpacityHandling = options.RequiresFullOpacityHandling;
                    key.Chosen = _chosen;
                    key.Leader = Leader;
                    key.Hovered = _over;
                    key.Focused = IsFocused();
                    key.KeyboardDriving = Deck::KeyboardDriving();
                    key.CacheChrome = CacheChrome;

                    if (const std::shared_ptr<Media::Imaging::RenderTargetBitmap> cached = FindTileRender(key))
                    {
                        context.DrawImage(*cached, Rect(-Bleed, -Bleed, w + Bleed * 2, h + Bleed * 2));
                        return;
                    }

                    std::shared_ptr<Media::Imaging::RenderTargetBitmap> image;
                    try
                    {
                        image = std::make_shared<Media::Imaging::RenderTargetBitmap>(
                            PixelSize{pixelWidth, pixelHeight}, Vector{96, 96});
                        auto into = image->CreateDrawingContext();
                        auto optionsState = into->PushRenderOptions(options);
                        auto scale = into->PushTransform(Matrix::CreateScale(scaleX, scaleY));
                        auto translate = into->PushTransform(Matrix::CreateTranslation(Bleed, Bleed));
                        drawContent(*into);
                    }
                    catch (...)
                    {
                        image.reset();
                    }
                    if (image != nullptr)
                    {
                        try
                        {
                            StoreTileRender(std::move(key), image, pixelCount * 4);
                        }
                        catch (...)
                        {
                            // Keep the local image for this draw if the bounded
                            // cache could not grow; the next draw can retry.
                        }
                        context.DrawImage(*image, Rect(-Bleed, -Bleed, w + Bleed * 2, h + Bleed * 2));
                        return;
                    }
                }
            }
        }

        drawContent(context);
        if (moving)
        {
            Ask();
        }
    }

    Av::Size DeckTile::ArrangeOverride(Av::Size finalSize)
    {
        // Cut in the arrange pass and never in Render.
        Bake(finalSize.Width, finalSize.Height);
        PrimeChrome(finalSize.Width, finalSize.Height, Em() * 0.55);
        return Control::ArrangeOverride(finalSize);
    }

    Media::BoxShadows DeckTile::Shadows(Media::Color ring, bool raised)
    {
        return Media::BoxShadows(Deck::Shadow(0, 0, 0, 2, ring),
            {Deck::Shadow(0, raised ? 7 : 5, 0, 0, Deck::Fade(0, 0.45)),
                Deck::Shadow(0, raised ? 16 : 10, raised ? 26 : 18, 0, Deck::Fade(0, 0.5))});
    }

    void DeckTile::PrimeChrome(double w, double h, double radius)
    {
        // The reachable combinations, and only those.
        (void)Chrome(w, h, radius, GuiTheme::Edge, false);
        (void)Chrome(w, h, radius, GuiTheme::Accent, false);
        (void)Chrome(w, h, radius, GuiTheme::Accent, true);
        (void)Chrome(w, h, radius, Media::Color::FromRgb(0x4a, 0x6f, 0x8c), true);
    }

    std::shared_ptr<Media::Imaging::Bitmap> DeckTile::Chrome(double w, double h, double radius, Media::Color ring,
        bool raised)
    {
        if (!CacheChrome)
        {
            return nullptr;
        }
        const std::int32_t width = UncheckedMultiply(ConvertToInt32Net9(std::ceil(w / 8)), 8);
        const std::int32_t height = UncheckedMultiply(ConvertToInt32Net9(std::ceil(h / 8)), 8);
        if (width <= 0 || height <= 0)
        {
            return nullptr;
        }
        const Cut key{width, height, ConvertToInt32Net9(RoundToEven(radius)), ring.ToUInt32(), raised};
        _chromeAsks++;
        auto& cache = ChromeCache();
        for (std::size_t i = 0; i < cache.size(); i++)
        {
            if (cache[i].first == key)
            {
                auto hit = cache[i];
                cache.erase(cache.begin() + static_cast<std::ptrdiff_t>(i));
                cache.push_back(hit);
                return hit.second;
            }
        }
        try
        {
            _chromeBakes++;
            // At the resolution it will be drawn at, so the two-point ring is
            // not a smear.
            const double scale = UiLayout::BakeScale <= 0 ? 1 : UiLayout::BakeScale;
            const double boxWidth = width + Bleed * 2;
            const double boxHeight = height + Bleed * 2;
            auto cut = std::make_shared<Media::Imaging::RenderTargetBitmap>(
                PixelSize{MathMax(ConvertToInt32Net9(std::ceil(boxWidth * scale)), 1),
                    MathMax(ConvertToInt32Net9(std::ceil(boxHeight * scale)), 1)},
                Vector{96, 96});
            {
                auto into = cut->CreateDrawingContext();
                auto scaled = into->PushTransform(Matrix::CreateScale(scale, scale));
                into->DrawRectangle(Solid(GuiTheme::PanelDeep), nullptr,
                    RoundedRect(Rect(Bleed, Bleed, width, height), radius), Shadows(ring, raised));
            }
            while (cache.size() >= ChromeKept)
            {
                cache.erase(cache.begin());
            }
            cache.emplace_back(key, cut);
            return cut;
        }
        catch (...)
        {
            return nullptr;
        }
    }

    std::shared_ptr<Media::Imaging::Bitmap> DeckTile::Bake(double w, double h)
    {
        const std::int32_t width = UncheckedMultiply(ConvertToInt32Net9(std::ceil(w / 8)), 8);
        const std::int32_t height = UncheckedMultiply(ConvertToInt32Net9(std::ceil(h / 8)), 8);
        if (width <= 0 || height <= 0)
        {
            return nullptr;
        }
        if (_ground != nullptr && _groundWidth == width && _groundHeight == height)
        {
            return _ground;
        }
        _ground = nullptr;
        try
        {
            auto cut = std::make_shared<Media::Imaging::RenderTargetBitmap>(PixelSize{width, height}, Vector{96, 96});
            {
                auto into = cut->CreateDrawingContext();
                const std::shared_ptr<Media::Imaging::Bitmap> shot = MapShot::For(RoomKey);
                if (shot != nullptr)
                {
                    // object-fit: cover.
                    const double sw = shot->PixelSize().Width;
                    const double sh = shot->PixelSize().Height;
                    if (sw > 0 && sh > 0)
                    {
                        const double scale = MathMax(width / sw, height / sh);
                        const double dw = sw * scale;
                        const double dh = sh * scale;
                        into->DrawImage(*shot, Rect((width - dw) / 2, (height - dh) / 2, dw, dh));
                    }
                }
                Drift(*into, width, height);
                Scrim(*into, width, height);
            }
            _ground = cut;
            _groundWidth = width;
            _groundHeight = height;
            return _ground;
        }
        catch (...)
        {
            _ground = nullptr;
            return nullptr;
        }
    }

    void DeckTile::OnDetachedFromVisualTree()
    {
        _ground = nullptr;
        _groundWidth = 0;
        _groundHeight = 0;
        Control::OnDetachedFromVisualTree();
    }

    void DeckTile::Badge(Media::DrawingContext& context, double w, double em) const
    {
        if (Tally < 0)
        {
            return;
        }
        const double size = GuiTheme::PixelSize(em);
        const auto count = DeckText::Run(std::to_string(Tally), Deck::Label(), size,
            Solid(Tally > 0 ? Media::Colors::White : GuiTheme::TextDim));
        const double side = MathMax(em * 1.7, count->Width() + em * 0.7);
        const double high = em * 1.7;
        const Rect box(RoundToEven(w - em * 0.4 - side), RoundToEven(em * 0.4), RoundToEven(side), RoundToEven(high));
        context.DrawRectangle(Solid(Tally > 0 ? Deck::Face::Brass().Fill : Deck::Fade(0x0a0c10, 0.85)),
            std::make_shared<Media::Pen>(Solid(Tally > 0 ? Media::Color::FromRgb(0xc9, 0xa2, 0x27) : GuiTheme::Edge), 1),
            RoundedRect(box, RoundToEven(em * 0.35)));
        context.DrawText(*count, Point{RoundToEven(box.X + (box.Width - count->Width()) / 2),
            RoundToEven(box.Y + (box.Height - count->Height()) / 2)});
    }

    void DeckTile::Drift(Media::DrawingContext& context, double w, double h) const
    {
        // The phase the reference would be at, from the room key.
        const std::int32_t hash = StringGetHashCode(RoomKey);
        if (hash == std::numeric_limits<std::int32_t>::min())
        {
            throw ::System::OverflowException();
        }
        const std::uint32_t magnitude = static_cast<std::uint32_t>(hash < 0 ? -hash : hash);
        const double phase = static_cast<double>(magnitude % 1000U) / 1000.0;
        const double dx = (phase - 0.5) * 0.08 * w;
        const double dy = (phase - 0.5) * 0.06 * h;
        auto warm = std::make_shared<Media::RadialGradientBrush>();
        warm->Center = RelativePoint(0.30, 0.40, RelativeUnit::Relative);
        warm->GradientOrigin = RelativePoint(0.30, 0.40, RelativeUnit::Relative);
        warm->RadiusX = RelativeScalar(0.45, RelativeUnit::Relative);
        warm->RadiusY = RelativeScalar(0.55, RelativeUnit::Relative);
        warm->GradientStops = {Media::GradientStop(Media::Color::FromArgb(56, 0xff, 0xb3, 0x47), 0),
            Media::GradientStop(Media::Color::FromArgb(0, 0xff, 0xb3, 0x47), 1)};
        auto cool = std::make_shared<Media::RadialGradientBrush>();
        cool->Center = RelativePoint(0.72, 0.65, RelativeUnit::Relative);
        cool->GradientOrigin = RelativePoint(0.72, 0.65, RelativeUnit::Relative);
        cool->RadiusX = RelativeScalar(0.50, RelativeUnit::Relative);
        cool->RadiusY = RelativeScalar(0.60, RelativeUnit::Relative);
        cool->GradientStops = {Media::GradientStop(Media::Color::FromArgb(80, 0x2b, 0x4e, 0x6b), 0),
            Media::GradientStop(Media::Color::FromArgb(0, 0x2b, 0x4e, 0x6b), 1)};
        const Rect box(dx - w * 0.25, dy - h * 0.25, w * 1.5, h * 1.5);
        context.FillRectangle(warm, box);
        context.FillRectangle(cool, box);
    }

    void DeckTile::Scrim(Media::DrawingContext& context, double w, double h)
    {
        auto brush = std::make_shared<Media::LinearGradientBrush>();
        brush->StartPoint = RelativePoint(0.5, 0, RelativeUnit::Relative);
        brush->EndPoint = RelativePoint(0.5, 1, RelativeUnit::Relative);
        brush->GradientStops = {Media::GradientStop(Media::Color::FromArgb(77, 10, 12, 16), 0),
            Media::GradientStop(Media::Color::FromArgb(51, 10, 12, 16), 0.40),
            Media::GradientStop(Media::Color::FromArgb(199, 10, 12, 16), 0.72),
            Media::GradientStop(Media::Color::FromArgb(245, 10, 12, 16), 1)};
        context.FillRectangle(brush, Rect(0, 0, w, h));
    }

    void DeckTile::Info(Media::DrawingContext& context, double w, double h, double em) const
    {
        const double pad = RoundToEven(em * 0.5);

        // .tag, with its code in the accent.
        const double tagSize = GuiTheme::PixelSize(em * 0.72);
        const auto code = DeckText::Run(::MphRead::NativeRuntime::ToUpperInvariant(Code), Deck::Body(true), tagSize,
            GuiTheme::AccentBrush);
        const double tagPadX = RoundToEven(tagSize * 0.45);
        const double tagPadY = RoundToEven(tagSize * 0.12);
        const Rect tag(pad, pad, RoundToEven(code->Width() + tagPadX * 2), RoundToEven(code->Height() + tagPadY * 2));
        context.DrawRectangle(Solid(Deck::Fade(0, 0.6)), std::make_shared<Media::Pen>(Solid(Deck::Fade(0xe6eaf2, 0.14)), 1),
            RoundedRect(tag, RoundToEven(tagSize * 0.25)));
        context.DrawText(*code, Point{tag.X + tagPadX, tag.Y + tagPadY});

        // .picked: a four-point lip the full width of the card's inside.
        const double wordSize = GuiTheme::PixelSize(em * 0.9);
        const double lip = 4;
        const double wordH = RoundToEven(wordSize * 1.7);
        double wordY = h - pad - wordH - lip;
        const std::string& word = _chosen ? ChosenVerb : Verb;
        if (word.empty())
        {
            wordY = h - pad + RoundToEven(em * 0.3);
        }
        else
        {
            const Rect slab(pad, wordY, MathMax(0.0, w - pad * 2), wordH);
            const Deck::Face faceColour = _chosen ? Deck::Face::Brass() : Deck::Face::Moss();
            Media::Color fill = faceColour.Fill;
            Media::Color lipColour = faceColour.Lip;
            if (_over)
            {
                fill = DeckPaint::Saturate(DeckPaint::Brightness(fill, 1.22), 1.15);
                lipColour = DeckPaint::Saturate(DeckPaint::Brightness(lipColour, 1.22), 1.15);
            }
            context.DrawRectangle(Solid(fill), nullptr, RoundedRect(slab, RoundToEven(wordSize * 0.55)),
                Media::BoxShadows(Deck::Shadow(0, lip, 0, 0, lipColour)));
            const double wordWidth = DeckText::MeasureTracked(word, Deck::Label(), wordSize, DeckText::LabelTracking);
            DeckText::DrawTracked(context, word, Deck::Label(), wordSize, GuiTheme::TextBrush,
                RoundToEven(slab.X + (slab.Width - wordWidth) / 2), slab.Y, slab.Height, DeckText::LabelTracking);
        }

        if (Blurb.empty())
        {
            return;
        }
        // .blurb: one line, above the word, trimmed to the card.
        const double blurbSize = GuiTheme::PixelSize(em * 0.76);
        const auto blurb = DeckText::Run(Blurb, Deck::Body(false), blurbSize, Solid(Media::Color::FromRgb(0xc7, 0xcf, 0xdd)),
            MathMax(10.0, w - pad * 2));
        context.DrawText(*blurb, Point{pad, RoundToEven(wordY - RoundToEven(em * 0.3) - blurb->Height())});
    }

    // ------------------------------------------------------------ DeckGrid

    DeckGrid::DeckGrid()
    {
        static const bool registered = []
        {
            AffectsMeasure<DeckGrid>(Deck::EmProperty);
            AffectsMeasure<DeckGrid>(Deck::FrameWidthProperty);
            return true;
        }();
        (void)registered;
        // In the tunnel, so the card the toolkit chose never sees it.
        AddHandler<Input::PointerPressedEventArgs>(PointerPressedEvent,
            [this](Interactivity::Interactive&, Input::PointerPressedEventArgs& e) { Pressed(e); },
            Interactivity::RoutingStrategies::Tunnel);
        AddHandler<Input::PointerEventArgs>(PointerMovedEvent,
            [this](Interactivity::Interactive&, Input::PointerEventArgs& e) { Moved(e); },
            Interactivity::RoutingStrategies::Tunnel);
        AddHandler<Input::PointerReleasedEventArgs>(PointerReleasedEvent,
            [this](Interactivity::Interactive&, Input::PointerReleasedEventArgs& e) { Released(e); },
            Interactivity::RoutingStrategies::Tunnel);
    }

    std::shared_ptr<DeckTile> DeckGrid::TileAt(Point at) const
    {
        for (const Controls::ControlPtr& child : Children)
        {
            if (auto tile = std::dynamic_pointer_cast<DeckTile>(child); tile != nullptr && tile->Bounds().Contains(at))
            {
                return tile;
            }
        }
        return nullptr;
    }

    void DeckGrid::Hover(std::shared_ptr<DeckTile> tile, Point at)
    {
        if (_over != tile)
        {
            if (_over != nullptr)
            {
                _over->Hover(false, Point{});
            }
            _over = tile;
        }
        if (tile != nullptr)
        {
            tile->Hover(true, at - static_cast<Vector>(tile->Bounds().Position()));
        }
    }

    void DeckGrid::Pressed(Input::PointerPressedEventArgs& e)
    {
        const Point at = e.GetPosition(this);
        _down = TileAt(at);
        Hover(_down, at);
        if (_down != nullptr)
        {
            _tap.Press(e, *this);
            _down->Focus();
        }
    }

    void DeckGrid::Moved(Input::PointerEventArgs& e)
    {
        const Point at = e.GetPosition(this);
        Hover(TileAt(at), at);
        if (_tap.Down())
        {
            _tap.Moved(e, *this);
        }
    }

    void DeckGrid::Released(Input::PointerReleasedEventArgs& e)
    {
        const std::shared_ptr<DeckTile> tile = _down;
        _down.reset();
        const bool tapped = _tap.Release(e, *this);
        if (tile == nullptr)
        {
            return;
        }
        // Ours either way: the card the toolkit would have sent this to is not
        // the one under the pointer.
        e.Handled = true;
        if (tapped && TileAt(e.GetPosition(this)) == tile)
        {
            tile->Fire();
        }
    }

    void DeckGrid::OnPointerExited(Input::PointerEventArgs& e)
    {
        Hover(nullptr, Point{});
        Panel::OnPointerExited(e);
    }

    void DeckGrid::OnPointerCaptureLost(Input::PointerCaptureLostEventArgs& e)
    {
        _tap.Cancel();
        _down.reset();
        Panel::OnPointerCaptureLost(e);
    }

    std::int32_t DeckGrid::Columns() const
    {
        return FixedColumns > 0 ? FixedColumns : Deck::GetFrameWidth(*this) <= TwoColumnFrame ? 2 : 3;
    }

    double DeckGrid::Gap() const
    {
        return RoundToEven(Deck::GetEm(*this) * 0.5);
    }

    Av::Size DeckGrid::MeasureOverride(Av::Size availableSize)
    {
        const std::int32_t columns = Columns();
        const double gap = Gap();
        const double width = std::isinf(availableSize.Width) ? Deck::GetEm(*this) * 40 : availableSize.Width;
        const double cell = MathMax(1.0, (width - gap * (columns - 1)) / columns);
        const double high = RoundToEven(cell / MathMax(0.1, Ratio));
        const Av::Size slot{cell, high};
        for (const Controls::ControlPtr& child : Children)
        {
            if (auto* tile = dynamic_cast<DeckTile*>(child.get()))
            {
                tile->Ratio = Ratio;
            }
            child->Measure(slot);
        }
        const auto count = static_cast<std::int32_t>(Children.Count());
        const std::int32_t rows = (count + columns - 1) / columns;
        return {width, rows * high + MathMax(0, rows - 1) * gap};
    }

    Av::Size DeckGrid::ArrangeOverride(Av::Size finalSize)
    {
        const std::int32_t columns = Columns();
        const double gap = Gap();
        const double cell = MathMax(1.0, (finalSize.Width - gap * (columns - 1)) / columns);
        const double high = RoundToEven(cell / MathMax(0.1, Ratio));
        for (std::size_t i = 0; i < Children.Count(); i++)
        {
            const auto row = static_cast<std::int32_t>(i) / columns;
            const auto column = static_cast<std::int32_t>(i) % columns;
            Children[i]->Arrange(Rect(column * (cell + gap), row * (high + gap), cell, high));
        }
        return finalSize;
    }
}
