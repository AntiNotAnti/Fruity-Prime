#include "LauncherPhoto.hpp"

#include "LauncherNoise.hpp"
#include "../DebugLog.hpp"
#include "../../Shaders.hpp"
#include "../../NativeRuntime/Avalonia/Media.hpp"
#include "../../NativeRuntime/Avalonia/Platform.hpp"
#include "../../NativeRuntime/OpenTK/GL.hpp"
#include "../../NativeRuntime/System/ExceptionText.hpp"

#include <exception>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace MphRead::Mods::Render
{
    namespace GL = ::OpenTK::Graphics::OpenGL::GL;
    namespace Avalonia = ::MphRead::NativeRuntime::Avalonia;

    namespace
    {
        struct ShaderCleanup final
        {
            std::int32_t Vertex = 0;
            std::int32_t Fragment = 0;

            ~ShaderCleanup()
            {
                if (Fragment != 0)
                {
                    GL::DeleteShader(Fragment);
                }
                if (Vertex != 0)
                {
                    GL::DeleteShader(Vertex);
                }
            }
        };
    }

    bool LauncherPhoto::_enabled = false;
    std::int32_t LauncherPhoto::_texture = 0;
    std::int32_t LauncherPhoto::_width = 0;
    std::int32_t LauncherPhoto::_height = 0;
    bool LauncherPhoto::_tried = false;
    std::int32_t LauncherPhoto::_program = 0;
    bool LauncherPhoto::_programTried = false;
    std::int32_t LauncherPhoto::_photoUniform = -1;
    std::int32_t LauncherPhoto::_noiseUniform = -1;
    std::int32_t LauncherPhoto::_strengthUniform = -1;

    void LauncherPhoto::Enabled(bool value) noexcept
    {
        _enabled = value;
    }

    bool LauncherPhoto::Enabled() noexcept
    {
        return _enabled;
    }

    bool LauncherPhoto::EnsureProgram()
    {
        if (_programTried)
        {
            return _program != 0;
        }
        _programTried = true;
        ShaderCleanup shaders;
        try
        {
            shaders.Vertex = GL::CreateShader(GL::ShaderType::VertexShader);
            GL::ShaderSource(shaders.Vertex, ::MphRead::Shaders::BackdropVertexShader);
            GL::CompileShader(shaders.Vertex);
            std::int32_t vertexOk = 0;
            GL::GetShader(shaders.Vertex, GL::ShaderParameter::CompileStatus, vertexOk);

            shaders.Fragment = GL::CreateShader(GL::ShaderType::FragmentShader);
            GL::ShaderSource(shaders.Fragment, ::MphRead::Shaders::BackdropFragmentShader);
            GL::CompileShader(shaders.Fragment);
            std::int32_t fragmentOk = 0;
            GL::GetShader(shaders.Fragment, GL::ShaderParameter::CompileStatus, fragmentOk);
            if (vertexOk == 0 || fragmentOk == 0)
            {
                DebugLog::Line("ui", "the moving backdrop's shaders would not compile: "
                    + GL::GetShaderInfoLog(shaders.Vertex) + " " + GL::GetShaderInfoLog(shaders.Fragment));
                return false;
            }

            const std::int32_t program = GL::CreateProgram();
            GL::AttachShader(program, shaders.Vertex);
            GL::AttachShader(program, shaders.Fragment);
            GL::LinkProgram(program);
            GL::DetachShader(program, shaders.Vertex);
            GL::DetachShader(program, shaders.Fragment);
            std::int32_t linked = 0;
            GL::GetProgram(program, GL::GetProgramParameterName::LinkStatus, linked);
            if (linked == 0)
            {
                DebugLog::Line("ui", "the moving backdrop would not link: " + GL::GetProgramInfoLog(program));
                GL::DeleteProgram(program);
                return false;
            }

            _program = program;
            _photoUniform = GL::GetUniformLocation(program, "photo");
            _noiseUniform = GL::GetUniformLocation(program, "noise");
            _strengthUniform = GL::GetUniformLocation(program, "strength");
            DebugLog::Line("ui", "the moving backdrop is on");
            return true;
        }
        catch (...)
        {
            _program = 0;
            DebugLog::Line("ui", "the moving backdrop could not be set up: "
                + ::MphRead::NativeRuntime::ExceptionMessage(std::current_exception()));
            return false;
        }
    }

    void LauncherPhoto::Draw(std::int32_t width, std::int32_t height)
    {
        if (!_enabled || width <= 0 || height <= 0 || !Ensure())
        {
            return;
        }

        const double window = static_cast<double>(width) / height;
        const double picture = static_cast<double>(_width) / _height;
        float u = 1;
        float v = 1;
        if (window > picture)
        {
            v = static_cast<float>(picture / window);
        }
        else
        {
            u = static_cast<float>(window / picture);
        }
        const float u0 = (1 - u) / 2;
        const float u1 = u0 + u;
        const float v0 = (1 - v) / 2;
        const float v1 = v0 + v;

        const bool moving = LauncherNoise::Step(width, height)
            && LauncherNoise::Texture() != 0 && EnsureProgram();
        GL::UseProgram(moving ? _program : 0);
        GL::Disable(GL::EnableCap::DepthTest);
        GL::Disable(GL::EnableCap::CullFace);
        GL::Disable(GL::EnableCap::AlphaTest);
        GL::Disable(GL::EnableCap::StencilTest);
        GL::Disable(GL::EnableCap::Blend);

        GL::ActiveTexture(GL::TextureUnit::Texture1);
        if (moving)
        {
            GL::Enable(GL::EnableCap::Texture2D);
            GL::BindTexture(GL::TextureTarget::Texture2D, LauncherNoise::Texture());
        }
        else
        {
            GL::BindTexture(GL::TextureTarget::Texture2D, 0);
            GL::Disable(GL::EnableCap::Texture2D);
        }
        GL::ActiveTexture(GL::TextureUnit::Texture0);
        GL::Enable(GL::EnableCap::Texture2D);
        GL::BindTexture(GL::TextureTarget::Texture2D, _texture);
        GL::TexEnv(GL::TextureEnvTarget::TextureEnv, GL::TextureEnvParameter::TextureEnvMode,
            static_cast<std::int32_t>(GL::TextureEnvMode::Replace));
        GL::Color4(1, 1, 1, 1);
        if (moving)
        {
            GL::Uniform1(_photoUniform, 0);
            GL::Uniform1(_noiseUniform, 1);
            GL::Uniform1(_strengthUniform, Strength);
        }

        GL::MatrixMode(GL::MatrixMode::Projection);
        GL::PushMatrix();
        GL::LoadIdentity();
        GL::MatrixMode(GL::MatrixMode::Modelview);
        GL::PushMatrix();
        GL::LoadIdentity();
        GL::Begin(GL::PrimitiveType::TriangleStrip);
        GL::MultiTexCoord2(GL::TextureUnit::Texture0, u1, v0);
        GL::MultiTexCoord2(GL::TextureUnit::Texture1, 1, 0);
        GL::Vertex3(1, 1, 0);
        GL::MultiTexCoord2(GL::TextureUnit::Texture0, u0, v0);
        GL::MultiTexCoord2(GL::TextureUnit::Texture1, 0, 0);
        GL::Vertex3(-1, 1, 0);
        GL::MultiTexCoord2(GL::TextureUnit::Texture0, u1, v1);
        GL::MultiTexCoord2(GL::TextureUnit::Texture1, 1, 1);
        GL::Vertex3(1, -1, 0);
        GL::MultiTexCoord2(GL::TextureUnit::Texture0, u0, v1);
        GL::MultiTexCoord2(GL::TextureUnit::Texture1, 0, 1);
        GL::Vertex3(-1, -1, 0);
        GL::End();
        GL::PopMatrix();
        GL::MatrixMode(GL::MatrixMode::Projection);
        GL::PopMatrix();
        GL::MatrixMode(GL::MatrixMode::Modelview);
        GL::TexEnv(GL::TextureEnvTarget::TextureEnv, GL::TextureEnvParameter::TextureEnvMode,
            static_cast<std::int32_t>(GL::TextureEnvMode::Modulate));
        GL::BindTexture(GL::TextureTarget::Texture2D, 0);
        if (moving)
        {
            GL::ActiveTexture(GL::TextureUnit::Texture1);
            GL::BindTexture(GL::TextureTarget::Texture2D, 0);
            GL::Disable(GL::EnableCap::Texture2D);
            GL::ActiveTexture(GL::TextureUnit::Texture0);
            GL::UseProgram(0);
        }
        GL::Enable(GL::EnableCap::Blend);
        GL::Enable(GL::EnableCap::DepthTest);
    }

    bool LauncherPhoto::Ensure()
    {
        if (_tried)
        {
            return _texture != 0;
        }
        _tried = true;
        try
        {
            constexpr std::string_view resource = "avares://FruityPrime/Assets/Backgrounds/launcher-bg.jpg";
            if (!Avalonia::Platform::AssetLoader::Exists(resource))
            {
                DebugLog::Line("ui", "no backdrop resource in this build");
                return false;
            }
            const std::vector<std::uint8_t> bytes = Avalonia::Platform::AssetLoader::Open(resource);
            const std::shared_ptr<Avalonia::Media::Imaging::Bitmap> image
                = Avalonia::Media::Imaging::Bitmap::FromBytes(bytes);
            const ::MphRead::NativeRuntime::Skia::Bitmap* pixels = image->Pixels();
            if (pixels == nullptr || pixels->Pixels() == nullptr)
            {
                DebugLog::Line("ui", "the backdrop decoded to nothing");
                return false;
            }
            _width = pixels->Width();
            _height = pixels->Height();
            if (_width <= 0 || _height <= 0)
            {
                return false;
            }

            GL::ActiveTexture(GL::TextureUnit::Texture0);
            _texture = Name;
            GL::BindTexture(GL::TextureTarget::Texture2D, _texture);
            GL::PixelStore(GL::PixelStoreParameter::UnpackAlignment, 4);
            GL::PixelStore(GL::PixelStoreParameter::UnpackRowLength, 0);
            GL::PixelStore(GL::PixelStoreParameter::UnpackSkipPixels, 0);
            GL::PixelStore(GL::PixelStoreParameter::UnpackSkipRows, 0);
            GL::PixelStore(GL::PixelStoreParameter::UnpackImageHeight, 0);
            GL::PixelStore(GL::PixelStoreParameter::UnpackSkipImages, 0);
            GL::PixelStore(GL::PixelStoreParameter::UnpackSwapBytes, 0);
            GL::PixelStore(GL::PixelStoreParameter::UnpackLsbFirst, 0);
            GL::TexImage2D(GL::TextureTarget::Texture2D, 0, GL::PixelInternalFormat::Rgba,
                _width, _height, 0, GL::PixelFormat::Rgba, GL::PixelType::UnsignedByte, pixels->Pixels());
            const ::OpenTK::Graphics::OpenGL::ErrorCode uploaded = GL::GetError();
            if (uploaded != ::OpenTK::Graphics::OpenGL::ErrorCode::NoError)
            {
                DebugLog::Line("ui", "backdrop upload said "
                    + ::OpenTK::Graphics::OpenGL::ToString(uploaded));
            }
            GL::TexParameter(GL::TextureTarget::Texture2D, GL::TextureParameterName::TextureBaseLevel, 0);
            GL::TexParameter(GL::TextureTarget::Texture2D, GL::TextureParameterName::TextureMaxLevel, 0);
            GL::TexParameter(GL::TextureTarget::Texture2D, GL::TextureParameterName::TextureMinFilter,
                static_cast<std::int32_t>(GL::TextureMinFilter::Linear));
            GL::TexParameter(GL::TextureTarget::Texture2D, GL::TextureParameterName::TextureMagFilter,
                static_cast<std::int32_t>(GL::TextureMagFilter::Linear));
            GL::TexParameter(GL::TextureTarget::Texture2D, GL::TextureParameterName::TextureWrapS,
                static_cast<std::int32_t>(GL::TextureWrapMode::ClampToEdge));
            GL::TexParameter(GL::TextureTarget::Texture2D, GL::TextureParameterName::TextureWrapT,
                static_cast<std::int32_t>(GL::TextureWrapMode::ClampToEdge));
            GL::BindTexture(GL::TextureTarget::Texture2D, 0);
            DebugLog::Line("ui", "launcher backdrop " + std::to_string(_width) + "x" + std::to_string(_height));
            return true;
        }
        catch (...)
        {
            _texture = 0;
            DebugLog::Line("ui", "no launcher backdrop: "
                + ::MphRead::NativeRuntime::ExceptionMessage(std::current_exception()));
            return false;
        }
    }
}
