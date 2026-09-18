#if MPHREAD_SHELL
using System;
using System.IO;
using System.Linq;
using Avalonia;
using Avalonia.Headless;
using Avalonia.VisualTree;
using Avalonia.Controls;
using Avalonia.Threading;
using MphRead.Mods.Input;

namespace MphRead.Mods.Launcher.Gui
{
    internal static class GamepadUiChecks
    {
        public static void Run(string? shots = null)
        {
            GamepadChecks.Check(GuiLauncher.EnsureSetup(), "headless UI initialization");
            var panel = new StackPanel();
            var first = new UiWord("First");
            var hidden = new UiWord("Hidden") { IsVisible = false };
            var disabled = new UiWord("Disabled") { IsEnabled = false };
            var last = new UiWord("Last");
            panel.Children.Add(first); panel.Children.Add(hidden); panel.Children.Add(disabled); panel.Children.Add(last);
            var window = new Window { Width = 600, Height = 400, Content = panel };
            window.Show(); window.UpdateLayout(); Dispatcher.UIThread.RunJobs();
            FocusNavigator.Ensure(panel);
            GamepadChecks.Check(first.IsFocused, "controller establishes focus");
            FocusNavigator.Move(panel, UiAction.Down);
            GamepadChecks.Check(last.IsFocused, "navigation skips hidden and disabled controls");
            int clicked = 0; last.Click += (_, _) => clicked++;
            FocusNavigator.Key(last, Avalonia.Input.Key.Enter);
            GamepadChecks.Check(clicked == 1, "controller activates existing UI control");
            var choice = new ChoiceRow("Option", new[] { "One", "Two" }, 0);
            panel.Children.Add(choice); window.UpdateLayout(); FocusNavigator.Focus(choice);
            FocusNavigator.Key(choice, Avalonia.Input.Key.Right);
            GamepadChecks.Check(choice.Index == 1, "choice row supports semantic arrows");
            ControllerNav.Identify(first, "fixture.first"); ControllerNav.Identify(choice, "fixture.choice");
            first.SetValue(ControllerNav.NavDownProperty, "fixture.choice");
            FocusNavigator.Focus(first); FocusNavigator.Move(panel, UiAction.Down);
            GamepadChecks.Check(choice.IsFocused, "explicit navigation neighbor takes precedence over geometry");
            panel.SetValue(ControllerNav.NavWrapProperty, true);
            FocusNavigator.Focus(first); FocusNavigator.Move(panel, UiAction.Up);
            GamepadChecks.Check(choice.IsFocused, "upward wrapping reaches the last eligible control");
            var text = new TextBox { Text = "Player", Width = 250 };
            panel.Children.Add(text); window.UpdateLayout();
            var keyboard = new ControllerKeyboard(text, () => { });
            Dispatcher.UIThread.RunJobs();
            GamepadChecks.Check(FocusNavigator.Ensure(keyboard.NavigationRoot) != null, "controller text entry has focus");
            keyboard.Close(false);
            GamepadChecks.Check(text.Text == "Player", "cancel text entry preserves value");
            var confirm = new ConfirmScreen("Leave the match?");
            bool? answer = null; confirm.Answered += (_, value) => answer = value;
            window.Content = confirm; window.UpdateLayout(); Dispatcher.UIThread.RunJobs();
            var cancel = FocusNavigator.Ensure(confirm);
            GamepadChecks.Check(cancel != null, "confirmation has focus");
            window.Content = null;
            var modalRoot = new Panel(); modalRoot.Children.Add(panel); modalRoot.Children.Add(confirm);
            window.Content = modalRoot; window.UpdateLayout(); Dispatcher.UIThread.RunJobs();
            FocusNavigator.Focus(first);
            GamepadChecks.Check(FocusNavigator.Ensure(modalRoot) == cancel, "modal scope contains focus even when underlying control was focused");
            FocusNavigator.Key(cancel!, Avalonia.Input.Key.Escape);
            GamepadChecks.Check(answer == false, "controller Back dismisses confirmation");
            var outer = UiLayout.Backdrop(); var inner = UiLayout.Backdrop();
            var scaledText = new TextBlock { Text = "Scale once", FontSize = 20 };
            inner.Children.Add(scaledText); outer.Children.Add(inner); window.Content = outer;
            window.UpdateLayout(); Dispatcher.UIThread.RunJobs();
            var savedVisual = Mods.Render.VisualOptions.Current;
            Mods.Render.VisualOptions.Current = savedVisual with { TextScale = 125 };
            Dispatcher.UIThread.RunJobs();
            GamepadChecks.Check(Math.Abs(scaledText.FontSize - 20 * 125.0 / savedVisual.TextScale) < .01,
                "nested screens apply accessibility text scaling once");
            Mods.Render.VisualOptions.Current = savedVisual;
            Dispatcher.UIThread.RunJobs();
            var settings = new SettingsView(new MenuSettings());
            window.Width = 960; window.Height = 660; window.Content = settings;
            settings.ShowSection("Display"); window.UpdateLayout(); Dispatcher.UIThread.RunJobs();
            var graphics = settings.GetVisualDescendants().OfType<ChoiceRow>()
                .First(row => Mods.Render.VisualOptions.PresetNames.Contains(row.Value));
            var filtering = settings.GetVisualDescendants().OfType<ChoiceRow>()
                .First(row => Mods.Render.VisualOptions.FilterNames.Contains(row.Value));
            var originalVisual = Mods.Render.VisualOptions.Current;
            graphics.Index = (int)Mods.Render.GraphicsPreset.Ultra;
            GamepadChecks.Check(filtering.Value == "Anisotropic 16x", "graphics preset updates individual controls");
            filtering.Index = (int)Mods.Render.TextureQuality.Pixel;
            GamepadChecks.Check(graphics.Value == "Custom", "individual graphics choice returns preset to Custom");
            GamepadChecks.Check(Mods.Render.VisualOptions.Current == originalVisual, "graphics draft does not apply before Save");
            settings.ShowSection("Controller"); window.UpdateLayout(); Dispatcher.UIThread.RunJobs();
            var rows = settings.GetVisualDescendants().OfType<PadRow>().ToArray();
            GamepadChecks.Check(rows.Length == PadBindings.Actions.Count, "every pad action appears in settings");
            FocusNavigator.Focus(rows[^1]); window.UpdateLayout(); Dispatcher.UIThread.RunJobs();
            GamepadChecks.Check(rows[^1].IsFocused, "last binding reachable through scrolling");
            var rowPoint = rows[^1].TranslatePoint(new Point(), window);
            GamepadChecks.Check(rowPoint.HasValue && rowPoint.Value.Y >= 120 && rowPoint.Value.Y + rows[^1].Bounds.Height <= 590,
                "focus scrolls binding inside the viewport");
            if (shots != null)
            {
                Directory.CreateDirectory(shots);
                AvaloniaHeadlessPlatform.ForceRenderTimerTick();
                using var bitmap = window.GetLastRenderedFrame();
                bitmap?.Save(Path.Combine(shots, "controller-bindings.png"));
                var gamepad = settings.GetVisualDescendants().OfType<GamepadSettingsPanel>().First();
                FocusNavigator.Focus(gamepad.GetVisualDescendants().OfType<SliderRow>().First());
                window.UpdateLayout(); Dispatcher.UIThread.RunJobs();
                AvaloniaHeadlessPlatform.ForceRenderTimerTick();
                using var calibration = window.GetLastRenderedFrame();
                calibration?.Save(Path.Combine(shots, "controller-settings.png"));
            }
            CheckControllerSettings(window, settings, shots);
            CheckSetup(window);
            var pause = new PauseMenuView(false);
            window.Content = pause; window.UpdateLayout(); Dispatcher.UIThread.RunJobs();
            GamepadChecks.Check(FocusNavigator.Ensure(pause) != null, "pause menu is controller focusable");
            // Android hosts PauseMenuView in StartScreen rather than InGameMenu,
            // so Back must reach the view's resume callback without a desktop host.
            int resumed = 0;
            pause.Resumed += (_, _) => resumed++;
            var navigation = new GamepadNavigation();
            GamepadManager.UpdateDevice("pause-test", new GamepadState { Name = "Pause test" }, true);
            navigation.Update(pause);
            foreach (var button in new[] { GamepadButtons.B, GamepadButtons.Start })
            {
                GamepadManager.UpdateDevice("pause-test", new GamepadState { Name = "Pause test", Buttons = button }, true);
                navigation.Update(pause);
                GamepadManager.UpdateDevice("pause-test", new GamepadState { Name = "Pause test" }, true);
                navigation.Update(pause);
            }
            GamepadChecks.Check(resumed == 2, "B and Start resume the Android-hosted pause menu");
            GamepadManager.RemoveDevice("pause-test");
            Network.MapVote.Apply(new Network.VoteStatePacket
            {
                State = Network.VoteStatePacket.StateRunning, RoomKey = "test", Proposer = "Player", Seconds = 30
            });
            pause.RefreshVote(); window.UpdateLayout();
            var vote = pause.GetVisualDescendants().OfType<UiWord>().First(w => w.Text == "Accept map vote");
            FocusNavigator.Focus(vote);
            GamepadChecks.Check(vote.IsVisible && vote.IsFocused, "active map vote can be reached with controller focus");
            Network.MapVote.Reset(); pause.RefreshVote(); window.UpdateLayout();
            GamepadChecks.Check(!vote.IsVisible && !vote.IsFocused, "expired vote restores pause focus");
            // Run the real binding row against synthetic normalized device events.
            PadBindings.Reset();
            var binding = new PadRow(PadAction.Scan);
            window.Content = binding; window.UpdateLayout(); binding.Focus();
            void Pad(GamepadButtons buttons)
            {
                GamepadManager.UpdateDevice("ui-test", new GamepadState { Connected = true, Name = "UI test", Buttons = buttons }, true);
                binding.Check();
            }
            Pad(GamepadButtons.A);
            FocusNavigator.Key(binding, Avalonia.Input.Key.Enter);
            binding.Check();
            GamepadChecks.Check(PadBindings.Get(PadAction.Scan) == GamepadButtons.X, "opening Accept cannot bind itself");
            Pad(0); Pad(GamepadButtons.RightBumper);
            GamepadChecks.Check(GamepadContexts.Capturing, "binding conflict waits for a decision");
            Pad(0); Pad(GamepadButtons.B);
            GamepadChecks.Check(PadBindings.Get(PadAction.Scan) == GamepadButtons.X, "cancel conflict preserves mapping");
            Pad(0); FocusNavigator.Key(binding, Avalonia.Input.Key.Enter); Pad(GamepadButtons.Back);
            GamepadChecks.Check(PadBindings.Get(PadAction.Scan) == 0, "controller can clear a binding");
            Pad(0); FocusNavigator.Key(binding, Avalonia.Input.Key.Enter);
            GamepadManager.RemoveDevice("ui-test"); binding.Check();
            GamepadChecks.Check(!GamepadContexts.Capturing, "disconnect exits binding capture");
            window.Close();
        }

