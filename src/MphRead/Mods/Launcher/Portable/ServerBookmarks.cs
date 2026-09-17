using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text.Json;

namespace MphRead.Mods.Launcher
{
    /// <summary>Bounded local history. Opening the browser never contacts a saved address implicitly.</summary>
    internal sealed class ServerBookmarks
    {
        public List<string> Favorites { get; set; } = new();
        public List<string> Recent { get; set; } = new();
        private static string FilePath => Path.Combine(LauncherPrefs.Directory, "servers.json");

        public static ServerBookmarks Load()
        {
            try
            {
                if (!File.Exists(FilePath) || new FileInfo(FilePath).Length > 65536) return new();
                var saved = JsonSerializer.Deserialize<ServerBookmarks>(File.ReadAllText(FilePath)) ?? new();
                saved.Favorites = Clean(saved.Favorites);
                saved.Recent = Clean(saved.Recent);
                return saved;
            }
            catch (Exception ex) when (ex is IOException or UnauthorizedAccessException or JsonException) { return new(); }
        }
        private static List<string> Clean(List<string>? entries) => (entries ?? new())
            .Where(e => !String.IsNullOrWhiteSpace(e) && e.Length <= 300 && !e.Any(Char.IsControl))
            .Distinct(StringComparer.OrdinalIgnoreCase).Take(32).ToList();
        public bool IsFavorite(string endpoint) => Favorites.Contains(endpoint, StringComparer.OrdinalIgnoreCase);
        public void ToggleFavorite(string endpoint)
        {
            if (IsFavorite(endpoint)) Favorites.RemoveAll(e => String.Equals(e, endpoint, StringComparison.OrdinalIgnoreCase));
            else Favorites.Insert(0, endpoint);
            Save();
        }
        public void Remember(string endpoint)
        {
            Recent.RemoveAll(e => String.Equals(e, endpoint, StringComparison.OrdinalIgnoreCase));
            Recent.Insert(0, endpoint);
            Save();
        }
        private void Save()
        {
            Favorites = Clean(Favorites); Recent = Clean(Recent);
            Directory.CreateDirectory(LauncherPrefs.Directory);
            string temporary = FilePath + ".tmp";
            File.WriteAllText(temporary, JsonSerializer.Serialize(this));
            File.Move(temporary, FilePath, overwrite: true);
        }
    }
}
