using System;
using System.Collections.Generic;
using OpenTK.Graphics.OpenGL;
using MphRead.Mods.Render;

namespace MphRead
{
    public partial class Scene
    {
        private readonly HashSet<int> _mipmappedTextures = new();
        private int _maxAnisotropy;
        private QualityUniforms _rttQuality, _shiftQuality;
        private int _hudScaleUniform, _hudOpacityUniform;

        private readonly record struct QualityUniforms(int Flags, int Width, int Height);
        private static QualityUniforms QualityLocations(int program) => new(
            GL.GetUniformLocation(program, "quality_flags"), GL.GetUniformLocation(program, "quality_texel_w"),
            GL.GetUniformLocation(program, "quality_texel_h"));

        private void InitQuality()
        {
            _rttQuality = QualityLocations(_rttShaderProgramId);
            _shiftQuality = QualityLocations(_shiftShaderProgramId);
            _hudScaleUniform = GL.GetUniformLocation(_rttShaderProgramId, "hud_scale");
            _hudOpacityUniform = GL.GetUniformLocation(_rttShaderProgramId, "hud_opacity");
            // Cel and disruption reuse the RTT vertex stage but never scale its positions.
            foreach (int program in new[] { _celShaderProgramId, _shiftShaderProgramId })
            {
                GL.UseProgram(program);
                GL.Uniform1(GL.GetUniformLocation(program, "hud_scale"), 1f);
            }
            string extensions = GL.GetString(StringName.Extensions) ?? "";
            _maxAnisotropy = extensions.Contains("GL_EXT_texture_filter_anisotropic", StringComparison.Ordinal)
                || extensions.Contains("GL_ARB_texture_filter_anisotropic", StringComparison.Ordinal)
                ? Math.Max(1, GL.GetInteger((GetPName)0x84FF)) : 1;
        }

        private void ApplyWorldFiltering(int texture)
        {
            TextureQuality filtering = VisualOptions.Current.Filtering;
            bool mipmaps = filtering >= TextureQuality.Trilinear;
            // Generated on first use by the world, never for the HUD atlases or render targets.
            if (mipmaps && _mipmappedTextures.Add(texture)) GL.GenerateMipmap(GenerateMipmapTarget.Texture2D);
            GL.TexParameter(TextureTarget.Texture2D, TextureParameterName.TextureMinFilter,
                (int)(mipmaps ? TextureMinFilter.LinearMipmapLinear : filtering == TextureQuality.Pixel ? TextureMinFilter.Nearest : TextureMinFilter.Linear));
            GL.TexParameter(TextureTarget.Texture2D, TextureParameterName.TextureMagFilter,
                (int)(filtering == TextureQuality.Pixel ? TextureMagFilter.Nearest : TextureMagFilter.Linear));
            if (_maxAnisotropy > 1)
            {
                int requested = filtering >= TextureQuality.Anisotropic2 ? 1 << ((int)filtering - 2) : 1;
                GL.TexParameter(TextureTarget.Texture2D, (TextureParameterName)0x84FE, Math.Min(requested, _maxAnisotropy));
            }
        }

        private void SetQuality(QualityUniforms uniforms, bool world)
        {
            var settings = VisualOptions.Current;
            GL.Uniform4(uniforms.Flags, world && settings.Fxaa ? 1f : 0f,
                world && settings.SharpUpscaling && Mods.RenderOptions.ResolutionScale < 100 ? 1f : 0f,
                world && settings.EnhancedColor ? 1f : 0f, settings.ReducedFlashes ? 1f : 0f);
            GL.Uniform1(uniforms.Width, 1f / Math.Max(1, _targetSize.X));
            GL.Uniform1(uniforms.Height, 1f / Math.Max(1, _targetSize.Y));
        }

        public void SetAimHudScale(bool aiming)
        {
            var settings = VisualOptions.Current;
            GL.Uniform1(_hudScaleUniform, aiming ? 1f : settings.HudScale / 100f * (1 - settings.SafeZone / 50f));
        }

        private void SetHudAccessibility(bool enabled)
        {
            var settings = VisualOptions.Current;
            GL.Uniform1(_hudScaleUniform, enabled ? settings.HudScale / 100f * (1 - settings.SafeZone / 50f) : 1f);
            GL.Uniform1(_hudOpacityUniform, enabled ? settings.HudOpacity / 100f : 1f);
        }
    }
}