        private static void CheckSetup(Window window)
        {
            long now = 0; int applied = 0;
            var setup = new GamepadSetupPanel(() => applied++, () => now);
            window.Content = setup; window.UpdateLayout(); Dispatcher.UIThread.RunJobs();
            void Pad(float x = 0, float y = 0, float trigger = 0, GamepadButtons buttons = 0)
                => GamepadManager.UpdateDevice("setup-check", new GamepadState
                    { LeftX = x, LeftY = y, RightX = x, RightY = y, LeftTrigger = trigger, RightTrigger = trigger, Buttons = buttons }, true);
            void Click(string label)
            {
                var button = setup.Children.OfType<UiWord>().First(w => w.Text == label);
                FocusNavigator.Focus(button); FocusNavigator.Key(button, Avalonia.Input.Key.Enter);
            }
            Pad(); Click("Calibrate sticks and triggers");
            GamepadChecks.Check(GamepadContexts.Capturing, "calibration blocks menu and gameplay navigation");
            float previous = GamepadOptions.LeftInner;
            for (now = 1100; now < 3500; now += 100) { Pad(.02f, .03f); setup.Tick(); }
            for (now = 3600; now <= 10500; now += 100) { Pad(MathF.Cos(now / 100f), MathF.Sin(now / 100f), 1); setup.Tick(); }
            GamepadChecks.Check(!GamepadContexts.Capturing && GamepadOptions.LeftInner == previous
                && setup.Children.OfType<UiWord>().First(w => w.Text == "Apply measured setup").IsEnabled,
                "completed calibration previews measurements without applying them");
            Click("Apply measured setup");
            GamepadChecks.Check(applied == 1 && Math.Abs(GamepadOptions.LeftCalibration.CenterX - .02f) < .001f,
                "calibration Apply commits the measured values");
            Pad(); Click("Calibrate sticks and triggers"); now += 100; Pad(buttons: GamepadButtons.B); setup.Tick();
            GamepadChecks.Check(!GamepadContexts.Capturing && applied == 1, "controller Back cancels calibration without applying");
            Pad(); Click("Calibrate sticks and triggers");
            GamepadManager.RemoveDevice("setup-check"); setup.Tick();
            GamepadChecks.Check(!GamepadContexts.Capturing && applied == 1, "disconnect cancels calibration without applying");
            GamepadOptions.Reset(); PadBindings.Reset();
        }

