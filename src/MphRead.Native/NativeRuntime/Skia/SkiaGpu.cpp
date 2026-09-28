#include "Skia.hpp"

#include "../OpenTK/GL.hpp"
#include "../OpenTK/GLFW.hpp"

#include <include/core/SkBlendMode.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkColor.h>
#include <include/core/SkData.h>
#include <include/core/SkFont.h>
#include <include/core/SkFontMgr.h>
#include <include/core/SkFontStyle.h>
#include <include/core/SkImage.h>
#include <include/core/SkImageInfo.h>
#include <include/core/SkMaskFilter.h>
#include <include/core/SkMatrix.h>
#include <include/core/SkPaint.h>
#include <include/core/SkPath.h>
#include <include/core/SkPixmap.h>
#include <include/core/SkSamplingOptions.h>
#include <include/core/SkSurface.h>
#include <include/core/SkTypeface.h>
#include <include/effects/SkDashPathEffect.h>
#include <include/effects/SkGradient.h>
#include <include/gpu/ganesh/GrBackendSurface.h>
#include <include/gpu/ganesh/GrDirectContext.h>
#include <include/gpu/ganesh/GrTypes.h>
#include <include/gpu/ganesh/SkSurfaceGanesh.h>
#include <include/gpu/ganesh/gl/GrGLAssembleInterface.h>
#include <include/gpu/ganesh/gl/GrGLBackendSurface.h>
#include <include/gpu/ganesh/gl/GrGLDirectContext.h>
#include <include/ports/SkFontMgr_data.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <unordered_map>
#include <utility>
#include <vector>

namespace MphRead::NativeRuntime::Skia
{
    namespace GL = ::OpenTK::Graphics::OpenGL::GL;

    namespace
    {
        constexpr std::int32_t GlFramebufferBinding = 0x8CA6;
        constexpr std::int32_t GlViewport = 0x0BA2;
        constexpr std::int32_t GlScissorBox = 0x0C10;

        [[nodiscard]] ::SkRect NativeRect(const Rect& r)
        {
            return ::SkRect::MakeLTRB(static_cast<float>(r.Left), static_cast<float>(r.Top),
                static_cast<float>(r.Right), static_cast<float>(r.Bottom));
        }

        [[nodiscard]] ::SkMatrix NativeMatrix(const Matrix& m)
        {
            return ::SkMatrix::MakeAll(static_cast<float>(m.ScaleX), static_cast<float>(m.SkewX),
                static_cast<float>(m.TransX), static_cast<float>(m.SkewY), static_cast<float>(m.ScaleY),
                static_cast<float>(m.TransY), 0.0F, 0.0F, 1.0F);
        }

        [[nodiscard]] ::SkColor4f NativeColor(const Color& c)
        {
            return {c.R / 255.0F, c.G / 255.0F, c.B / 255.0F, c.A / 255.0F};
        }

        [[nodiscard]] ::SkTileMode NativeTileMode(SpreadMethod spread)
        {
            switch (spread)
            {
            case SpreadMethod::Repeat:
                return ::SkTileMode::kRepeat;
            case SpreadMethod::Reflect:
                return ::SkTileMode::kMirror;
            default:
                return ::SkTileMode::kClamp;
            }
        }

        [[nodiscard]] ::SkBlendMode NativeBlend(BlendMode blend)
        {
            return blend == BlendMode::Overlay ? ::SkBlendMode::kOverlay : ::SkBlendMode::kSrcOver;
        }

        [[nodiscard]] ::SkSamplingOptions NativeSampling(FilterQuality quality)
        {
            switch (quality)
            {
            case FilterQuality::None:
                return ::SkSamplingOptions(::SkFilterMode::kNearest);
            case FilterQuality::High:
                return ::SkSamplingOptions(::SkCubicResampler::Mitchell());
            case FilterQuality::Low:
            case FilterQuality::Medium:
            default:
                return ::SkSamplingOptions(::SkFilterMode::kLinear);
            }
        }

