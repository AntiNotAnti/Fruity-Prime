using System;
using System.Collections.Generic;
using System.Linq;
using System.Reflection;
using System.Runtime.CompilerServices;
using MphRead;
using MphRead.Mods;
using MphRead.Mods.Render;
using OpenTK.Graphics.OpenGL;
using OpenTK.Mathematics;

internal static class ShaderQualityChecks
{
    public static void Run(Action<bool, string> check)
    {
        Console.WriteLine($"QUALITY driver={GL.GetString(StringName.Renderer)} GL={GL.GetString(StringName.Version)}");
        EsShaders.CheckInSync();
        check(true, "desktop/ES shader sources in sync");
        var programs = new List<int>();
        var original = VisualOptions.Current;
        int originalScale = RenderOptions.ResolutionScale;
        int texture = 0, target = 0, framebuffer = 0;
        try
        {
            foreach (int scale in VisualOptions.ScaleStops)
            {
                RenderOptions.ResolutionScale = scale;
                check(RenderOptions.Scaled(640) == 640 * scale / 100 && RenderOptions.Scaled(0) >= 1,
                    $"render scale {scale}% dimensions and minimum allocation");
            }
            RenderOptions.ResolutionScale = 200;
            check(RenderOptions.Scaled(int.MaxValue) == int.MaxValue, "scaled allocation arithmetic cannot overflow");
            var clamped = new VisualSettings { HudScale = -1, TextScale = 999, TeamPalette = 999, Filtering = (TextureQuality)999 }.Validated();
            check(clamped.HudScale == 70 && clamped.TextScale == 125 && clamped.TeamPalette == 2 && clamped.Filtering == TextureQuality.Pixel,
                "untrusted preferences clamped");

            int Link(string vertex, string fragment)
            {
                int Compile(ShaderType type, string source)
                {
                    int id = GL.CreateShader(type); GL.ShaderSource(id, source); GL.CompileShader(id);
                    GL.GetShader(id, ShaderParameter.CompileStatus, out int ok);
                    if (ok == 0) throw new InvalidOperationException(GL.GetShaderInfoLog(id));
                    return id;
                }
                int v = Compile(ShaderType.VertexShader, vertex), f = Compile(ShaderType.FragmentShader, fragment);
                int program = GL.CreateProgram(); GL.AttachShader(program, v); GL.AttachShader(program, f); GL.LinkProgram(program);
                GL.DeleteShader(v); GL.DeleteShader(f); programs.Add(program);
                GL.GetProgram(program, GetProgramParameterName.LinkStatus, out int linked);
                if (linked == 0) throw new InvalidOperationException(GL.GetProgramInfoLog(program));
                return program;
            }
            int rtt = Link(Shaders.RttVertexShader, Shaders.RttFragmentShader);
            int shift = Link(Shaders.RttVertexShader, Shaders.ShiftFragmentShader);
            int cel = Link(Shaders.RttVertexShader, Shaders.CelFragmentShader);
            check(true, "desktop composite, disruption and cel shaders compile and link");
            // A desktop driver with ES compatibility can validate the actual ES sources;
            // this does not claim Android device execution.
            if ((GL.GetString(StringName.Extensions) ?? "").Contains("GL_ARB_ES3_compatibility", StringComparison.Ordinal))
            {
                Link(EsShaders.RttVertexShader, EsShaders.RttFragmentShader);
                Link(EsShaders.RttVertexShader, EsShaders.ShiftFragmentShader);
                Link(EsShaders.RttVertexShader, EsShaders.CelFragmentShader);
                check(true, "ES composite, disruption and cel shaders compile and link");
            }

            var scene = (Scene)RuntimeHelpers.GetUninitializedObject(typeof(Scene));
            void Field(string name, object value) => typeof(Scene).GetField(name, BindingFlags.Instance | BindingFlags.NonPublic)!.SetValue(scene, value);
            void Call(string name, params object[] args) => typeof(Scene).GetMethod(name, BindingFlags.Instance | BindingFlags.NonPublic)!.Invoke(scene, args);
            Field("_rttShaderProgramId", rtt); Field("_shiftShaderProgramId", shift); Field("_celShaderProgramId", cel);
            Field("_mipmappedTextures", new HashSet<int>()); Field("_targetSize", new Vector2i(16, 16));
            Call("InitQuality");
            texture = GL.GenTexture(); GL.BindTexture(TextureTarget.Texture2D, texture);
            byte[] pattern = new byte[16 * 16 * 4];
            for (int y = 0; y < 16; y++) for (int x = 0; x < 16; x++)
            {
                int i = (y * 16 + x) * 4;
                pattern[i] = (byte)(x < y ? 40 : 210); pattern[i + 1] = (byte)(50 + y * 10);
                pattern[i + 2] = (byte)(30 + x * 8); pattern[i + 3] = 255;
            }
            GL.TexImage2D(TextureTarget.Texture2D, 0, PixelInternalFormat.Rgba8, 16, 16, 0, PixelFormat.Rgba, PixelType.UnsignedByte, pattern);
            GL.TexParameter(TextureTarget.Texture2D, TextureParameterName.TextureWrapS, (int)TextureWrapMode.ClampToEdge);
            GL.TexParameter(TextureTarget.Texture2D, TextureParameterName.TextureWrapT, (int)TextureWrapMode.ClampToEdge);
            foreach (var filter in Enum.GetValues<TextureQuality>())
            {
                VisualOptions.Current = new() { Filtering = filter };
                Call("ApplyWorldFiltering", texture);
                GL.GetTexParameter(TextureTarget.Texture2D, GetTextureParameter.TextureMinFilter, out int min);
                check(min == (int)(filter >= TextureQuality.Trilinear ? TextureMinFilter.LinearMipmapLinear :
                    filter == TextureQuality.Pixel ? TextureMinFilter.Nearest : TextureMinFilter.Linear), $"{filter} sampler");
                if (filter >= TextureQuality.Trilinear)
                {
                    GL.GetTexLevelParameter(TextureTarget.Texture2D, 1, GetTextureParameter.TextureWidth, out int width);
                    check(width == 8, $"{filter} mipmap generated");
                }
                check(GL.GetError() == ErrorCode.NoError, $"{filter} no GL errors");
            }
            // Synthetic render target, so hidden-window backbuffer behavior is irrelevant.
            target = GL.GenTexture(); GL.BindTexture(TextureTarget.Texture2D, target);
            GL.TexImage2D(TextureTarget.Texture2D, 0, PixelInternalFormat.Rgba8, 64, 64, 0, PixelFormat.Rgba, PixelType.UnsignedByte, IntPtr.Zero);
            framebuffer = GL.GenFramebuffer(); GL.BindFramebuffer(FramebufferTarget.Framebuffer, framebuffer);
            GL.FramebufferTexture2D(FramebufferTarget.Framebuffer, FramebufferAttachment.ColorAttachment0, TextureTarget.Texture2D, target, 0);
            check(GL.CheckFramebufferStatus(FramebufferTarget.Framebuffer) == FramebufferErrorCode.FramebufferComplete, "quality test framebuffer complete");
            GL.Viewport(0, 0, 64, 64); GL.Disable(EnableCap.Blend); GL.Disable(EnableCap.DepthTest);
            GL.BindTexture(TextureTarget.Texture2D, texture);
            GL.TexParameter(TextureTarget.Texture2D, TextureParameterName.TextureMinFilter, (int)TextureMinFilter.Linear);
            GL.TexParameter(TextureTarget.Texture2D, TextureParameterName.TextureMagFilter, (int)TextureMagFilter.Linear);
            GL.UseProgram(rtt);
            GL.Uniform1(GL.GetUniformLocation(rtt, "tex"), 0);
            GL.Uniform1(GL.GetUniformLocation(rtt, "alpha"), 1f);
            GL.Uniform1(GL.GetUniformLocation(rtt, "quality_texel_w"), 1f / 16);
            GL.Uniform1(GL.GetUniformLocation(rtt, "quality_texel_h"), 1f / 16);
            byte[] Draw(Vector4 flags, float scale = 1, float opacity = 1)
            {
                GL.Uniform4(GL.GetUniformLocation(rtt, "quality_flags"), flags);
                GL.Uniform1(GL.GetUniformLocation(rtt, "hud_scale"), scale);
                GL.Uniform1(GL.GetUniformLocation(rtt, "hud_opacity"), opacity);
                GL.ClearColor(0, 0, 0, 0); GL.Clear(ClearBufferMask.ColorBufferBit);
                GL.Begin(PrimitiveType.TriangleStrip);
                GL.TexCoord2(0f, 0f); GL.Vertex2(-1f, -1f); GL.TexCoord2(1f, 0f); GL.Vertex2(1f, -1f);
                GL.TexCoord2(0f, 1f); GL.Vertex2(-1f, 1f); GL.TexCoord2(1f, 1f); GL.Vertex2(1f, 1f);
                GL.End();
                byte[] pixels = new byte[64 * 64 * 4];
                GL.ReadPixels(0, 0, 64, 64, PixelFormat.Rgba, PixelType.UnsignedByte, pixels);
                return pixels;
            }
            byte[] baseline = Draw(Vector4.Zero);
            check(baseline.Any(p => p > 0) && baseline.Where((_, i) => i % 4 == 3).All(p => p == 255), "default composite remains visible and opaque");
            foreach (var (name, flags) in new[] { ("FXAA", Vector4.UnitX), ("sharp upscale", Vector4.UnitY), ("enhanced color", Vector4.UnitZ) })
            {
                byte[] changed = Draw(flags);
                int differences = baseline.Zip(changed).Count(pair => pair.First != pair.Second);
                check(differences > 30, $"{name} changes rendered pixels ({differences})");
            }
            check(Draw(Vector4.Zero).SequenceEqual(baseline), "disabling enhancements restores exact output");
            byte[] hud = Draw(Vector4.Zero, .7f, .5f);
            check(hud[3] == 0 && hud[(32 * 64 + 32) * 4 + 3] is >= 127 and <= 128, "HUD scaling and opacity affect only requested draw");
            VisualOptions.Current = new() { HudScale = 70, SafeZone = 15 };
            scene.SetAimHudScale(true);
            GL.GetUniform(rtt, GL.GetUniformLocation(rtt, "hud_scale"), out float aimScale);
            check(aimScale == 1, "HUD safe zone cannot move the aiming reticle");
            check(GL.GetError() == ErrorCode.NoError, "quality rendering no GL errors");
        }
        catch (Exception ex) { Console.WriteLine(ex); check(false, "quality renderer checks completed"); }
        finally
        {
            GL.BindFramebuffer(FramebufferTarget.Framebuffer, 0); GL.UseProgram(0); GL.BindTexture(TextureTarget.Texture2D, 0);
            if (framebuffer != 0) GL.DeleteFramebuffer(framebuffer);
            if (target != 0) GL.DeleteTexture(target);
            if (texture != 0) GL.DeleteTexture(texture);
            foreach (int program in programs) GL.DeleteProgram(program);
            VisualOptions.Current = original; RenderOptions.ResolutionScale = originalScale;
        }
    }
}
