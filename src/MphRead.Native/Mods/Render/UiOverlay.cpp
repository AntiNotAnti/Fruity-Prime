#include "UiOverlay.hpp"

#include "LauncherHunter.hpp"
#include "LauncherPhoto.hpp"
#include "../../NativeRuntime/OpenTK/GL.hpp"

#include <algorithm>

namespace MphRead::Mods::Render
{
    namespace GL = ::OpenTK::Graphics::OpenGL::GL;

    std::int32_t UiOverlay::_texture = 0;
    std::int32_t UiOverlay::_width = 0;
    std::int32_t UiOverlay::_height = 0;
    bool UiOverlay::_hasFrame = false;
    bool UiOverlay::_visible = false;

    bool UiOverlay::Visible() noexcept
    {
        return _visible;
    }

    void UiOverlay::Visible(bool value) noexcept
    {
        _visible = value;
    }

    bool UiOverlay::HasFrame() noexcept
    {
        return _hasFrame;
    }

    void UiOverlay::Upload(const void* pixels, std::int32_t width, std::int32_t height)
    {
        if (pixels == nullptr || width <= 0 || height <= 0)
        {
            return;
        }
        GL::ActiveTexture(GL::TextureUnit::Texture0);
        if (_texture == 0)
        {
            _texture = Name;
            GL::BindTexture(GL::TextureTarget::Texture2D, _texture);
            GL::TexParameter(GL::TextureTarget::Texture2D, GL::TextureParameterName::TextureMinFilter,
                static_cast<std::int32_t>(GL::TextureMinFilter::Linear));
            GL::TexParameter(GL::TextureTarget::Texture2D, GL::TextureParameterName::TextureMagFilter,
                static_cast<std::int32_t>(GL::TextureMagFilter::Linear));
            GL::TexParameter(GL::TextureTarget::Texture2D, GL::TextureParameterName::TextureWrapS,
                static_cast<std::int32_t>(GL::TextureWrapMode::ClampToEdge));
            GL::TexParameter(GL::TextureTarget::Texture2D, GL::TextureParameterName::TextureWrapT,
                static_cast<std::int32_t>(GL::TextureWrapMode::ClampToEdge));
        }
        else
        {
            GL::BindTexture(GL::TextureTarget::Texture2D, _texture);
        }
        GL::PixelStore(GL::PixelStoreParameter::UnpackAlignment, 4);
        if (width != _width || height != _height)
        {
            _width = width;
            _height = height;
            GL::TexImage2D(GL::TextureTarget::Texture2D, 0, GL::PixelInternalFormat::Rgba,
                width, height, 0, GL::PixelFormat::Rgba, GL::PixelType::UnsignedByte, pixels);
        }
        else
        {
            GL::TexSubImage2D(GL::TextureTarget::Texture2D, 0, 0, 0, width, height,
                GL::PixelFormat::Rgba, GL::PixelType::UnsignedByte, pixels);
        }
        GL::BindTexture(GL::TextureTarget::Texture2D, 0);
        _hasFrame = true;
    }

    void UiOverlay::Draw(std::int32_t width, std::int32_t height)
    {
        if (!_visible || !_hasFrame || _texture == 0)
        {
            return;
        }
        if (width > 0 && height > 0)
        {
            GL::Viewport(0, 0, width, height);
        }
        GL::UseProgram(0);
        GL::Disable(GL::EnableCap::DepthTest);
        GL::Disable(GL::EnableCap::CullFace);
        GL::Disable(GL::EnableCap::AlphaTest);
        GL::Disable(GL::EnableCap::StencilTest);
        GL::Enable(GL::EnableCap::Blend);
        GL::BlendFunc(GL::BlendingFactor::One, GL::BlendingFactor::OneMinusSrcAlpha);
        GL::ActiveTexture(GL::TextureUnit::Texture1);
        GL::BindTexture(GL::TextureTarget::Texture2D, 0);
        GL::Disable(GL::EnableCap::Texture2D);
        GL::ActiveTexture(GL::TextureUnit::Texture0);
        GL::Enable(GL::EnableCap::Texture2D);
        GL::BindTexture(GL::TextureTarget::Texture2D, _texture);
        GL::TexEnv(GL::TextureEnvTarget::TextureEnv, GL::TextureEnvParameter::TextureEnvMode,
            static_cast<std::int32_t>(GL::TextureEnvMode::Replace));
        GL::Color4(1, 1, 1, 1);
        GL::MatrixMode(GL::MatrixMode::Projection);
        GL::PushMatrix();
        GL::LoadIdentity();
        GL::MatrixMode(GL::MatrixMode::Modelview);
        GL::PushMatrix();
        GL::LoadIdentity();
        GL::Begin(GL::PrimitiveType::TriangleStrip);
        GL::TexCoord2(1, 0);
        GL::Vertex3(1, 1, 0);
        GL::TexCoord2(0, 0);
        GL::Vertex3(-1, 1, 0);
        GL::TexCoord2(1, 1);
        GL::Vertex3(1, -1, 0);
        GL::TexCoord2(0, 1);
        GL::Vertex3(-1, -1, 0);
        GL::End();
        GL::PopMatrix();
        GL::MatrixMode(GL::MatrixMode::Projection);
        GL::PopMatrix();
        GL::MatrixMode(GL::MatrixMode::Modelview);
        GL::TexEnv(GL::TextureEnvTarget::TextureEnv, GL::TextureEnvParameter::TextureEnvMode,
            static_cast<std::int32_t>(GL::TextureEnvMode::Modulate));
        GL::BindTexture(GL::TextureTarget::Texture2D, 0);
        GL::BlendFunc(GL::BlendingFactor::SrcAlpha, GL::BlendingFactor::OneMinusSrcAlpha);
        GL::Enable(GL::EnableCap::DepthTest);
    }

    void UiOverlay::DrawAlone(::MphRead::RenderWindow& window, std::int32_t width, std::int32_t height)
    {
        GL::Viewport(0, 0, std::max(width, 1), std::max(height, 1));
        GL::ClearColor(0, 0, 0, 1);
        GL::Clear(GL::ClearBufferMask::ColorBufferBit | GL::ClearBufferMask::DepthBufferBit
            | GL::ClearBufferMask::StencilBufferBit);
        LauncherPhoto::Draw(width, height);
        Draw(width, height);
        LauncherHunter::Draw(window, width, height);
    }

    void UiOverlay::Release()
    {
        if (_texture != 0)
        {
            GL::DeleteTexture(_texture);
            _texture = 0;
        }
        _width = 0;
        _height = 0;
        _hasFrame = false;
    }
}
