using System;
using MphRead.Entities;
using MphRead.Formats;
using MphRead.Mods.Input;
using MphRead.Mods.Network;
using OpenTK.Mathematics;
using static MphRead.Mods.Network.NetPlayerBridge;

namespace MphRead.Testing
{
    public static partial class TestPlayer
    {
        // Standalone deterministic checks: no room, models, GL context or test framework.
        public static int CheckAltForms()
        {
            int failures = 0;
            int checks = 0;
            void Check(bool passed, string name)
            {
                checks++;
                if (!passed)
                {
                    failures++;
                    Console.WriteLine($"FAIL: {name}");
                }
            }
            CheckCamera(Check);
            CheckReconciliation(Check);
            PlayerEntity.ModCheckAltFlickRouting(Check);
            PlayerEntity.ModCheckAltFlickInputOrder(Check);
            Reset();
            Console.WriteLine($"ALTFORMCHECK: {checks} checks, {failures} failures");
            return failures == 0 ? 0 : 1;
        }

        private static void CheckCamera(Action<bool, string> check)
        {
            var camera = new CameraInfo();
            camera.Reset();
            Vector4 Basis() => new Vector4(camera.Field48, camera.Field4C, camera.Field50, camera.Field54);
            bool Orthonormal()
            {
                Vector4 b = Basis();
                return Single.IsFinite(b.X) && Single.IsFinite(b.Y)
                    && Single.IsFinite(b.Z) && Single.IsFinite(b.W)
                    && MathF.Abs(b.X * b.X + b.Y * b.Y - 1) < 0.00001f
                    && MathF.Abs(b.Z * b.Z + b.W * b.W - 1) < 0.00001f
                    && MathF.Abs(b.X * b.Z + b.Y * b.W) < 0.00001f;
            }
            check(Basis() == new Vector4(0, -1, -1, 0), "reset basis before first update");
            camera.Update();
            check(Basis() == new Vector4(0, -1, -1, 0), "valid -Z basis");
            camera.Position = Vector3.Zero;
            camera.Target = new Vector3(3, 2, 4);
            camera.Update();
            check(Orthonormal(), "arbitrary basis orthonormal");
            Vector4 previous = Basis();
            Vector3[] invalid =
            {
                Vector3.Zero, Vector3.UnitY, -Vector3.UnitY,
                new Vector3(1f / 16384, 1, 1f / 16384),
                new Vector3(1f / 4096, 1, 0),
                new Vector3(Single.NaN, 1, 1), new Vector3(1, 1, Single.NaN),
                new Vector3(Single.PositiveInfinity, 1, 1),
                new Vector3(1, 1, Single.NegativeInfinity),
                new Vector3(Single.MaxValue, 1, Single.MaxValue)
            };
            foreach (Vector3 target in invalid)
            {
                camera.Target = target;
                camera.Update();
                check(Basis() == previous && Orthonormal(), $"preserve basis at {target}");
            }
            camera.Target = new Vector3(-4, 2, 3);
            camera.Update();
            check(Basis() != previous && Orthonormal(), "recover after invalid direction");
            camera.Reset();
            check(Basis() == new Vector4(0, -1, -1, 0), "reset replaces previous basis");
        }

        private static void CheckReconciliation(Action<bool, string> check)
        {
            const int slot = 2;
            FormCorrection At(uint frame, bool current, bool wanted, bool morphing = false, bool unmorphing = false)
                => ReconcileForm(slot, frame, wanted, current, morphing, unmorphing, 300);
            check(ReconcileForm(-1, 0, true, false, false, false, 0) == FormCorrection.None
                && ReconcileForm(PlayerEntity.SlotCapacity, 0, true, false, false, false, 0) == FormCorrection.None,
                "invalid slots ignored");
            foreach (bool target in new[] { false, true })
            {
                Reset();
                for (uint frame = 0; frame <= 20; frame++)
                    check(At(frame, !target, target) == (frame == 8 ? FormCorrection.Start
                        : frame == 20 ? FormCorrection.Force : FormCorrection.None), "bounded lost-press recovery");
                Reset();
                for (uint frame = 0; frame <= 90; frame++)
                    check(At(frame, false, target, morphing: target, unmorphing: !target)
                        == (frame == 90 ? FormCorrection.Force : FormCorrection.None),
                        "stalled transition completes within ninety frames");
                Reset();
                for (uint frame = 0; frame < 60; frame++)
                {
                    bool active = frame < 20;
                    bool actual = target && !active;
                    bool desired = frame >= 38 ? target : !target;
                    check(At(frame, actual, desired, active && target, active && !target) == FormCorrection.None,
                        "normal animation and stale snapshots receive latency grace");
                }
            }
            foreach (Action reset in new Action[] { Reset, NoteRoomChanged, () => ForgetSlot(slot) })
            {
                Reset();
                for (uint frame = 0; frame <= 8; frame++) At(frame, false, true);
                reset();
                for (uint frame = 9; frame < 17; frame++)
                    check(At(frame, false, true) == FormCorrection.None, "lifecycle clears recovery history");
                check(At(17, false, true) == FormCorrection.Start, "recovery restarts after lifecycle reset");
            }
            Reset();
            for (int repeat = 0; repeat < 100; repeat++)
                check(At(0, false, true) == FormCorrection.None, "repeated packets cannot advance recovery");
            At(1, false, true);
            long before = GC.GetAllocatedBytesForCurrentThread();
            for (uint frame = 2; frame < 10002; frame++) At(frame, false, true, true);
            check(GC.GetAllocatedBytesForCurrentThread() == before, "allocation-free reconciliation");
        }
    }
}

