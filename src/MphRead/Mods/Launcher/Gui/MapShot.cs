#if MPHREAD_AVALONIA
using System;
using System.Collections.Generic;
using System.IO;
using Avalonia.Media.Imaging;

namespace MphRead.Mods.Launcher.Gui
{
    /// <summary>
    /// The launcher's own map renders, cached, for the screens that want one
    /// behind something rather than beside it.
    ///
    /// <see cref="PlayScreen"/> already loads a preview for the map selected;
    /// what this is for is the rows, where every visible row wants a different
    /// one at once and decoding a PNG per row per frame is not a thing to do
    /// in <c>Render</c>. Decoded once per room key and held: there are thirty
    /// of them and the launcher is already holding the backdrop.
    ///
    /// <para>
    /// Nothing is generated here and nothing ships. These are the files the
    /// thumbnail pass writes out of the player's own game files, so a machine
    /// that has not run it gets no picture and the row is the row it always
    /// was -- which is why every caller has to handle null rather than being
    /// handed a placeholder.
    /// </para>
    /// </summary>
    internal static class MapShot
    {
        private static readonly Dictionary<string, Bitmap?> _cache = new(StringComparer.OrdinalIgnoreCase);

        public static Bitmap? For(string? roomKey)
        {
            if (string.IsNullOrEmpty(roomKey))
            {
                return null;
            }
            if (_cache.TryGetValue(roomKey, out Bitmap? hit))
            {
                return hit;
            }
            Bitmap? shot = null;
            try
            {
                string path = ThumbnailGenerator.PathFor(roomKey);
                if (File.Exists(path))
                {
                    // Through a MemoryStream so the file is not held open: the
                    // preview generator rewrites these while the launcher is up.
                    using var stream = new MemoryStream(File.ReadAllBytes(path));
                    shot = new Bitmap(stream);
                }
            }
            catch (Exception)
            {
                // A truncated PNG from an interrupted batch is no picture, not
                // a dead launcher.
            }
            _cache[roomKey] = shot;
            return shot;
        }

        /// <summary>Drop the lot: the thumbnail pass has rewritten them.</summary>
        public static void Forget()
        {
            _cache.Clear();
        }
    }
}
#endif
