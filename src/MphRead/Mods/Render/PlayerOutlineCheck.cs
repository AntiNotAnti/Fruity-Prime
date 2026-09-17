#if !ANDROID
using System;
using OpenTK.Graphics.OpenGL;
using OpenTK.Mathematics;
using OpenTK.Windowing.Common;
using OpenTK.Windowing.Desktop;

namespace MphRead
{
    public partial class Scene
    {
        // Initialize renderer fields without constructing a game/save, which reads scan
        // strings from the user's extracted assets. This fixture draws only synthetic quads.
        private Scene(GameWindow outlineCheckWindow)
        {
            Size = new Vector2i(128);
            _keyboardState = outlineCheckWindow.KeyboardState;
            _mouseState = outlineCheckWindow.MouseState;
            _setTitle = _ => { };
            _close = () => { };
        }

        /// <summary>Exercises the actual mask, compositor and cel pass without proprietary assets.</summary>
        internal static int RunPlayerOutlineCheck()
        {
            using var window = new GameWindow(GameWindowSettings.Default, new NativeWindowSettings
            {
                ClientSize = new Vector2i(128, 128), StartVisible = false,
                // Match the compatibility-context request used on macOS.
                Profile = ContextProfile.Any, APIVersion = new Version(2, 1), Flags = ContextFlags.Default
            });
            Console.WriteLine($"OUTLINECHECK GL2.1 request, driver={GL.GetString(StringName.Version)}");
            var scene = new Scene(window);
            return scene.CheckPlayerOutlinePixels();
        }

