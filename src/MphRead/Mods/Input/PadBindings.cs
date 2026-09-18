using System;
using System.Collections.Generic;

namespace MphRead.Mods.Input
{
    /// <summary>
    /// What a pad button does, as a thing a player can change.
    ///
    /// Not the <c>PlayerControls</c> properties themselves, and the difference
    /// is deliberate: several of those are one button on a pad and always were
    /// on the DS. <see cref="Shoot"/> drives both <c>Shoot</c> and
    /// <c>AltAttack</c>, <see cref="Jump"/> drives both <c>Jump</c> and
    /// <c>Boost</c>, and offering four rows for two buttons would let a player
    /// build a pad on which the ball cannot boost and nothing on screen says
    /// why.
    ///
    /// The weapon wheel shares the existing gameplay bind and slot resolver.
    /// </summary>
    public enum PadAction
    {
        /// <summary>The gun on foot and the alt form's attack in the ball.</summary>
        Shoot,
        Zoom,
        /// <summary>Jumping on foot, boosting in the ball: one button, as on the DS.</summary>
        Jump,
        Morph,
        Scan,
        ScanVisor,
        /// <summary>The DS's own pause button: map and status, scoreboard in a match.</summary>
        Scoreboard,
        NextWeapon,
        PrevWeapon,
        Missile,
        PowerBeam,
        /// <summary>
        /// The app's own menu. Not a <c>Keybind</c> like the rest -- it opens a
        /// window rather than doing something in the world -- so it is taken
        /// by whoever owns the window, through
        /// <see cref="GamepadInput.TakeMenuPress"/>.
        /// </summary>
        Menu,
        /// <summary>
        /// Open the chat line. Like <see cref="Menu"/> and unlike the rest,
        /// this opens something that then takes the keyboard, so it is taken
        /// through <see cref="GamepadInput.TakeChatPress"/> rather than held
        /// as a bind.
        /// </summary>
        Chat,
        WeaponWheel,
        VoltDriver, Battlehammer, Imperialist, Judicator, Magmaul, ShockCoil,
        OmegaCannon, AffinitySlot, LastWeapon
    }

    /// <summary>
    /// Which pad button each action is on, and the player's changes to that.
    ///
    /// Its own table rather than an extra <c>ButtonType</c> on
    /// <see cref="Entities.Keybind"/>, because that type is upstream's and
    /// everything this project adds stays under <c>Mods/</c> so a pull from
    /// NoneGiven/MphRead is a fast-forward. It costs nothing to keep them
    /// apart: <see cref="GamepadInput.Apply"/> already *adds* the pad's
    /// contribution to the binds the keyboard just filled in, so the two
    /// mappings never have to agree about anything.
    ///
    /// A binding may be more than one button -- the defaults put the weapon
    /// cycling on a bumper *and* the d-pad -- which is why this is a flag set
    /// and not a single value. Primary and secondary slots are saved alongside
    /// the legacy flag set so older controls files continue to load.
    /// </summary>
    public static class PadBindings
    {
        private static readonly GamepadButtons[] _defaults =
        {
            /* Shoot      */ GamepadButtons.RightTrigger,
            /* Zoom       */ GamepadButtons.LeftTrigger,
            /* Jump       */ GamepadButtons.A,
            /* Morph      */ GamepadButtons.B,
            /* Scan       */ GamepadButtons.X,
            /* ScanVisor  */ GamepadButtons.Y,
            /* Scoreboard */ GamepadButtons.Back,
            /* NextWeapon */ GamepadButtons.RightBumper | GamepadButtons.DpadRight,
            /* PrevWeapon */ GamepadButtons.LeftBumper | GamepadButtons.DpadLeft,
            /* Missile    */ GamepadButtons.DpadUp,
            /* PowerBeam  */ GamepadButtons.DpadDown,
            /* Menu       */ GamepadButtons.Start,
            // The left stick click, which is the only button on a standard pad
            // the defaults had not already spent. Awkward to hit by accident
            // while aiming, which is what you want from the one that stops you
            // playing and starts you typing.
            /* Chat       */ GamepadButtons.LeftThumb,
            /* WeaponWheel */ GamepadButtons.RightThumb,
            /* Direct weapons and last weapon are opt-in. */
            0, 0, 0, 0, 0, 0, 0, 0, 0
        };

