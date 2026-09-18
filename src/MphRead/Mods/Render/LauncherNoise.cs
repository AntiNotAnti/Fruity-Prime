using System;
using System.Diagnostics;
using OpenTK.Graphics.OpenGL;

namespace MphRead.Mods.Render
{
    /// <summary>
    /// The front screen's moving ground: a small field of domain-warped value
    /// noise, laid over the photograph in <c>overlay</c> at sixty-two per
    /// cent.
    ///
    /// <para>
    /// This is the reference's <c>#backdrop</c> canvas, and it is the one
    /// thing on the front screen that is never still. Without it the screen is
    /// a photograph with a menu on it; with it the lava under the wordmark
    /// breathes, which is the whole reason that photograph was chosen.
    /// </para>
    ///
    /// <para>
    /// <b>Why it is here and not in the screens' own bitmap.</b> Two reasons,
    /// and either would be enough. The photograph is already GL's (see
    /// <see cref="LauncherPhoto"/>) and a layer that blends with it has to be
    /// on the same side of the fence. And the screens are rasterised by Skia
    /// on the CPU and only when something has happened -- an animated layer in
    /// there would mean re-rasterising the whole window thirty times a second
    /// for ever, which is exactly the cost <see cref="Launcher.Gui.BakedBackdrop"/>
    /// exists to avoid. Here it is a texture upload of a few tens of
    /// kilobytes and one quad, and the screens above it go on costing nothing
    /// while nobody touches them.
    /// </para>
    ///
    /// <para>
    /// <b>Why it needs a shader.</b> <c>mix-blend-mode: overlay</c> is
    /// multiply where the backdrop is dark and screen where it is light, and
    /// which one applies is decided per pixel <i>by the destination</i>. Fixed
    /// function blending cannot ask that question -- it can do multiply
    /// (<c>DstColor, Zero</c>) or screen (<c>OneMinusDstColor, One</c>) but
    /// not choose between them -- so the photograph and this are sampled
    /// together in one fragment and combined there. That also makes it one
    /// draw rather than two, and no read of the framebuffer at all.
    /// </para>
    ///
    /// <para>
    /// Everything below is the reference's own arithmetic: a 64x64 random
    /// grid, smoothstepped bilinear lookups, the field warped by two more
    /// lookups of itself, a radial falloff, and the two colours it mixes
    /// between. The numbers are not adjustable and are not meant to be --
    /// they are what makes it the same picture.
    /// </para>
    /// </summary>
    public static class LauncherNoise
    {
        /// <summary>
        /// Window points per noise cell. The reference's <c>CELL</c>, and the
        /// canvas is then magnified with <c>image-rendering: pixelated</c> --
        /// which is why the texture below is filtered nearest.
        /// </summary>
        private const int Cell = 6;

        /// <summary>
        /// The widest field that is worked out cell by cell.
        ///
        /// 1920 points at six to a cell is 320, so every window up to that is
        /// the reference's own resolution exactly and only a larger one gets
        /// coarser cells. It is a CPU loop on the thread drawing the frame:
        /// at 320x180 it is fifty-odd thousand cells thirty times a second,
        /// which does not show; letting a 4K window ask for four times that
        /// would start to.
        /// </summary>
        private const int MaxCells = 320;

        /// <summary>Milliseconds between fields. The reference's own 33.</summary>
        private const double Gap = 33;

        /// <summary>
        /// The texture name, chosen rather than asked for, one above
        /// <see cref="LauncherPhoto"/>'s. See the note there: the engine
        /// counts its own names up from one, so a name from GenTextures is a
        /// name the next match will draw a hunter into.
        /// </summary>
        private const int Name = 1_000_002;

        private const int GridSize = 64;

        private static readonly float[] _grid = new float[GridSize * GridSize];
        private static readonly Stopwatch _clock = Stopwatch.StartNew();

