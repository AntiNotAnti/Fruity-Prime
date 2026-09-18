using System;
using System.IO;
using System.Linq;
using Avalonia.Controls;
using MphRead.Mods.Input;

namespace MphRead.Mods.Launcher.Gui
{
    internal sealed class GamepadProfilePanel : StackPanel
    {
        public GamepadProfilePanel(Action changed)
        {
            Spacing = 8;
            GamepadProfiles.Initialize();
            Children.Add(new Note("Controller profiles — " + GamepadProfiles.ActiveName));
            string[] names = GamepadProfiles.Profiles.Select(p => p.Name).ToArray();
            var choice = new ChoiceRow("Saved profile", names.Length == 0 ? new[] { "No saved profiles" } : names, 0);
            var name = new FieldRow("Profile name", "My controller", boxWidth: 260);
            var file = new FieldRow("Import / export file", Path.Combine(LauncherPrefs.Directory, "controller-profile.json"), boxWidth: 360);
            Children.Add(choice); Children.Add(name); Children.Add(file);
            var status = new Note(GamepadProfiles.Status);
            void Button(string label, Action action)
            {
                var button = new UiWord(label, 13);
                button.Click += (_, _) =>
                {
                    try { action(); status.Text = "Done."; }
                    catch (Exception ex) when (ex is IOException or UnauthorizedAccessException or ArgumentException or System.Text.Json.JsonException)
                    { status.Text = ex.Message; }
                };
                Children.Add(button);
            }
            Button("Save current as named profile", () => { GamepadProfiles.Save(name.Value); changed(); });
            Button("Load selected profile", () => { GamepadProfiles.Load(choice.Value); changed(); });
            Button("Use selected profile for this controller", () =>
            {
                var device = GamepadManager.ActiveDevice ?? throw new InvalidDataException("Connect and select a controller first.");
                GamepadProfiles.Assign(choice.Value, device); changed();
            });
            Button("Remove automatic profile assignment", () =>
            {
                var device = GamepadManager.ActiveDevice ?? throw new InvalidDataException("Connect and select a controller first.");
                GamepadProfiles.Unassign(device); changed();
            });
            Button("Export selected profile to file", () => GamepadProfiles.Export(choice.Value, file.Value));
            Button("Import profile from file", () => { GamepadProfiles.Import(file.Value); changed(); });
            Children.Add(status);
            Children.Add(new Note("Profiles contain controller settings only. Save replaces a profile with the same name. Import adds a profile; load it to apply. Desktop automatic selection identifies the controller model and firmware; identical controllers share that assignment."));
        }
    }
}