namespace MphRead.Entities
{
    public partial class PlayerEntity
    {
        internal static void ModCheckAltFlickInputOrder(Action<bool, string> check)
        {
            var scene = (Scene)System.Runtime.CompilerServices.RuntimeHelpers.GetUninitializedObject(typeof(Scene));
            typeof(Scene).GetField("_movieFrameIndex", System.Reflection.BindingFlags.Instance
                | System.Reflection.BindingFlags.NonPublic)!.SetValue(scene, -1);
            Reset();
            Construct(scene);
            var player = Main;
            player.LoadFlags = LoadFlags.Active;
            player._health = 99;
            player.Flags1 = PlayerFlags1.AltForm;
            player._abilities = AbilityFlags.SpireAltAttack;
            var keyboard = SyntheticInput.CreateKeyboard();
            var mouse = SyntheticInput.CreateMouse();
            try
            {
                NetPlayerBridge.Reset();
                player.AltFlickRequested = true; // Android delivers the swipe before the input pass.
                ProcessInput(keyboard, mouse, false);
                NetPlayerBridge.RecordPresses(player); // NetHooks.AfterInput runs BEFORE ProcessAlt.
                var intent = NetPlayerBridge.CaptureIntent(player);
                check(((IntentButtons)intent.Presses![0] & IntentButtons.AltAttack) != 0,
                    "production input order records Spire flick before simulation");
                check(player.Controls.AltAttack.IsPressed, "Spire local attack is ready before simulation");
                ProcessInput(keyboard, mouse, false);
                NetPlayerBridge.RecordPresses(player);
                intent = NetPlayerBridge.CaptureIntent(player);
                check(((IntentButtons)intent.Presses![0] & IntentButtons.AltAttack) == 0,
                    "next input pass does not repeat Spire flick");
            }
            finally { Reset(); NetPlayerBridge.Reset(); MouseFlick.Reset(); }
        }