        private static int _texture;
        private static int _width;
        private static int _height;
        private static byte[] _pixels = Array.Empty<byte>();
        private static float[] _fall = Array.Empty<float>();
        private static double _drawnAt = -1000;
        private static bool _seeded;

        /// <summary>The field's texture name, or zero when there is nothing to draw.</summary>
        public static int Texture => _texture;

        /// <summary>
        /// Step the field and hand it to GL, at most once every
        /// <see cref="Gap"/> milliseconds however fast the window is going.
        ///
        /// Returns false when there is nothing usable, which is the caller's
        /// cue to draw the photograph the plain way. That fallback is the
        /// point: a front screen with a still backdrop is a front screen, and
        /// a front screen that does not come up is not.
        /// </summary>
        public static bool Step(int windowWidth, int windowHeight)
        {
            if (windowWidth <= 0 || windowHeight <= 0)
            {
                return false;
            }
            int w = Math.Clamp(windowWidth / Cell, 32, MaxCells);
            int h = Math.Clamp(windowHeight / Cell, 32, MaxCells);
            if (!Resize(w, h))
            {
                return false;
            }
            double now = _clock.Elapsed.TotalMilliseconds;
            if (_texture != 0 && now - _drawnAt < Gap)
            {
                // The field on the GPU is less than a frame old. Leave it.
                return true;
            }
            _drawnAt = now;
            Fill(now / 1000.0);
            return Upload();
        }

        /// <summary>
        /// The grid, the buffers and the falloff for a size just arrived at.
        ///
        /// The falloff is the reference's <c>fall[]</c>: the field is pulled
        /// down towards the corners so the picture keeps a middle, and it
        /// depends only on the shape, so it is worked out on a resize rather
        /// than thirty times a second.
        /// </summary>
        private static bool Resize(int w, int h)
        {
            if (!_seeded)
            {
                // A fixed seed, not the clock's. Two windows of the same
                // program should be looking at the same lava, and a picture
                // that cannot be reproduced is a picture nobody can report a
                // fault in.
                var random = new Random(0x46505250);
                for (int i = 0; i < _grid.Length; i++)
                {
                    _grid[i] = (float)random.NextDouble();
                }
                _seeded = true;
            }
            if (w == _width && h == _height && _pixels.Length != 0)
            {
                return true;
            }
            _width = w;
            _height = h;
            _pixels = new byte[w * h * 3];
            _fall = new float[w * h];
            for (int y = 0; y < h; y++)
            {
                for (int x = 0; x < w; x++)
                {
                    double dx = (x / (double)w - 0.5) * 2;
                    double dy = (y / (double)h - 0.5) * 2;
                    double d = Math.Min(1, Math.Sqrt(dx * dx * 0.78 + dy * dy) / 1.18);
                    _fall[y * w + x] = (float)(1 - d * d * 0.45);
                }
            }
            // The size changed, so whatever is on the GPU is the wrong shape:
            // force the next Step to upload rather than keep it.
            _drawnAt = -1000;
            return true;
        }

        private static double Smooth(double t) => t * t * (3 - 2 * t);

        /// <summary>
        /// One lookup into the 64x64 grid: bilinear between four corners, with
        /// the fractions smoothstepped and the indices wrapped.
        /// </summary>
        private static double Noise(double x, double y)
        {
            int xi = (int)Math.Floor(x);
            int yi = (int)Math.Floor(y);
            double xf = Smooth(x - xi);
            double yf = Smooth(y - yi);
            int y0 = (yi & 63) * GridSize;
            int y1 = ((yi + 1) & 63) * GridSize;
            int x0 = xi & 63;
            int x1 = (xi + 1) & 63;
            double a = _grid[y0 + x0];
            double b = _grid[y0 + x1];
            double c = _grid[y1 + x0];
            double d = _grid[y1 + x1];
            double t = a + (b - a) * xf;
            return t + ((c + (d - c) * xf) - t) * yf;
        }