        private static void CheckControllerSettings(Window window, SettingsView settings, string? shots)
        {
            var panel = settings.GetVisualDescendants().OfType<GamepadSettingsPanel>().First();
            PadBindings.ApplyPreset("Default"); panel.Reload(); window.UpdateLayout();
            var preset = panel.Children.OfType<ChoiceRow>().First(r => r.Value == "Default");
            FocusNavigator.Focus(preset); preset.Index = 1;
            Dispatcher.UIThread.RunJobs(); window.UpdateLayout();
            preset = panel.Children.OfType<ChoiceRow>().First(r => r.Value == "Bumper Jumper");
            GamepadChecks.Check(preset.IsFocused && PadBindings.Get(PadAction.Jump) == GamepadButtons.LeftBumper,
                "changing controller preset applies bindings and retains focus");
            FocusNavigator.Key(preset, Avalonia.Input.Key.Right);
            Dispatcher.UIThread.RunJobs(); window.UpdateLayout();
            GamepadChecks.Check(GamepadOptions.Southpaw && PadBindings.Preset == "Southpaw",
                "consecutive controller preset changes remain usable");
            PadBindings.ApplyPreset("Default"); panel.Reload(); window.UpdateLayout();

            var navigation = new GamepadNavigation();
            void Pad(GamepadButtons buttons = 0, float rt = 0)
                => GamepadManager.UpdateDevice("settings-xbox", new GamepadState
                    { Name = "Xbox Series controller", Buttons = buttons, RightTrigger = rt }, true,
                    GamepadFamily.Xbox, mapping: "Xbox Bluetooth compatibility");
            Pad(); navigation.Update(settings);
            settings.ShowSection(OperatingSystem.IsAndroid() ? "Touch and mouse" : "Mouse and stylus"); window.UpdateLayout();
            var jumpKey = settings.GetVisualDescendants().OfType<KeyRow>().First(r => r.BindingName == "Jump");
            var jumpProperty = InputSettings.Bindings.First(p => p.Name == "Jump");
            string keyboardBefore = InputSettings.Describe(InputSettings.Bind(jumpProperty));
            FocusNavigator.Focus(jumpKey);
            FocusNavigator.Key(jumpKey, Avalonia.Input.Key.Enter);
            Pad(rt: 1); navigation.Update(settings); window.UpdateLayout();
            var jumpPad = settings.GetVisualDescendants().OfType<PadRow>().First(r => r.Action == PadAction.Jump);
            GamepadChecks.Check(jumpPad.IsFocused && GamepadContexts.Capturing && !jumpKey.Listening,
                "controller trigger in keyboard capture opens the matching controller action");
            Pad(); jumpPad.Check(); Pad(GamepadButtons.A); jumpPad.Check();
            GamepadChecks.Check(PadBindings.Get(PadAction.Jump) == GamepadButtons.RightTrigger
                && PadBindings.Get(PadAction.Shoot) == GamepadButtons.A && !GamepadContexts.Capturing,
                "Accept confirms the default Swap instead of silently cancelling a rebind");
            GamepadChecks.Check(InputSettings.Describe(InputSettings.Bind(jumpProperty)) == keyboardBefore,
                "controller rebinding preserves the actual keyboard key");

            panel.RefreshLabels();
            GamepadChecks.Check(panel.Children.OfType<ChoiceRow>().Any(r => r.Value == "Custom"),
                "binding changes update the displayed controller preset");
            settings.ShowSection(OperatingSystem.IsAndroid() ? "Touch and mouse" : "Mouse and stylus"); window.UpdateLayout(); FocusNavigator.Focus(jumpKey);
            Pad(); navigation.Update(settings); Pad(GamepadButtons.A); navigation.Update(settings);
            GamepadChecks.Check(jumpPad.IsFocused && GamepadContexts.Capturing && !jumpKey.Listening,
                "controller Accept on a keyboard action enters controller capture");
            Pad(); jumpPad.Check(); Pad(GamepadButtons.B); jumpPad.Check();
            GamepadChecks.Check(!GamepadContexts.Capturing, "Back cancels redirected capture");
            settings.ShowSection(OperatingSystem.IsAndroid() ? "Touch and mouse" : "Mouse and stylus"); window.UpdateLayout();
            var moveKey = settings.GetVisualDescendants().OfType<KeyRow>().First(r => r.BindingName == "MoveUp");
            var moveProperty = InputSettings.Bindings.First(p => p.Name == "MoveUp");
            string movementBefore = InputSettings.Describe(InputSettings.Bind(moveProperty));
            FocusNavigator.Focus(moveKey); Pad(); navigation.Update(settings);
            Pad(GamepadButtons.A); navigation.Update(settings);
            GamepadChecks.Check(!moveKey.Listening && InputSettings.Describe(InputSettings.Bind(moveProperty)) == movementBefore,
                "keyboard-only rows never bind synthetic controller Enter");
            settings.ShowSection("Controller"); window.UpdateLayout();
            var monitor = panel.Children.OfType<GamepadMonitor>().Single();
            Pad(rt: 1); monitor.Refresh();
            GamepadChecks.Check(monitor.Status.Contains("Xbox Bluetooth compatibility"), "live controller test identifies hardware mapping");
            if (shots != null)
            {
                Dispatcher.UIThread.RunJobs(); window.UpdateLayout();
                FocusNavigator.Focus(panel.Children.OfType<ChoiceRow>().First());
                window.UpdateLayout(); Dispatcher.UIThread.RunJobs();
                // Flush the headless compositor after scrolling and deferred row updates.
                AvaloniaHeadlessPlatform.ForceRenderTimerTick(3);
                Dispatcher.UIThread.RunJobs();
                using var bitmap = window.CaptureRenderedFrame();
                bitmap?.Save(Path.Combine(shots, "controller-live-test.png"));
            }
            GamepadOptions.BindingModifier = GamepadButtons.LeftBumper;
            settings.ShowSection(OperatingSystem.IsAndroid() ? "Touch and mouse" : "Mouse and stylus"); window.UpdateLayout();
            var imperialistKey = settings.GetVisualDescendants().OfType<KeyRow>().First(r => r.BindingName == "Imperialist");
            var weaponProperty = InputSettings.Bindings.First(p => p.Name == "Imperialist");
            string weaponKeyBefore = InputSettings.Describe(InputSettings.Bind(weaponProperty));
            FocusNavigator.Focus(imperialistKey); FocusNavigator.Key(imperialistKey, Avalonia.Input.Key.Enter);
            Pad(); navigation.Update(settings); Pad(GamepadButtons.X); navigation.Update(settings);
            GamepadChecks.Check(PadBindings.Slot(PadAction.Imperialist, 0) == GamepadButtons.X
                && PadBindings.Modifier(PadAction.Imperialist, 0) == GamepadButtons.LeftBumper
                && !GamepadContexts.Capturing, "keyboard weapon row captures a modifier binding directly");
            GamepadChecks.Check(InputSettings.Describe(InputSettings.Bind(weaponProperty)) == weaponKeyBefore,
                "weapon controller capture preserves keyboard weapon key");
            panel.Reload(); window.UpdateLayout(); Dispatcher.UIThread.RunJobs();
            var wheelRow = panel.Children.OfType<ChoiceRow>().First(r => r.Value == "Volt Driver");
            FocusNavigator.Focus(wheelRow); FocusNavigator.Key(wheelRow, Avalonia.Input.Key.Right);
            Dispatcher.UIThread.RunJobs(); window.UpdateLayout();
            GamepadChecks.Check(FocusNavigator.Focused(panel) is ChoiceRow wheel && wheel.Value == "Battlehammer"
                && GamepadOptions.WheelOrder[0] == 1 && GamepadOptions.WheelOrder[1] == 0,
                "wheel reorder retains focus and swaps its displayed weapon");
            GamepadChecks.Check(panel.Children.OfType<GamepadSetupPanel>().Count() == 1
                && panel.Children.OfType<GamepadProfilePanel>().Count() == 1,
                "controller setup and profile tools are reachable in settings");
            if (shots != null)
            {
                var imperialistPad = settings.GetVisualDescendants().OfType<PadRow>().First(r => r.Action == PadAction.Imperialist);
                FocusNavigator.Focus(imperialistPad); window.UpdateLayout(); Dispatcher.UIThread.RunJobs();
                AvaloniaHeadlessPlatform.ForceRenderTimerTick(3); Dispatcher.UIThread.RunJobs();
                using var bitmap = window.CaptureRenderedFrame();
                bitmap?.Save(Path.Combine(shots, "controller-weapons.png"));
                FocusNavigator.Focus(panel.GetVisualDescendants().OfType<UiWord>().First(w => w.Text == "Save current as named profile"));
                window.UpdateLayout(); Dispatcher.UIThread.RunJobs(); AvaloniaHeadlessPlatform.ForceRenderTimerTick(3);
                Dispatcher.UIThread.RunJobs(); using var profiles = window.CaptureRenderedFrame();
                profiles?.Save(Path.Combine(shots, "controller-profiles.png"));
            }
            GamepadManager.RemoveDevice("settings-xbox"); PadBindings.Reset(); GamepadOptions.Reset();
        }
    }
}
#endif