        [[nodiscard]] sk_sp<::SkShader> NativeGradient(const Shader& shader)
        {
            const GradientDescriptor& description = shader.Descriptor();
            if (description.Stops.empty())
            {
                return nullptr;
            }

            std::vector<::SkColor4f> colors;
            std::vector<float> positions;
            colors.reserve(description.Stops.size());
            positions.reserve(description.Stops.size());
            for (const GradientStop& stop : description.Stops)
            {
                colors.push_back(NativeColor(stop.StopColor));
                positions.push_back(static_cast<float>(std::clamp(stop.Offset, 0.0, 1.0)));
            }
            const ::SkTileMode tile = NativeTileMode(description.Spread);
            ::SkGradient::Colors colorSet(
                ::SkSpan<const ::SkColor4f>(colors.data(), colors.size()),
                ::SkSpan<const float>(positions.data(), positions.size()), tile);
            ::SkGradient::Interpolation interpolation;
            interpolation.fInPremul = ::SkGradient::Interpolation::InPremul::kYes;
            const ::SkGradient gradient(colorSet, interpolation);

            if (description.Kind == GradientKind::Linear)
            {
                const ::SkPoint points[2]{
                    {static_cast<float>(description.Start.X), static_cast<float>(description.Start.Y)},
                    {static_cast<float>(description.End.X), static_cast<float>(description.End.Y)}
                };
                return ::SkShaders::LinearGradient(points, gradient);
            }

            const float rx = static_cast<float>(std::max(std::abs(description.RadiusX), 1e-6));
            const float ry = static_cast<float>(std::max(std::abs(description.RadiusY), 1e-6));
            const ::SkPoint focus{
                static_cast<float>((description.Origin.X - description.Centre.X) / rx),
                static_cast<float>((description.Origin.Y - description.Centre.Y) / ry)
            };
            const ::SkMatrix ellipse = ::SkMatrix::MakeAll(rx, 0.0F, static_cast<float>(description.Centre.X),
                0.0F, ry, static_cast<float>(description.Centre.Y), 0.0F, 0.0F, 1.0F);
            if (std::abs(focus.x()) < 1e-6F && std::abs(focus.y()) < 1e-6F)
            {
                return ::SkShaders::RadialGradient({0.0F, 0.0F}, 1.0F, gradient, &ellipse);
            }
            return ::SkShaders::TwoPointConicalGradient(focus, 0.0F, {0.0F, 0.0F}, 1.0F, gradient, &ellipse);
        }

        [[nodiscard]] ::SkPaint NativePaint(const Paint& paint, BlendMode blend = BlendMode::SrcOver)
        {
            ::SkPaint native;
            native.setAntiAlias(paint.Antialias);
            native.setBlendMode(NativeBlend(blend));
            if (paint.Gradient != nullptr)
            {
                native.setShader(NativeGradient(*paint.Gradient));
                native.setAlphaf(static_cast<float>(std::clamp(paint.Opacity, 0.0, 1.0)));
            }
            else
            {
                ::SkColor4f color = NativeColor(paint.Solid);
                color.fA *= static_cast<float>(std::clamp(paint.Opacity, 0.0, 1.0));
                native.setColor4f(color);
            }
            return native;
        }

        [[nodiscard]] GrGLFuncPtr ResolveSkiaGl(void*, const char name[])
        {
            const auto proc = ::OpenTK::Windowing::GraphicsLibraryFramework::GLFW::GetProcAddress(name);
            return reinterpret_cast<GrGLFuncPtr>(proc);
        }
    }

    struct GpuAccess final
    {
        [[nodiscard]] static ::SkPath Path(const Skia::Path& source)
        {
            ::SkPath path;
            path.setFillType(source.Rule == FillRule::EvenOdd ? ::SkPathFillType::kEvenOdd : ::SkPathFillType::kWinding);
            std::size_t point = 0;
            for (const Skia::Path::Verb verb : source._verbs)
            {
                switch (verb)
                {
                case Skia::Path::Verb::Move:
                    path.moveTo(static_cast<float>(source._points[point].X),
                        static_cast<float>(source._points[point].Y));
                    ++point;
                    break;
                case Skia::Path::Verb::Line:
                    path.lineTo(static_cast<float>(source._points[point].X),
                        static_cast<float>(source._points[point].Y));
                    ++point;
                    break;
                case Skia::Path::Verb::Quad:
                    path.quadTo(static_cast<float>(source._points[point].X),
                        static_cast<float>(source._points[point].Y),
                        static_cast<float>(source._points[point + 1].X),
                        static_cast<float>(source._points[point + 1].Y));
                    point += 2;
                    break;
                case Skia::Path::Verb::Cubic:
                    path.cubicTo(static_cast<float>(source._points[point].X),
                        static_cast<float>(source._points[point].Y),
                        static_cast<float>(source._points[point + 1].X),
                        static_cast<float>(source._points[point + 1].Y),
                        static_cast<float>(source._points[point + 2].X),
                        static_cast<float>(source._points[point + 2].Y));
                    point += 3;
                    break;
                case Skia::Path::Verb::Close:
                    path.close();
                    break;
                }
            }
            return path;
        }
    };