        // Unloaded players exercise the shared gesture and real wire/press paths.
        // No game assets are needed until an attack animation is actually played.
        internal static void ModCheckAltFlickRouting(Action<bool, string> check)
        {
            var player = new PlayerEntity(1, null!);
            player._health = 99;
            player.Flags1 = PlayerFlags1.AltForm;
            player._abilities = AbilityFlags.SpireAltAttack;
            player.Controls.Boost.IsDown = true;
            player.AltFlickRequested = true;
            player.AltFlickX = 0.6f;
            player.AltFlickY = -0.8f;
            player.ModPrepareSpireFlick();
            check(player.Controls.AltAttack.IsPressed && !player.Controls.AltAttack.IsDown
                && player.Input.HasInput, "Spire emits canonical one-shot despite Boost bind");
            check(!player.AltFlickRequested && player.AltFlickX == 0 && player.AltFlickY == 0,
                "gesture state cleared together");

            NetPlayerBridge.Reset();
            NetPlayerBridge.RecordPresses(player);
            IntentPacket intent = NetPlayerBridge.CaptureIntent(player);
            check(((IntentButtons)intent.Presses![0] & IntentButtons.AltAttack) != 0,
                "Spire flick recorded by existing network press history");
            // Avoid weapon/model setup in this unloaded-player check.
            intent.WeaponSelect = 0xFF;
            intent.Aim = Vector3.UnitZ;
            intent.Frame = 11;
            intent.SlotGeneration = 1;
            intent.LifeId = 1;
            byte[] wire = new byte[IntentPacket.FullSize];
            intent.Write(wire);
            IntentPacket received = IntentPacket.Read(wire);
            var authority = new PlayerEntity(2, null!);
            var viewer = new PlayerEntity(3, null!);
            foreach (var receiver in new[] { authority, viewer })
            {
                NetPlayerLifecycle.SetOccupant(receiver.SlotIndex, 1);
                NetPlayerLifecycle.AcceptState(new PlayerState { SlotIndex = (byte)receiver.SlotIndex,
                    SlotGeneration = 1, LifeId = 1, Health = 99,
                    Flags = PlayerState.FlagActive | PlayerState.FlagSpawned }, 1);
            }
            IntentPacket baseline = received;
            baseline.Frame = 10;
            baseline.Presses = new uint[IntentPacket.PressHistory];
            NetPlayerBridge.ApplyIntent(authority, baseline);
            NetPlayerBridge.ApplyIntent(viewer, baseline);
            NetPlayerBridge.ApplyIntent(authority, received);
            NetPlayerBridge.ApplyIntent(viewer, received);
            check(authority.Controls.AltAttack.IsPressed && viewer.Controls.AltAttack.IsPressed,
                "serialized flick edge reaches authority and viewer controls");
            NetPlayerBridge.ApplyIntent(authority, received);
            NetPlayerBridge.ApplyIntent(viewer, received);
            check(!authority.Controls.AltAttack.IsPressed && !viewer.Controls.AltAttack.IsPressed,
                "repeated history does not replay attack");

            player.Controls.AltAttack.IsPressed = false;
            check(!player.ModConsumeAltFlick(out _, out _) && !player.Controls.AltAttack.IsPressed,
                "one gesture cannot fire twice");
            player._abilities = AbilityFlags.Boost;
            player.AltFlickRequested = true;
            player.AltFlickX = -1;
            check(player.ModConsumeAltFlick(out float x, out float y) && x == -1 && y == 0
                && !player.Controls.AltAttack.IsPressed, "Samus flick retains aimed boost event");
            foreach (AbilityFlags ability in new[] { AbilityFlags.Bombs, AbilityFlags.NoxusAltAttack,
                AbilityFlags.TraceAltAttack, AbilityFlags.WeavelAltAttack })
            {
                player._abilities = ability;
                player.AltFlickRequested = true;
                check(!player.ModConsumeAltFlick(out _, out _) && !player.Controls.AltAttack.IsPressed
                    && !player.AltFlickRequested, "unsupported hunter discards flick");
            }
            player._abilities = AbilityFlags.SpireAltAttack;
            foreach (PlayerFlags1 flags in new[] { PlayerFlags1.None, PlayerFlags1.Morphing,
                PlayerFlags1.AltForm | PlayerFlags1.Morphing, PlayerFlags1.AltForm | PlayerFlags1.Unmorphing })
            {
                player.Flags1 = flags;
                player.AltFlickRequested = true;
                check(!player.ModConsumeAltFlick(out _, out _) && !player.AltFlickRequested,
                    "biped/transition discards flick");
            }
            player.Flags1 = PlayerFlags1.AltForm;
            player._health = 0;
            player.AltFlickRequested = true;
            check(!player.ModConsumeAltFlick(out _, out _), "dead player discards flick");
            player._health = 99;
            player._frozenTimer = 10;
            player.AltFlickRequested = true;
            check(!player.ModConsumeAltFlick(out _, out _), "frozen player discards flick");
            player._frozenTimer = 0;
            check(!player.ModConsumeAltFlick(out _, out _), "discarded flick cannot fire after thaw");

            MouseFlick.Reset();
            float delta = 600 / Mods.InputSettings.MouseSensitivity;
            MouseFlick.Check(0, 0, 1000, out _, out _);
            check(MouseFlick.Check(delta, 0, 1001, out x, out y) && x == 1 && y == 0,
                "desktop horizontal flick normalized");
            bool repeated = false;
            for (ulong frame = 1002; frame < 1050; frame++)
                repeated |= MouseFlick.Check(delta, 0, frame, out _, out _);
            check(!repeated, "continued mouse sweep cannot repeat flick");
            MouseFlick.Check(0, 0, 1050, out _, out _);
            check(MouseFlick.Check(delta, -delta, 1051, out x, out y)
                && x > 0 && y < 0 && MathF.Abs(x * x + y * y - 1) < 0.00001f,
                "desktop diagonal flick keeps normalized diagonal");
            MouseFlick.Reset();
        }
    }
}
