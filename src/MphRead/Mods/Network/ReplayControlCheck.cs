using System;
using MphRead.Mods.Input;

namespace MphRead.Mods.Network
{
    internal static class ReplayControlCheck
    {
        public static int Run()
        {
            try
            {
                CheckGamepadContext();
                foreach (float rate in ReplayController.Rates)
                {
                    ReplayController.Begin();
                    ReplayController.SetPlaybackRate(rate);
                    int frames = 0;
                    for (int i = 0; i < 240; i++) frames += ReplayController.FramesDue();
                    Require(frames == 240 * rate, $"{rate}x: expected {240 * rate}, got {frames}");
                    ReplayController.Pause();
                    for (int i = 0; i < 60; i++) Require(ReplayController.FramesDue() == 0, "pause advanced");
                    ReplayController.StepForward();
                    Require(ReplayController.FramesDue() == 1, "step did not advance exactly once");
                    Require(ReplayController.FramesDue() == 0, "step repeated");
                    ReplayController.Play();
                    int resumed = 0;
                    for (int i = 0; i < 240; i++) resumed += ReplayController.FramesDue();
                    Require(resumed == 240 * rate, "resume changed rate");
                }
                // Different presentation rates all owe 60 fixed intervals per second.
                foreach (int fps in new[] { 30, 60, 120, 144, 240 })
                {
                    ReplayController.Begin();
                    ReplayController.SetPlaybackRate(4);
                    double accumulator = 0;
                    int frames = 0;
                    for (int draw = 0; draw < fps * 4; draw++)
                    {
                        accumulator += 60.0 / fps;
                        while (accumulator + 1e-9 >= 1)
                        { accumulator -= 1; frames += ReplayController.FramesDue(); }
                    }
                    Require(frames == 960, $"display rate {fps} changed replay timing");
                }
                Console.WriteLine("[replaycheck] timing: all rates, pause, step, resume and presentation independence passed");
                Replay.ReplayCameraTrackCheck.Run();
                return 0;
            }
            catch (Exception ex) { Console.WriteLine($"[replaycheck] FAIL: {ex.Message}"); return 1; }
            finally { ReplayController.Stop(); }
        }
        private static void CheckGamepadContext()
        {
            var previousContext = GamepadContexts.Current;
            bool previousMenu = GamepadContexts.MenuVisible;
            try
            {
                // A prior paused game leaves Current at Menu, even after its UI closes.
                GamepadContexts.Current = GamepadContext.Menu;
                GamepadContexts.MenuVisible = false;
                GamepadManager.UpdateDevice("replay-context-check", new GamepadState { Connected = true }, mapped: true);
                Replay.ReplayInput.BeginFrame();
                Require(GamepadContexts.Current == GamepadContext.Gameplay, "replay retained previous menu context");
                GamepadManager.UpdateDevice("replay-context-check", new GamepadState
                    { Connected = true, Buttons = GamepadButtons.Start, RightX = 1 }, mapped: true);
                Replay.ReplayInput.BeginFrame();
                Require(GamepadInput.AimDeltaX != 0 && GamepadInput.TakeMenuPress(), "replay lost controller look/menu");
                GamepadContexts.MenuVisible = true;
                Replay.ReplayInput.BeginFrame();
                Require(GamepadContexts.Current == GamepadContext.Menu && GamepadInput.AimDeltaX == 0,
                    "replay menu did not suppress controller look");
                GamepadContexts.MenuVisible = false;
                Replay.ReplayInput.BeginFrame();
                Require(!GamepadInput.TakeMenuPress(), "held Start reopened menu on replay resume");
                GamepadManager.UpdateDevice("replay-context-check", new GamepadState { Connected = true }, mapped: true);
                Replay.ReplayInput.BeginFrame();
                GamepadManager.UpdateDevice("replay-context-check", new GamepadState
                    { Connected = true, Buttons = GamepadButtons.Start }, mapped: true);
                Replay.ReplayInput.BeginFrame();
                Require(GamepadInput.TakeMenuPress(), "fresh Start after release lost on replay resume");
            }
            finally
            {
                GamepadManager.RemoveDevice("replay-context-check");
                GamepadContexts.MenuVisible = previousMenu;
                GamepadContexts.Current = previousContext;
                GamepadInput.BeginFrame();
            }
        }

        private static void Require(bool condition, string message)
        { if (!condition) throw new InvalidOperationException(message); }
    }
}