    struct GpuSurface::Impl final
    {
        struct ImageEntry final
        {
            std::uint64_t Revision = 0;
            sk_sp<::SkImage> Image;
        };

        struct FontEntry final
        {
            sk_sp<::SkData> Data;
            sk_sp<::SkFontMgr> Manager;
            sk_sp<::SkTypeface> Typeface;
        };

        sk_sp<::GrDirectContext> Context;
        sk_sp<::SkSurface> Surface;
        std::unordered_map<const Bitmap*, ImageEntry> Images;
        std::unordered_map<const Skia::Typeface*, FontEntry> Fonts;
        std::int32_t Width = 0;
        std::int32_t Height = 0;
        std::int32_t Texture = 0;
        bool InFrame = false;
        std::int32_t PreviousFramebuffer = 0;
        std::array<std::int32_t, 4> PreviousViewport{};
        std::array<std::int32_t, 4> PreviousScissor{};
        bool PreviousScissorEnabled = false;

        void EnsureContext()
        {
            if (Context != nullptr && !Context->abandoned())
            {
                return;
            }
            sk_sp<const ::GrGLInterface> interface = ::GrGLMakeAssembledInterface(nullptr, ResolveSkiaGl);
            if (interface == nullptr)
            {
                throw std::runtime_error("Skia could not assemble the current OpenGL interface.");
            }
            Context = ::GrDirectContexts::MakeGL(std::move(interface));
            if (Context == nullptr)
            {
                throw std::runtime_error("Skia could not create a Ganesh OpenGL context.");
            }
        }

        void EnsureSurface(std::int32_t width, std::int32_t height)
        {
            if (Surface != nullptr && Width == width && Height == height)
            {
                return;
            }
            if (!InFrame)
            {
                throw std::logic_error("Skia GPU surface resized outside a render frame.");
            }
            const ::SkImageInfo info = ::SkImageInfo::Make(width, height, kRGBA_8888_SkColorType, kPremul_SkAlphaType);
            Surface = ::SkSurfaces::RenderTarget(Context.get(), skgpu::Budgeted::kYes, info, 0,
                kBottomLeft_GrSurfaceOrigin, nullptr);
            if (Surface == nullptr)
            {
                throw std::runtime_error("Skia could not allocate the launcher Ganesh surface.");
            }
            Width = width;
            Height = height;
            Texture = 0;
            Images.clear();
            Surface->getCanvas()->clear(SK_ColorTRANSPARENT);
        }

        [[nodiscard]] ::SkCanvas& Canvas()
        {
            if (Surface == nullptr)
            {
                throw std::logic_error("Skia GPU surface has not been allocated.");
            }
            return *Surface->getCanvas();
        }

        [[nodiscard]] sk_sp<::SkImage> ImageFor(const Bitmap& bitmap, std::uint64_t revision)
        {
            ImageEntry& entry = Images[&bitmap];
            if (entry.Image != nullptr && entry.Revision == revision)
            {
                return entry.Image;
            }
            if (bitmap.Width() <= 0 || bitmap.Height() <= 0 || bitmap.Pixels() == nullptr)
            {
                entry = {};
                return nullptr;
            }
            const ::SkImageInfo info = ::SkImageInfo::Make(bitmap.Width(), bitmap.Height(),
                kRGBA_8888_SkColorType, kPremul_SkAlphaType);
            const ::SkPixmap pixmap(info, bitmap.Pixels(), static_cast<std::size_t>(bitmap.Width()) * 4);
            entry.Image = ::SkImages::RasterFromPixmapCopy(pixmap);
            entry.Revision = revision;
            return entry.Image;
        }