        public static string Preset { get; internal set; } = "Default";
        public static long Revision { get; private set; }

        private static readonly GamepadButtons[] _current = (GamepadButtons[])_defaults.Clone();
        private static readonly GamepadButtons[] Primary = new GamepadButtons[_defaults.Length];
        private static readonly GamepadButtons[] Secondary = new GamepadButtons[_defaults.Length];
        private static readonly GamepadButtons[,] Modifiers = new GamepadButtons[_defaults.Length, 2];
        static PadBindings() { Reset(); }

        /// <summary>Every action, in the order a settings screen should list them.</summary>
        public static IReadOnlyList<PadAction> Actions => ActionOrder;
        private static readonly PadAction[] ActionOrder = new[]
        {
            PadAction.Shoot, PadAction.Jump, PadAction.Morph, PadAction.Zoom,
            PadAction.ScanVisor, PadAction.Scan, PadAction.NextWeapon,
            PadAction.PrevWeapon, PadAction.Missile, PadAction.PowerBeam,
            PadAction.Scoreboard, PadAction.Menu, PadAction.Chat, PadAction.WeaponWheel,
            PadAction.VoltDriver, PadAction.Battlehammer, PadAction.Imperialist, PadAction.Judicator,
            PadAction.Magmaul, PadAction.ShockCoil, PadAction.OmegaCannon, PadAction.AffinitySlot, PadAction.LastWeapon
        };

        public static GamepadButtons Get(PadAction action)
        {
            return _current[(int)action];
        }

        public static void Set(PadAction action, GamepadButtons buttons)
        {
            _current[(int)action] = buttons;
            int index = (int)action;
            Primary[index] = Secondary[index] = 0;
            Modifiers[index, 0] = Modifiers[index, 1] = 0;
            foreach (GamepadButtons button in Enum.GetValues<GamepadButtons>())
                if (button != 0 && (buttons & button) != 0)
                {
                    if (Primary[index] == 0) Primary[index] = button;
                    else if (Secondary[index] == 0) Secondary[index] = button;
                }
            Preset = "Custom";
            Revision++;
        }

        public static GamepadButtons Default(PadAction action)
        {
            return _defaults[(int)action];
        }

