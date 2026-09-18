using System;
using System.Reflection;
using MphRead.Entities;
using MphRead.Mods.Network;
using OpenTK.Mathematics;

namespace MphRead.Mods.Input.AimAssist
{
    // Optional asset-backed check of the real camera/rig/collision adapter, not a mock LOS flag.
    internal static class AimAssistWorldChecks
    {
        public static int Run(string room)
        {
            if (!ServerSim.Available(out string reason)) { Console.Error.WriteLine(reason); return 1; }
            var sim = new ServerSim();
            if (!sim.Start(room, GameMode.Battle, 2, _ => { }, () => { })) return 1;
            int checks = 0;
            void Check(bool condition, string message)
            { if (!condition) throw new InvalidOperationException(message); checks++; Console.WriteLine("AIM WORLD PASS " + message); }
            try
            {
                NetSession.ApplyMatchState(new MatchStatePacket { MatchId = 1, AuthorityEpoch = 1,
                    RoomKey = room, Mode = (byte)GameMode.Battle }, false);
                var roster = RosterPacket.Create();
                roster.MatchId = 1; roster.AuthorityEpoch = 1; roster.Revision = 1; roster.Count = 2;
                for (byte i = 0; i < 2; i++) { roster.Slots[i] = i; roster.Generations[i] = 1; roster.Names[i] = "AIM" + i; }
                NetSession.ApplyRoster(roster);
                for (int i = 0; i < 120; i++) sim.Step();
                var shooter = PlayerEntity.Players[0]; var target = PlayerEntity.Players[1];
                PlayerEntity.MainPlayerIndex = 0;
                var flags = BindingFlags.Instance | BindingFlags.NonPublic;
                var visible = typeof(PlayerEntity).GetMethod("AssistVisible", flags)!;
                var apply = typeof(PlayerEntity).GetMethod("ApplyControllerAssist", flags)!;
                var gun = typeof(PlayerEntity).GetField("_gunVec1", flags)!;
                Vector3 eye = shooter.Position + new Vector3(0, .8f, 0);
                shooter.CameraInfo.Position = eye;
                Vector3 direction = default;
                foreach (var candidate in new[] { Vector3.UnitZ, -Vector3.UnitZ, Vector3.UnitX, -Vector3.UnitX })
                    if ((bool)visible.Invoke(shooter, new object[] { eye + candidate * 6 })!) { direction = candidate; break; }
                Check(direction != Vector3.Zero, "fixture has a clear six-unit sightline");
                float yaw = MathF.Atan2(direction.X, direction.Z) - MathHelper.DegreesToRadians(.5f);
                gun.SetValue(shooter, new Vector3(MathF.Sin(yaw), 0, MathF.Cos(yaw)));
                var center = PlayerEntity.PlayerVolumes[(int)target.Hunter, 0].SpherePosition;
                var chest = Vector3.Lerp(center, new Vector3(0, Fixed.ToFloat(target.Values.MaxPickupHeight) - .3f, 0), .65f);
                target.Position = eye + direction * 6 - chest;
                shooter.Health = target.Health = 99;
                GamepadContexts.Current = GamepadContext.Gameplay; GamepadContexts.MenuVisible = false; GamepadContexts.Focused = true;
                GamepadManager.UpdateDevice("aim-world-fixture", new() { RightX = -.5f }, true);
                GamepadManager.SelectDevice("aim-world-fixture"); GamepadInput.BeginFrame();
                AimInputSourceTracker.Reset();
                AimAssistResult Apply() => (AimAssistResult)apply.Invoke(shooter, new object[] { .1f, 0f })!;
                Check(Apply().TargetSlot == target.SlotIndex, "real visible hunter rig is acquired");
                target.Health = 0;
                Check(Apply().TargetSlot == -1, "real dead player is rejected");
                target.Health = 99; target.ModSetSpectating(true);
                Check(Apply().TargetSlot == -1, "real spectator is rejected");
                target.ModSetSpectating(false);
                GameState.Teams = true; GameState.TeamCount = 2; shooter.TeamIndex = target.TeamIndex = 0;
                Check(Apply().TargetSlot == -1, "real teammate is rejected");
                GameState.Teams = false;
                float blocked = 8;
                for (; blocked <= 60; blocked += 2)
                    if (!(bool)visible.Invoke(shooter, new object[] { eye + direction * blocked })!) break;
                Check(blocked <= 60, "fixture has an occluder inside assist range");
                target.Position = eye + direction * blocked - chest;
                Check(!(bool)visible.Invoke(shooter, new object[] { target.Position })!, "real room collision blocks line of sight");
                Check(Apply().TargetSlot == -1, "in-range player behind real geometry is rejected");
                Check(sim.StepFailures == 0, "engine steps have no failures");
                Console.WriteLine($"AIM WORLD PASS {checks} assertions"); return 0;
            }
            catch (Exception ex) { Console.Error.WriteLine("AIM WORLD FAIL " + ex); return 1; }
            finally { GamepadManager.RemoveDevice("aim-world-fixture"); sim.Stop(); }
        }
    }
}