        [[nodiscard]] sk_sp<::SkTypeface> FontFor(const Skia::Typeface& typeface,
            const std::uint8_t* data, std::size_t size)
        {
            auto found = Fonts.find(&typeface);
            if (found != Fonts.end())
            {
                return found->second.Typeface;
            }
            if (data == nullptr || size == 0)
            {
                return nullptr;
            }
            FontEntry entry;
            entry.Data = ::SkData::MakeWithCopy(data, size);
            std::array<sk_sp<::SkData>, 1> fontData{entry.Data};
            entry.Manager = ::SkFontMgr_New_Custom_Data(
                ::SkSpan<sk_sp<::SkData>>(fontData.data(), fontData.size()));
            if (entry.Manager != nullptr)
            {
                entry.Typeface = entry.Manager->makeFromData(entry.Data);
            }
            sk_sp<::SkTypeface> result = entry.Typeface;
            Fonts.emplace(&typeface, std::move(entry));
            return result;
        }

        void RestoreGlState() noexcept
        {
            GL::BindFramebuffer(GL::FramebufferTarget::Framebuffer, PreviousFramebuffer);
            GL::Viewport(PreviousViewport[0], PreviousViewport[1], PreviousViewport[2], PreviousViewport[3]);
            GL::Scissor(PreviousScissor[0], PreviousScissor[1], PreviousScissor[2], PreviousScissor[3]);
            if (PreviousScissorEnabled)
            {
                GL::Enable(GL::EnableCap::ScissorTest);
            }
            else
            {
                GL::Disable(GL::EnableCap::ScissorTest);
            }
        }
    };

    GpuSurface::GpuSurface()
        : _impl(std::make_unique<Impl>())
    {
    }

    GpuSurface::~GpuSurface()
    {
        if (_impl != nullptr && _impl->Context != nullptr)
        {
            _impl->Context->abandonContext();
            _impl->Surface.reset();
            _impl->Context.reset();
        }
    }

    GpuSurface::GpuSurface(GpuSurface&&) noexcept = default;
    GpuSurface& GpuSurface::operator=(GpuSurface&&) noexcept = default;

    std::int32_t GpuSurface::Width() const noexcept
    {
        return _impl != nullptr ? _impl->Width : 0;
    }

    std::int32_t GpuSurface::Height() const noexcept
    {
        return _impl != nullptr ? _impl->Height : 0;
    }

    std::int32_t GpuSurface::TextureId() const noexcept
    {
        return _impl != nullptr ? _impl->Texture : 0;
    }

    void GpuSurface::BeginFrame()
    {
        if (_impl->InFrame)
        {
            throw std::logic_error("Skia GPU render frame is already active.");
        }
        _impl->PreviousFramebuffer = GL::GetInteger(GlFramebufferBinding);
        GL::GetIntegers(GlViewport, _impl->PreviousViewport.data());
        GL::GetIntegers(GlScissorBox, _impl->PreviousScissor.data());
        _impl->PreviousScissorEnabled = GL::IsEnabled(GL::EnableCap::ScissorTest);
        _impl->InFrame = true;
        try
        {
            _impl->EnsureContext();
            _impl->Context->resetContext();
        }
        catch (...)
        {
            _impl->InFrame = false;
            _impl->RestoreGlState();
            throw;
        }
    }

    void GpuSurface::EndFrame()
    {
        if (_impl == nullptr || !_impl->InFrame)
        {
            return;
        }
        try
        {
            if (_impl->Surface != nullptr && _impl->Context != nullptr)
            {
                _impl->Context->flushAndSubmit(_impl->Surface.get());
                const ::GrBackendTexture backend = ::SkSurfaces::GetBackendTexture(
                    _impl->Surface.get(), ::SkSurface::BackendHandleAccess::kFlushRead);
                ::GrGLTextureInfo info{};
                if (backend.isValid() && ::GrBackendTextures::GetGLTextureInfo(backend, &info))
                {
                    _impl->Texture = static_cast<std::int32_t>(info.fID);
                }
                else
                {
                    _impl->Texture = 0;
                }
            }
        }
        catch (...)
        {
            _impl->InFrame = false;
            _impl->RestoreGlState();
            throw;
        }
        _impl->InFrame = false;
        _impl->RestoreGlState();
    }

    void GpuSurface::Resize(std::int32_t width, std::int32_t height)
    {
        _impl->EnsureSurface(std::max(width, 1), std::max(height, 1));
    }

    void GpuSurface::Clear(Color color)
    {
        _impl->Canvas().clear(NativeColor(color));
    }