        public static GamepadButtons Slot(PadAction action, int slot)
            => slot == 0 ? Primary[(int)action] : Secondary[(int)action];
        public static void SetSlot(PadAction action, int slot, GamepadButtons button, GamepadButtons modifier = 0)
        {
            int index = (int)action;
            if (!Single(button) || !Single(modifier) || (button != 0 && button == modifier))
                throw new ArgumentException("A binding needs distinct single buttons.");
            Modifiers[index, slot] = button == 0 ? 0 : modifier;
            GamepadButtons old = Slot(action, slot);
            if (slot == 0) Primary[index] = button; else Secondary[index] = button;
            _current[index] = (_current[index] & ~old) | Primary[index] | Secondary[index];
            Preset = "Custom";
            Revision++;
        }
        public static bool Single(GamepadButtons button)
            => ((int)button & ~0xffff) == 0 && ((int)button & ((int)button - 1)) == 0;
        public static GamepadButtons Modifier(PadAction action, int slot) => Modifiers[(int)action, slot];
        public static string DescribeSlot(PadAction action, int slot)
            => (Modifier(action, slot) == 0 ? "" : ButtonName(Modifier(action, slot)) + " + ") + Describe(Slot(action, slot));
        public static ulong Evaluate(GamepadButtons buttons, GamepadButtons suppressed = 0)
        {
            GamepadButtons modifiers = 0, used = 0;
            ulong result = 0;
            foreach (var action in ActionOrder) for (int slot = 0; slot < 2; slot++)
            {
                var modifier = Modifier(action, slot); var button = Slot(action, slot);
                modifiers |= modifier;
                if (modifier != 0 && button != 0 && (buttons & (modifier | button)) == (modifier | button))
                { result |= 1UL << (int)action; used |= modifier | button; }
            }
            foreach (var action in ActionOrder)
            {
                var available = buttons & ~(modifiers | used | suppressed);
                // Retain additional alternatives from legacy flag-set bindings.
                var plain = Get(action) & ~(Slot(action, 0) | Slot(action, 1));
                for (int slot = 0; slot < 2; slot++) if (Modifier(action, slot) == 0) plain |= Slot(action, slot);
                if ((available & plain) != 0) result |= 1UL << (int)action;
            }
            return result;
        }
        public static GamepadButtons ChordButtons(GamepadButtons buttons)
        {
            GamepadButtons used = 0;
            foreach (var action in ActionOrder) for (int slot = 0; slot < 2; slot++)
            {
                var modifier = Modifier(action, slot); var chord = modifier | Slot(action, slot);
                if (modifier != 0 && (buttons & chord) == chord) used |= chord;
            }
            return used;
        }
        public static void Write(List<string> lines)
        {
            foreach (var action in ActionOrder)
            {
                string key = SettingKey(action);
                lines.Add($"{key}={Get(action)}");
                for (int slot = 0; slot < 2; slot++)
                {
                    string suffix = slot == 0 ? "primary" : "secondary";
                    lines.Add($"{key}_{suffix}={Slot(action, slot)}");
                    lines.Add($"{key}_{suffix}_modifier={Modifier(action, slot)}");
                }
            }
        }
        public static void LoadSlots(IEnumerable<string> lines)
        {
            foreach (string line in lines)
            {
                int split = line.IndexOf('=');
                if (split < 0) continue;
                string key = line[..split].Trim(), value = line[(split + 1)..].Trim();
                int slot = key.EndsWith("_primary", StringComparison.Ordinal) ? 0 : 1;
                string suffix = slot == 0 ? "_primary" : "_secondary";
                if (!key.StartsWith("pad_", StringComparison.Ordinal) || !key.EndsWith(suffix, StringComparison.Ordinal)) continue;
                if (Enum.TryParse<PadAction>(key[4..^suffix.Length], out var action) && Enum.IsDefined(action)
                    && Enum.TryParse<GamepadButtons>(value, out var button) && ((int)button & ~0xffff) == 0
                    && ((int)button & ((int)button - 1)) == 0) SetSlot(action, slot, button);
            }
            foreach (string line in lines)
            {
                int split = line.IndexOf('=');
                if (split < 0) continue;
                string key = line[..split].Trim(), value = line[(split + 1)..].Trim();
                foreach (var action in ActionOrder) for (int slot = 0; slot < 2; slot++)
                    if (key == SettingKey(action) + (slot == 0 ? "_primary_modifier" : "_secondary_modifier")
                        && Enum.TryParse<GamepadButtons>(value, out var modifier) && Single(modifier)
                        && modifier != Slot(action, slot)) SetSlot(action, slot, Slot(action, slot), modifier);
            }
        }

