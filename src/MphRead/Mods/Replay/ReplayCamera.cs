using System;
using System.Collections.Generic;
using MphRead.Entities;
using MphRead.Formats;
using MphRead.Mods.Network;
using OpenTK.Mathematics;

namespace MphRead.Mods.Replay
{
    public enum ReplayCameraMode { FirstPerson, Chase, Free, Orbit }
    public static class ReplayCamera
    {
        public static ReplayCameraMode Mode { get; private set; }
        public static float Distance { get; set; } = 4;
        public static float Height { get; set; } = 1.5f;
        public static float FieldOfView { get; set; } = 80;
        public static bool Director { get; set; }
        internal static readonly List<(uint Frame, Vector3 Position, Vector3 Facing, float Fov)> Bookmarks = new();
        internal static int BookmarkIndex;
        internal static void ClearBookmarks() { Bookmarks.Clear(); BookmarkIndex = 0; }
        private static int _eventCursor;
        private static uint _lastFrame;
        internal static bool Changed;
        internal static bool BookmarkRequested;
        internal static bool RestoreRequested;
        public static void SetMode(ReplayCameraMode mode) { Mode = mode; Changed = true; ReplayController.NoteInput(); }
        public static void ToggleFree() => SetMode(Mode == ReplayCameraMode.Free ? ReplayCameraMode.FirstPerson : ReplayCameraMode.Free);
        public static void Bookmark() { BookmarkRequested = true; ReplayController.NoteInput(); }
        public static void RestoreBookmark() { RestoreRequested = true; ReplayController.NoteInput(); }
        internal static void Reset() { Mode = ReplayCameraMode.FirstPerson; Changed = true; _eventCursor = 0; _lastFrame = 0; }
        internal static void TickDirector()
        {
            uint frame = ReplayController.CurrentFrame;
            if (frame < _lastFrame) _eventCursor = 0;
            _lastFrame = frame;
            var events = DemoPlayback.Events;
            while (_eventCursor < events.Count && events[_eventCursor].Frame <= frame)
            {
                ReplayEvent e = events[_eventCursor++];
                if (Director && e.Type is ReplayEventType.Kill or ReplayEventType.Objective)
                    SpectatorMode.Watch(e.ActorSlot);
            }
        }
        public static void WatchEvent(bool victim)
        {
            ReplayEvent? nearest = null;
            foreach (ReplayEvent e in DemoPlayback.Events)
                if (e.Type == ReplayEventType.Kill && e.Frame <= ReplayController.CurrentFrame) nearest = e;
            if (nearest is ReplayEvent selected) SpectatorMode.Watch(victim ? selected.TargetSlot : selected.ActorSlot);
        }
    }
}

namespace MphRead
{
    public partial class Scene
    {
        private long _replayCameraTime;
        private float _replayOrbit;
        private Vector3? _replayFollowPosition;

        private void ModReplayCamera()
        {
            if (!Mods.Network.DemoPlayback.IsActive) return;
            Mods.Replay.ReplayCamera.TickDirector();
            var mode = Mods.Replay.ReplayCamera.Mode;
            if (Mods.Replay.ReplayCamera.Changed)
            {
                SetFreeCamera(mode != Mods.Replay.ReplayCameraMode.FirstPerson);
                Mods.Replay.ReplayCamera.Changed = false;
                _replayFollowPosition = null;
            }
            long now = Environment.TickCount64;
            float delta = _replayCameraTime == 0 ? 0 : Math.Clamp((now - _replayCameraTime) / 1000f, 0, 0.1f);
            _replayCameraTime = now;
            if (Mods.Replay.ReplayCamera.BookmarkRequested)
            {
                Mods.Replay.ReplayCamera.BookmarkRequested = false;
                if (Mods.Replay.ReplayCamera.Bookmarks.Count < 64) Mods.Replay.ReplayCamera.Bookmarks.Add((Mods.Network.ReplayController.CurrentFrame, _cameraPosition, _cameraFacing, _cameraFov));
            }
            if (Mods.Replay.ReplayCamera.RestoreRequested)
            {
                Mods.Replay.ReplayCamera.RestoreRequested = false;
                if (Mods.Replay.ReplayCamera.Bookmarks.Count > 0)
                {
                    var saved = Mods.Replay.ReplayCamera.Bookmarks[Mods.Replay.ReplayCamera.BookmarkIndex++ % Mods.Replay.ReplayCamera.Bookmarks.Count];
                    Mods.Replay.ReplayCamera.SetMode(Mods.Replay.ReplayCameraMode.Free);
                    SetFreeCamera(true);
                    _cameraPosition = saved.Position; _cameraFacing = saved.Facing; _cameraFov = saved.Fov;
                    _cameraRight = Vector3.Cross(_cameraFacing, Vector3.UnitY).Normalized();
                    _cameraUp = Vector3.Cross(_cameraRight, _cameraFacing).Normalized();
                    return;
                }
            }
            if (mode is not (Mods.Replay.ReplayCameraMode.Chase or Mods.Replay.ReplayCameraMode.Orbit)) return;
            var player = PlayerEntity.Main;
            if (!player.LoadFlags.TestFlag(LoadFlags.Spawned)) return;
            SetFreeCamera(true);
            Vector3 target = player.Position + Vector3.UnitY * Math.Clamp(Mods.Replay.ReplayCamera.Height, 0.5f, 8);
            Vector3 facing = player.CameraInfo.Facing;
            facing.Y = 0;
            if (facing.LengthSquared < 0.001f) facing = -Vector3.UnitZ;
            facing.Normalize();
            if (mode == Mods.Replay.ReplayCameraMode.Orbit)
            {
                _replayOrbit += delta * 0.4f;
                facing = new Vector3(MathF.Sin(_replayOrbit), 0, MathF.Cos(_replayOrbit));
            }
            Vector3 desired = target - facing * Math.Clamp(Mods.Replay.ReplayCamera.Distance, 1, 20);
            Vector3 candidate = _replayFollowPosition.HasValue ? Vector3.Lerp(_replayFollowPosition.Value, desired, 1 - MathF.Exp(-delta * 10)) : desired;
            CollisionResult collision = default;
            if (CollisionDetection.CheckBetweenPoints(target, candidate, TestFlags.Players, this, ref collision))
                candidate = target + (candidate - target) * Math.Max(0, collision.Distance - 0.05f);
            _cameraPosition = candidate;
            _replayFollowPosition = candidate;
            Vector3 view = target - candidate;
            if (view.LengthSquared > 0.0001f) _cameraFacing = view.Normalized();
            _cameraRight = Vector3.Cross(_cameraFacing, Vector3.UnitY).Normalized();
            _cameraUp = Vector3.Cross(_cameraRight, _cameraFacing).Normalized();
            _cameraFov = MathHelper.DegreesToRadians(Math.Clamp(Mods.Replay.ReplayCamera.FieldOfView, 40, 120));
        }
    }
}