    void GpuSurface::ClearRect(Color color, std::int32_t x, std::int32_t y,
        std::int32_t width, std::int32_t height)
    {
        if (width <= 0 || height <= 0)
        {
            return;
        }
        ::SkCanvas& canvas = _impl->Canvas();
        const int saved = canvas.save();
        canvas.resetMatrix();
        canvas.clipRect(::SkRect::MakeXYWH(static_cast<float>(x), static_cast<float>(y),
            static_cast<float>(width), static_cast<float>(height)), ::SkClipOp::kIntersect, false);
        canvas.drawColor(NativeColor(color), ::SkBlendMode::kSrc);
        canvas.restoreToCount(saved);
    }

    void GpuSurface::Save()
    {
        (void)_impl->Canvas().save();
    }

    void GpuSurface::SaveLayerAlpha(double opacity)
    {
        (void)_impl->Canvas().saveLayerAlphaf(nullptr, static_cast<float>(std::clamp(opacity, 0.0, 1.0)));
    }

    void GpuSurface::Restore()
    {
        _impl->Canvas().restore();
    }

    void GpuSurface::Concat(const Matrix& matrix)
    {
        _impl->Canvas().concat(NativeMatrix(matrix));
    }

    void GpuSurface::SetMatrix(const Matrix& matrix)
    {
        _impl->Canvas().setMatrix(NativeMatrix(matrix));
    }

    Rect GpuSurface::DeviceClipBounds() const noexcept
    {
        if (_impl == nullptr || _impl->Surface == nullptr)
        {
            return {};
        }
        const ::SkIRect clip = _impl->Surface->getCanvas()->getDeviceClipBounds();
        return Rect{static_cast<double>(clip.left()), static_cast<double>(clip.top()),
            static_cast<double>(clip.right()), static_cast<double>(clip.bottom())};
    }

    void GpuSurface::ClipRect(const Rect& rect, bool antialias)
    {
        _impl->Canvas().clipRect(NativeRect(rect), ::SkClipOp::kIntersect, antialias);
    }

    void GpuSurface::ClipPath(const Path& path, bool antialias)
    {
        _impl->Canvas().clipPath(GpuAccess::Path(path), ::SkClipOp::kIntersect, antialias);
    }

    void GpuSurface::FillPath(const Path& path, const Paint& paint)
    {
        if (path.IsEmpty() || paint.Opacity <= 0.0)
        {
            return;
        }
        ::SkPaint native = NativePaint(paint);
        native.setStyle(::SkPaint::kFill_Style);
        _impl->Canvas().drawPath(GpuAccess::Path(path), native);
    }

    void GpuSurface::StrokePath(const Path& path, const StrokeStyle& stroke, const Paint& paint)
    {
        if (path.IsEmpty() || paint.Opacity <= 0.0 || stroke.Width <= 0.0)
        {
            return;
        }
        ::SkPaint native = NativePaint(paint);
        native.setStyle(::SkPaint::kStroke_Style);
        native.setStrokeWidth(static_cast<float>(stroke.Width));
        native.setStrokeMiter(static_cast<float>(stroke.MiterLimit));
        native.setStrokeCap(stroke.Cap == LineCap::Round ? ::SkPaint::kRound_Cap
            : stroke.Cap == LineCap::Square ? ::SkPaint::kSquare_Cap : ::SkPaint::kButt_Cap);
        native.setStrokeJoin(stroke.Join == LineJoin::Round ? ::SkPaint::kRound_Join
            : stroke.Join == LineJoin::Bevel ? ::SkPaint::kBevel_Join : ::SkPaint::kMiter_Join);
        if (!stroke.Dashes.empty())
        {
            std::vector<::SkScalar> intervals;
            intervals.reserve(stroke.Dashes.size());
            for (const double dash : stroke.Dashes)
            {
                intervals.push_back(static_cast<::SkScalar>(dash * stroke.Width));
            }
            native.setPathEffect(::SkDashPathEffect::Make(
                ::SkSpan<const ::SkScalar>(intervals.data(), intervals.size()),
                static_cast<::SkScalar>(stroke.DashOffset * stroke.Width)));
        }
        _impl->Canvas().drawPath(GpuAccess::Path(path), native);
    }