        public static IReadOnlyList<PadAction> Conflicts(PadAction action, GamepadButtons button, GamepadButtons modifier = 0)
        {
            var result = new List<PadAction>();
            if (button != 0) foreach (var other in Actions)
                if (other != action && ((Slot(other, 0) == button && Modifier(other, 0) == modifier)
                    || (Slot(other, 1) == button && Modifier(other, 1) == modifier)
                    || (modifier == 0 && (Get(other) & ~(Slot(other, 0) | Slot(other, 1)) & button) != 0))) result.Add(other);
            return result;
        }
        public static void Assign(PadAction action, int slot, GamepadButtons button, string resolution, GamepadButtons modifier = 0)
        {
            if (resolution == "Cancel") return;
            var old = Slot(action, slot);
            var oldModifier = Modifier(action, slot);
            if (resolution == "Swap" || resolution == "Replace")
                foreach (var other in Conflicts(action, button, modifier))
                {
                    int index = (int)other;
                    var replacement = resolution == "Swap" ? old : GamepadButtons.None;
                    for (int otherSlot = 0; otherSlot < 2; otherSlot++)
                        if (Slot(other, otherSlot) == button && Modifier(other, otherSlot) == modifier)
                            SetSlot(other, otherSlot, replacement, resolution == "Swap" ? oldModifier : 0);
                    _current[index] = (Get(other) & ~button) | replacement | Primary[index] | Secondary[index];
                    Revision++;
                }
            SetSlot(action, slot, button, modifier);
        }
        public static void ApplyPreset(string name)
        {
            if (name == "Custom") { Preset = name; return; }
            Reset();
            GamepadOptions.Southpaw = name == "Southpaw";
            if (name == "Bumper Jumper")
            {
                Set(PadAction.Jump, GamepadButtons.LeftBumper);
                Set(PadAction.PrevWeapon, GamepadButtons.A | GamepadButtons.DpadLeft);
            }
            if (name == "Classic")
            {
                Set(PadAction.WeaponWheel, GamepadButtons.None);
                Set(PadAction.Chat, GamepadButtons.LeftThumb);
            }
            Preset = name;
        }

        /// <summary>Put every button back where it shipped.</summary>
        public static void Reset()
        {
            foreach (PadAction action in Actions) Set(action, _defaults[(int)action]);
            Preset = "Default";
        }

        /// <summary>The name of the setting a row is editing.</summary>
        public static string Name(PadAction action)
        {
            return action switch
            {
                PadAction.Shoot => "Fire / alt attack",
                PadAction.Zoom => "Zoom",
                PadAction.Jump => "Jump / boost",
                PadAction.Morph => "Morph ball",
                PadAction.Scan => "Scan",
                PadAction.ScanVisor => "Scan visor",
                PadAction.Scoreboard => "Map / scoreboard",
                PadAction.NextWeapon => "Next weapon",
                PadAction.PrevWeapon => "Cycle previous weapon",
                PadAction.LastWeapon => "Last equipped weapon",
                PadAction.VoltDriver => "Volt Driver",
                PadAction.ShockCoil => "Shock Coil",
                PadAction.OmegaCannon => "Omega Cannon",
                PadAction.AffinitySlot => "Affinity weapon slot",
                PadAction.Missile => "Missile",
                PadAction.PowerBeam => "Power beam",
                PadAction.Menu => "Menu",
                PadAction.WeaponWheel => "Weapon wheel",
                _ => action.ToString()
            };
        }

        /// <summary>"A", "RT", "RB or D-pad right", "unbound".</summary>
        public static string Describe(GamepadButtons buttons)
        {
            if (buttons == GamepadButtons.None)
            {
                return "unbound";
            }
            var names = new List<string>();
            foreach (GamepadButtons button in Enum.GetValues<GamepadButtons>())
            {
                if (button != GamepadButtons.None && (buttons & button) == button)
                {
                    names.Add(ButtonName(button));
                }
            }
            return String.Join(" or ", names);
        }

        /// <summary>
        /// What the button is called on the pad in the player's hands.
        ///
        /// Family-specific labels keep normalized physical positions unchanged.
        /// </summary>
        public static string ButtonName(GamepadButtons button)
        {
            return GamepadGlyphs.Resolve(button);
        }

        /// <summary>The key this action is written under in controls.txt.</summary>
        public static string SettingKey(PadAction action)
        {
            return "pad_" + action;
        }

        /// <summary>
        /// Read one saved line. The value is the flag set's own round trip --
        /// "RightBumper, DpadRight" -- so a binding with two buttons in it
        /// survives being written out and read back.
        /// </summary>
        public static bool TryLoad(string key, string value)
        {
            if (!key.StartsWith("pad_", StringComparison.Ordinal)
                || !Enum.TryParse(key[4..], out PadAction action)
                || !Enum.IsDefined(action)
                || !Enum.TryParse(value, out GamepadButtons buttons)
                || ((int)buttons & ~0xffff) != 0)
            {
                return false;
            }
            Set(action, buttons);
            return true;
        }
    }
}
