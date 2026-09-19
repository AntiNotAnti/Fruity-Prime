using System;
using System.IO;

namespace MphRead.Mods.Network
{
    /// <summary>
    /// Canonical replay capture for a dedicated server that runs the simulation.
    /// It records the packets the server itself treats as authoritative: accepted
    /// slot intents, its own snapshots, and the roster/match-state stream.
    ///
    /// Recording is intentionally failure-isolated. An I/O problem aborts the
    /// .part file and reports it, but never changes or stops the match.
    /// </summary>
    internal static class ServerReplayRecorder
    {
        private static ReplayWriterV3? _writer;
        private static uint _startFrame;

        public static bool IsRecording => _writer != null;
        public static string? CurrentPath { get; private set; }
        public static string? LastError { get; private set; }

        public static bool Start(ReplayMetadata metadata)
        {
            Stop();
            LastError = null;
            try
            {
                if (metadata.MapHash == 0)
                    throw new IOException("The server replay map could not be identified.");

                string room = Sanitize(metadata.RoomKey.Length == 0 ? "match" : metadata.RoomKey);
                string directory = Paths.Combine(Paths.Export, "_demos", "server");
                Directory.CreateDirectory(directory);
                CurrentPath = Path.Combine(directory,
                    $"{room}_{DateTime.Now:yyyy-MM-dd_HH-mm-ss-fff}_{Guid.NewGuid():N}{DemoFile.Extension}");
                _writer = new ReplayWriterV3(CurrentPath, metadata);
                _startFrame = NetSession.NetFrame;
                _writer.WriteEvent(new ReplayEvent(0, ReplayEventType.MatchStarted));
                Console.WriteLine($"[replay] canonical server recording: {CurrentPath}");
                return true;
            }
            catch (Exception ex) when (ex is IOException or UnauthorizedAccessException
                or InvalidDataException or ArgumentException)
            {
                LastError = ex.Message;
                _writer = null;
                CurrentPath = null;
                Console.WriteLine($"[replay] canonical recording did not start: {ex.Message}");
                return false;
            }
        }

        public static void Record(PacketType type, ReadOnlySpan<byte> payload)
        {
            if (_writer == null) return;
            byte[] packet = new byte[1 + payload.Length];
            packet[0] = (byte)type;
            payload.CopyTo(packet.AsSpan(1));
            Write(packet);
        }

        public static void RecordSlotIntent(int slot, ReadOnlySpan<byte> payload)
        {
            if (_writer == null || slot is < 0 or >= RosterPacket.MaxSlots) return;
            byte[] packet = new byte[2 + payload.Length];
            packet[0] = (byte)PacketType.SlotIntent;
            packet[1] = (byte)slot;
            payload.CopyTo(packet.AsSpan(2));
            Write(packet);
        }

        public static void RecordEvent(ReplayEvent value)
        {
            if (_writer == null) return;
            uint frame = Frame();
            _writer.WriteEvent(value with { Frame = frame });
        }

        public static void Stop(bool matchEnded = false)
        {
            ReplayWriterV3? writer = _writer;
            _writer = null;
            if (writer == null)
            {
                CurrentPath = null;
                return;
            }
            try
            {
                if (matchEnded)
                    writer.WriteEvent(new ReplayEvent(Frame(), ReplayEventType.MatchEnded));
                writer.Dispose();
            }
            catch (Exception ex) when (ex is IOException or UnauthorizedAccessException
                or InvalidDataException)
            {
                LastError = ex.Message;
                writer.Abort();
                Console.WriteLine($"[replay] canonical recording interrupted: {ex.Message}");
            }
            finally
            {
                CurrentPath = null;
            }
        }

        private static void Write(ReadOnlySpan<byte> packet)
        {
            ReplayWriterV3? writer = _writer;
            if (writer == null) return;
            try
            {
                writer.WriteRecord(Frame(), packet);
            }
            catch (Exception ex) when (ex is IOException or UnauthorizedAccessException
                or InvalidDataException)
            {
                LastError = ex.Message;
                writer.Abort();
                _writer = null;
                CurrentPath = null;
                Console.WriteLine($"[replay] canonical recording interrupted; recover its .part file: {ex.Message}");
            }
        }

        private static uint Frame()
        {
            uint now = NetSession.NetFrame;
            return now >= _startFrame ? now - _startFrame : 0;
        }

        private static string Sanitize(string value)
        {
            foreach (char c in Path.GetInvalidFileNameChars())
                value = value.Replace(c, '_');
            return value;
        }
    }
}