    void GpuSurface::DrawBitmap(const Bitmap& bitmap, const Rect& source, const Rect& destination,
        FilterQuality quality, double opacity, BlendMode blend)
    {
        if (bitmap.Width() <= 0 || bitmap.Height() <= 0 || source.IsEmpty()
            || destination.IsEmpty() || opacity <= 0.0)
        {
            return;
        }
        const std::uint64_t revision = bitmap.Revision();
        const sk_sp<::SkImage> image = _impl->ImageFor(bitmap, revision);
        if (image == nullptr)
        {
            return;
        }
        ::SkPaint paint;
        paint.setAlphaf(static_cast<float>(std::clamp(opacity, 0.0, 1.0)));
        paint.setBlendMode(NativeBlend(blend));
        _impl->Canvas().drawImageRect(image, NativeRect(source), NativeRect(destination),
            NativeSampling(quality), &paint, ::SkCanvas::kStrict_SrcRectConstraint);
    }

    void GpuSurface::DrawBoxShadow(const Path& shape, const BoxShadowSpec& shadow, const Rect& bounds,
        const std::array<Point, 4>& radii)
    {
        if (shadow.ShadowColor.A == 0)
        {
            return;
        }
        const double spreadAmount = shadow.Inset ? -shadow.Spread : shadow.Spread;
        std::array<Point, 4> grown = radii;
        for (Point& radius : grown)
        {
            if (radius.X > 0.0 || radius.Y > 0.0)
            {
                radius.X = std::max(0.0, radius.X + spreadAmount);
                radius.Y = std::max(0.0, radius.Y + spreadAmount);
            }
        }
        Path spread;
        spread.AddRoundRect(Rect{bounds.Left - spreadAmount + shadow.OffsetX,
            bounds.Top - spreadAmount + shadow.OffsetY,
            bounds.Right + spreadAmount + shadow.OffsetX,
            bounds.Bottom + spreadAmount + shadow.OffsetY}, grown);

        ::SkPaint paint;
        paint.setAntiAlias(true);
        paint.setColor4f(NativeColor(shadow.ShadowColor));
        const float sigma = static_cast<float>(ConvertRadiusToSigma(shadow.Blur));
        if (sigma > 0.01F)
        {
            paint.setMaskFilter(::SkMaskFilter::MakeBlur(kNormal_SkBlurStyle, sigma, true));
        }

        ::SkCanvas& canvas = _impl->Canvas();
        const int saved = canvas.save();
        const ::SkPath nativeShape = GpuAccess::Path(shape);
        if (!shadow.Inset)
        {
            canvas.clipPath(nativeShape, ::SkClipOp::kDifference, true);
            canvas.drawPath(GpuAccess::Path(spread), paint);
        }
        else
        {
            canvas.clipPath(nativeShape, ::SkClipOp::kIntersect, true);
            ::SkPath inverse;
            inverse.setFillType(::SkPathFillType::kEvenOdd);
            const double reach = std::ceil(std::max(1.0,
                shadow.Blur * 2.0 + std::abs(shadow.Spread) + 4.0));
            inverse.addRect(::SkRect::MakeLTRB(static_cast<float>(-reach), static_cast<float>(-reach),
                static_cast<float>(Width() + reach), static_cast<float>(Height() + reach)));
            inverse.addPath(GpuAccess::Path(spread));
            canvas.drawPath(inverse, paint);
        }
        canvas.restoreToCount(saved);
    }

    void GpuSurface::DrawText(std::u32string_view text, const Skia::Typeface& typeface,
        double size, Point origin, const Paint& paint)
    {
        if (text.empty() || size <= 0.0 || paint.Opacity <= 0.0)
        {
            return;
        }
        const sk_sp<::SkTypeface> face = _impl->FontFor(
            typeface, typeface.FontData(), typeface.FontDataSize());
        if (face == nullptr)
        {
            return;
        }
        ::SkFont font(face, static_cast<float>(size));
        ::SkPaint native = NativePaint(paint);
        double pen = 0.0;
        char32_t previous = 0;
        for (const char32_t code : text)
        {
            if (previous != 0)
            {
                pen += typeface.Kerning(previous, code, size);
            }
            _impl->Canvas().drawSimpleText(&code, sizeof(code), ::SkTextEncoding::kUTF32,
                static_cast<float>(origin.X + pen), static_cast<float>(origin.Y), font, native);
            pen += typeface.Advance(code, size);
            previous = code;
        }
    }
}