        // The two colours the field mixes between and the floor under both.
        private static readonly double[] _hot = { 196, 96, 88 };
        private static readonly double[] _cold = { 48, 112, 186 };
        private static readonly double[] _floor = { 14, 20, 30 };

        /// <summary>
        /// The field at <paramref name="seconds"/>. Two lookups warp the
        /// coordinates a third is then read at, which is what turns a bed of
        /// blobs into something that flows.
        /// </summary>
        private static void Fill(double seconds)
        {
            double time = seconds * 0.26;
            int w = _width, h = _height;
            int p = 0, q = 0;
            for (int y = 0; y < h; y++)
            {
                double v = y * 0.055;
                for (int x = 0; x < w; x++)
                {
                    double u = x * 0.055;
                    double wx = Noise(u + time * 2.0, v) * 4.4;
                    double wy = Noise(u, v - time * 1.6) * 4.4;
                    double n = Noise(u + wx, v + wy);
                    n = n * n * (3 - 2 * n);
                    double shade = (0.18 + n * 0.95) * _fall[q++];
                    for (int c = 0; c < 3; c++)
                    {
                        _pixels[p++] = (byte)Math.Clamp(
                            _floor[c] + (_hot[c] * n + _cold[c] * (1 - n)) * shade, 0, 255);
                    }
                }
            }
        }

        private static bool Upload()
        {
            try
            {
                GL.ActiveTexture(TextureUnit.Texture0);
                bool fresh = _texture == 0;
                if (fresh)
                {
                    _texture = Name;
                }
                GL.BindTexture(TextureTarget.Texture2D, _texture);
                // Every piece of unpack state said out loud, for the reason
                // LauncherPhoto gives: these are context-wide, the thumbnail
                // sweeps and the screen capture both leave them somewhere
                // else, and a row length left behind by one of them starts
                // every row of this upload in the wrong place.
                GL.PixelStore(PixelStoreParameter.UnpackAlignment, 1);
                GL.PixelStore(PixelStoreParameter.UnpackRowLength, 0);
                GL.PixelStore(PixelStoreParameter.UnpackSkipPixels, 0);
                GL.PixelStore(PixelStoreParameter.UnpackSkipRows, 0);
                GL.PixelStore(PixelStoreParameter.UnpackSwapBytes, 0);
                GL.PixelStore(PixelStoreParameter.UnpackLsbFirst, 0);
                GL.TexImage2D(TextureTarget.Texture2D, 0, PixelInternalFormat.Rgb,
                    _width, _height, 0, PixelFormat.Rgb, PixelType.UnsignedByte, _pixels);
                // Nearest, both ways: the reference magnifies this canvas with
                // `image-rendering: pixelated`, and the blocky cells are the
                // look rather than an artefact of it being small.
                GL.TexParameter(TextureTarget.Texture2D,
                    TextureParameterName.TextureMinFilter, (int)TextureMinFilter.Nearest);
                GL.TexParameter(TextureTarget.Texture2D,
                    TextureParameterName.TextureMagFilter, (int)TextureMagFilter.Nearest);
                GL.TexParameter(TextureTarget.Texture2D,
                    TextureParameterName.TextureWrapS, (int)TextureWrapMode.ClampToEdge);
                GL.TexParameter(TextureTarget.Texture2D,
                    TextureParameterName.TextureWrapT, (int)TextureWrapMode.ClampToEdge);
                GL.BindTexture(TextureTarget.Texture2D, 0);
                return true;
            }
            catch (Exception ex)
            {
                Mods.DebugLog.Line("ui", $"the moving backdrop could not be uploaded: {ex.Message}");
                _texture = 0;
                return false;
            }
        }

        /// <summary>Give the texture back. The context has to be current.</summary>
        public static void Release()
        {
            if (_texture != 0)
            {
                GL.DeleteTexture(_texture);
                _texture = 0;
            }
            _width = 0;
            _height = 0;
            _drawnAt = -1000;
        }
    }
}
