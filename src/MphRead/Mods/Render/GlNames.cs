namespace MphRead.Mods.Render
{
    /// <summary>
    /// Texture names for the scene's own render targets, chosen rather than
    /// asked for.
    ///
    /// The engine does not call <c>glGenTextures</c> for its textures: it
    /// counts (<c>Scene._textureCount</c>) and binds the number. A name taken
    /// from GenTextures is therefore a number some scene will count its way
    /// onto -- and with more than one scene in a session (the launcher's
    /// hunter stands one up) GenTextures stops handing back the number the
    /// counter expects. The match's screen texture then took a name the HUD
    /// later counted onto, and the scoreboard's hunter portraits came out as
    /// the frame upside down, or as the depth buffer: black.
    ///
    /// So the render targets live above anything the counter reaches, and
    /// above <see cref="UiOverlay"/>'s own name, which is chosen the same way.
    /// </summary>
    public static class GlNames
    {
        private static int _next = 2_000_000;

        public static int NextTexture()
        {
            return System.Threading.Interlocked.Increment(ref _next);
        }
    }
}