        private int CheckPlayerOutlinePixels()
        {
            int failures = 0;
            Mods.RenderOptions.ResolutionScale = 100;
            Mods.RenderOptions.PlayerOutlineWidth = 4;
            Mods.RenderOptions.CelShading = true;
            Mods.RenderOptions.PlayerOutline = Mods.PlayerOutlineStyle.Off;
            InitShaders();
            int bodyList = GL.GenLists(1);
            GL.NewList(bodyList, ListMode.Compile);
            GL.Begin(PrimitiveType.Quads);
            GL.TexCoord3(0f, 0f, 0f);
            GL.Vertex3(-0.5f, -0.5f, 0f); GL.Vertex3(0.5f, -0.5f, 0f);
            GL.Vertex3(0.5f, 0.5f, 0f); GL.Vertex3(-0.5f, 0.5f, 0f);
            GL.End();
            GL.EndList();
            var body = new RenderItem
            {
                Type = RenderItemType.Mesh, Alpha = 1, CullingMode = CullingMode.Neither,
                Diffuse = new Vector3(0.2f, 0.35f, 0.6f), Transform = Matrix4.Identity,
                ListId = bodyList, PlayerOutlineColor = new Vector4(1, 0.05f, 0.05f, 1)
            };
            _usedRenderItems.Enqueue(body);
            _calibrateInk = false;
            _faceCulling = false;
            int Probe(string label, bool occluded = false)
            {
                GL.BindFramebuffer(FramebufferTarget.Framebuffer, _frameBuffer);
                UpdateDepthAttachment(_targetSize);
                GL.Viewport(0, 0, _targetSize.X, _targetSize.Y);
                GL.UseProgram(_shaderProgramId);
                GL.ClearColor(0, 0, 0, 1);
                GL.DepthMask(true);
                GL.ColorMask(true, true, true, true);
                GL.Disable(EnableCap.Blend);
                GL.Disable(EnableCap.StencilTest);
                GL.Disable(EnableCap.ScissorTest);
                GL.Disable(EnableCap.AlphaTest);
                GL.Enable(EnableCap.DepthTest);
                GL.DepthFunc(DepthFunction.Less);
                GL.Clear(ClearBufferMask.ColorBufferBit | ClearBufferMask.DepthBufferBit);
                Matrix4 identity = Matrix4.Identity;
                GL.UniformMatrix4(_shaderLocations.ProjectionMatrix, false, ref identity);
                GL.UniformMatrix4(_shaderLocations.ViewMatrix, false, ref identity);
                GL.Uniform1(_shaderLocations.UseFog, 0);
                GL.Uniform1(_shaderLocations.CelBands, Mods.RenderOptions.CelShading ? 8 : 0);
                GL.Uniform1(_shaderLocations.ShowColors, 1);
                GL.Uniform1(_playerOutlineMaskUniform, 0);
                RenderItem(body);
                if (occluded)
                {
                    var wall = new RenderItem
                    {
                        Type = RenderItemType.Mesh, Alpha = 1, CullingMode = CullingMode.Neither,
                        Diffuse = new Vector3(0.2f), Transform = Matrix4.CreateTranslation(0, 0, -0.5f), ListId = bodyList
                    };
                    RenderItem(wall);
                }
                DrawWorldOutlines();
                GL.BindFramebuffer(FramebufferTarget.Framebuffer, _frameBuffer);
                byte[] pixels = new byte[_targetSize.X * _targetSize.Y * 4];
                GL.ReadPixels(0, 0, _targetSize.X, _targetSize.Y, PixelFormat.Rgba, PixelType.UnsignedByte, pixels);
                int red = 0;
                for (int i = 0; i < pixels.Length; i += 4)
                {
                    if (pixels[i] > 180 && pixels[i + 1] < 80 && pixels[i + 2] < 80) red++;
                }
                ErrorCode error = GL.GetError();
                Console.WriteLine($"OUTLINECHECK {label}: red_pixels={red} gl={error}");
                if (error != ErrorCode.NoError) failures++;
                return red;
            }
            void Check(bool condition, string label)
            {
                Console.WriteLine($"OUTLINECHECK {(condition ? "PASS" : "FAIL")} {label}");
                if (!condition) failures++;
            }
            Check(Probe("off") == 0, "off leaves original surface");
            Mods.RenderOptions.PlayerOutline = Mods.PlayerOutlineStyle.Red;
            Check(Probe("enabled live") > 0, "live enable produces visible outline");
            Check(Probe("behind wall", occluded: true) == 0, "fully occluded body has no outline");
            Mods.RenderOptions.CelShading = true;
            Check(Probe("cel on") > 0, "cel shading preserves colored outline");
            Mods.RenderOptions.PlayerOutline = Mods.PlayerOutlineStyle.Off;
            Mods.RenderOptions.CelShading = false;
            Probe("disabled during depth switch");
            int newTexture = BindGetTexture(new[] { new ColorRgba(10, 20, 30, 255) }, 1, 1);
            Check(newTexture != _playerOutlineTexture, "new texture cannot reuse live player mask name");
            Mods.RenderOptions.PlayerOutline = Mods.PlayerOutlineStyle.Red;
            Check(Probe("cel off after new texture") > 0, "new uploads preserve outline with cel disabled");
            Mods.RenderOptions.PlayerOutline = Mods.PlayerOutlineStyle.Off;
            Mods.RenderOptions.CelShading = true;
            Probe("cel restored while outline disabled");
            Mods.RenderOptions.PlayerOutline = Mods.PlayerOutlineStyle.Red;
            Check(Probe("outline restored") > 0, "depth lifecycle preserves outline");
            Mods.RenderOptions.ResolutionScale = 50;
            OnResize();
            Check(Probe("half render scale") > 0, "scaled target preserves outline");
            Check(Probe("scaled wall", occluded: true) == 0, "scaled target preserves occlusion");
            Mods.RenderOptions.ResolutionScale = 25;
            Mods.RenderOptions.PlayerOutlineWidth = 1;
            OnResize();
            Check(Probe("quarter render scale, minimum width") > 0, "minimum width remains visible at minimum render scale");
            GL.DeleteLists(bodyList, 1);
            DisposePlayerOutlines();
            Console.WriteLine($"OUTLINECHECK failures={failures}");
            return failures == 0 ? 0 : 1;
        }
    }
}
#endif
