using System;
using System.Buffers.Binary;
using System.Net;
using System.Reflection;
using System.Runtime.CompilerServices;
using MphRead.Entities;
using MphRead.Mods.Network;
using OpenTK.Mathematics;

namespace MphRead.NetTest
{
    // Drive production handlers synchronously so packet reordering and empty-server
    // boundaries are deterministic and require neither assets nor timing sleeps.
    internal static class NetworkAuditTests
    {
        private const BindingFlags Private = BindingFlags.NonPublic | BindingFlags.Instance | BindingFlags.Static;
        private static int _checks, _failures;
        private static readonly IPEndPoint Owner = new(IPAddress.Loopback, 31001);
        private static readonly IPEndPoint Other = new(IPAddress.Loopback, 31002);

        public static int Run()
        {
            _checks = _failures = 0;
            try
            {
                ContinuousRestart();
                RelaySnapshots();
                FullSnapshot();
                DownloadHeartbeat();
                SessionOrdering();
                TeamConvergence();
                SpireDiagnosticIdentity();
                if (_failures != 0) throw new InvalidOperationException($"{_failures} network audit regressions failed");
                return _checks;
            }
            finally { NetSession.Stop(); }
        }

        private static void SpireDiagnosticIdentity()
        {
            NetSession.StartServerAuthority(_ => { }, () => { });
            typeof(SpireAltPoseCheck).GetMethod("InitializeRoster", Private)!.Invoke(null, new object[] { "MP3 PROVING GROUND" });
            Check(NetSession.CurrentMatchId == 1 && NetSession.AuthorityEpoch == 1
                && NetSession.SlotOccupied[0] && NetPlayerLifecycle.Generation(0) == 1,
                "Spire diagnostic establishes a valid lifecycle roster");
            NetPlayerLifecycle.AcceptState(new PlayerState { SlotIndex = 0, SlotGeneration = 1,
                LifeId = 1, Health = 50, Flags = PlayerState.FlagActive | PlayerState.FlagSpawned }, 1);
            typeof(SpireAltPoseCheck).GetMethod("FeedIntent", Private)!.Invoke(null,
                new object[] { 2u, IntentButtons.AltAttack, new uint[IntentPacket.PressHistory], Vector3.Zero });
            Check(NetSession.RemoteIntentValid[0] && NetSession.RemoteIntents[0].LifeId == 1,
                "Spire diagnostic sends an accepted current-life intent");
            NetSession.Stop();
        }

        private static void DownloadHeartbeat()
        {
            NetSession.StartClient("127.0.0.1", 31003);
            double stale = NetSession.Clock - NetConfig.TimeoutSeconds - 5;
            typeof(NetSession).GetField("_lastServerPacket", Private)!.SetValue(null, stale);
            // Before the fix this was the paused scene's frozen receive time.
            typeof(NetSession).GetField("_updateTime", Private)?.SetValue(null, stale);
            var transport = (NetTransport)typeof(NetSession).GetField("_transport", Private)!.GetValue(null)!;
            var inbox = Field<System.Collections.Concurrent.ConcurrentQueue<ReceivedPacket>>(transport, "_inbox");
            byte[] data = { (byte)PacketType.MapOffer, (byte)'{', (byte)'}' };
            inbox.Enqueue(new ReceivedPacket(new IPEndPoint(IPAddress.Loopback, 31003), data, data.Length));
            typeof(NetTransport).GetField("_inboxCount", Private)!.SetValue(transport, 1);
            uint frame = NetSession.NetFrame;
            Check(NetSession.SessionTimedOut, "download heartbeat fixture starts with an expired receive time");
            typeof(NetSession).GetMethod("PumpMapTransfer", Private)!.Invoke(null, null);
            Check(!NetSession.SessionTimedOut, "download replies refresh the monotonic session timeout");
            Check(NetSession.NetFrame == frame, "download pumping does not advance gameplay frames");
            NetSession.Stop();
        }

        private static void Check(bool ok, string name)
        {
            _checks++;
            if (!ok) { _failures++; Console.Error.WriteLine($"FAIL audit: {name}"); }
        }

        private static T Field<T>(object target, string name) =>
            (T)target.GetType().GetField(name, Private)!.GetValue(target)!;
        private static T Call<T>(object target, string name, params object[] args) =>
            (T)target.GetType().GetMethod(name, Private)!.Invoke(target, args)!;
        private static DedicatedServer Server() => new(0) { RunsTheMatch = false };
        private static SessionStatePacket Session(DedicatedServer server) => Call<SessionStatePacket>(server, "BuildSessionState");

        private static void Send(DedicatedServer server, IPEndPoint sender, PacketType type, byte[] body)
        {
            byte[] data = new byte[body.Length + 1]; data[0] = (byte)type; body.CopyTo(data, 1);
            Call<object>(server, "Handle", new ReceivedPacket(sender, data, data.Length), 1.0);
        }

        private static void Hello(DedicatedServer server, IPEndPoint sender, uint id)
        {
            byte[] body = new byte[6]; body[0] = NetConfig.ProtocolVersion; body[1] = 255;
            BinaryPrimitives.WriteUInt32LittleEndian(body.AsSpan(2), id);
            Send(server, sender, PacketType.Hello, body);
        }

        private static void End(DedicatedServer server)
        {
            var session = Session(server);
            byte[] body = new byte[10]; BinaryPrimitives.WriteUInt16LittleEndian(body, session.MatchId);
            BinaryPrimitives.WriteUInt64LittleEndian(body.AsSpan(2), session.AuthorityEpoch);
            Send(server, Owner, PacketType.MatchEnd, body);
        }

        private static void ContinuousRestart()
        {
            var server = Server(); Hello(server, Owner, 1); End(server);
            Check(Session(server).Phase == SessionPhase.PostMatch, "continuous fixture reaches results");
            ushort match = Session(server).MatchId;
            Send(server, Owner, PacketType.Bye, Array.Empty<byte>());
            Hello(server, Owner, 2);
            Check(Session(server).Phase == SessionPhase.InMatch && Session(server).MatchId != match,
                "first player after empty results starts a playable match");
            Check(!Field<bool>(server, "_ballotOpen"), "empty-server restart closes the previous results ballot");
            End(server);
            Check(Field<double>(server, "_matchEndedAt") >= 0, "restarted continuous match can finish again");
        }

        private static byte[] Snapshot(SessionStatePacket session, uint frame, params PlayerState[] states)
        {
            byte[] body = new byte[SnapshotHeader.Size + states.Length * PlayerState.Size + 64 + NetHealthSync.HeaderSize];
            new SnapshotHeader { MatchId = session.MatchId, AuthorityEpoch = session.AuthorityEpoch,
                Frame = frame, PlayerCount = (byte)states.Length }.Write(body);
            for (int i = 0; i < states.Length; i++) states[i].Write(body.AsSpan(SnapshotHeader.Size + i * PlayerState.Size));
            BinaryPrimitives.WriteUInt16LittleEndian(body.AsSpan(body.Length - NetHealthSync.HeaderSize), session.MatchId);
            return body;
        }

        private static void RelaySnapshots()
        {
            var server = Server(); Hello(server, Owner, 1); Hello(server, Other, 2);
            var state = new PlayerState { SlotIndex = 0, SlotGeneration = 1, LifeId = 1, Health = 50,
                Flags = PlayerState.FlagActive | PlayerState.FlagSpawned, Facing = Vector3.UnitZ };
            var session = Session(server);
            Send(server, Owner, PacketType.Snapshot, Snapshot(session, 1, state));
            Check(Field<uint>(server, "_snapshotFrame") == 1, "valid authority snapshot establishes relay baseline");
            Send(server, Other, PacketType.Snapshot, Snapshot(session, 500, state));
            Check(Field<uint>(server, "_snapshotFrame") == 1, "non-authority cannot change relay baseline");
            state.LifeId = 2;
            Send(server, Owner, PacketType.Snapshot, Snapshot(session, 100, state, state));
            Check(Field<uint>(server, "_snapshotFrame") == 1 && Field<ushort[]>(server, "_slotLives")[0] == 1,
                "duplicate-slot snapshot cannot advance relay frame or life");
            state.LifeId = 1;
            Send(server, Owner, PacketType.Snapshot, Snapshot(session, 2, state));
            Check(Field<uint>(server, "_snapshotFrame") == 2, "valid snapshot after malformed higher frame still arrives");
        }

        private static void DeliverSession(SessionStatePacket state)
        {
            byte[] data = new byte[SessionStatePacket.Size + 1]; data[0] = (byte)PacketType.SessionState;
            state.Write(data.AsSpan(1)); NetSession.InjectPlaybackPacket(data, data.Length); NetSession.Update(0);
        }

        private static void FullSnapshot()
        {
            var server = new DedicatedServer(0, 8) { RunsTheMatch = false };
            var players = new PlayerState[8];
            for (int slot = 0; slot < players.Length; slot++)
            {
                Hello(server, new IPEndPoint(IPAddress.Loopback, Owner.Port + slot), (uint)slot + 1);
                players[slot] = new PlayerState { SlotIndex = (byte)slot, SlotGeneration = 1, LifeId = 1,
                    Health = 50, Flags = PlayerState.FlagActive | PlayerState.FlagSpawned, Facing = Vector3.UnitZ };
            }
            var session = Session(server);
            byte[] body = Snapshot(session, 1, players);
            int health = body.Length - NetHealthSync.HeaderSize;
            Array.Resize(ref body, body.Length + NetHealthSync.MaxSpawns * NetHealthSync.EntrySize);
            body[health + 2] = NetHealthSync.MaxSpawns;
            for (int i = 0; i < NetHealthSync.MaxSpawns; i++)
            {
                int offset = health + NetHealthSync.HeaderSize + i * NetHealthSync.EntrySize;
                BinaryPrimitives.WriteInt16LittleEndian(body.AsSpan(offset), (short)i);
                body[offset + 2] = 3;
            }
            Check(body.Length + 1 == 1875 && body.Length + 1 <= NetConfig.MaxPacketSize,
                "encoded eight-player/56-spawner datagram has the documented bounded size");
            Send(server, Owner, PacketType.Snapshot, body);
            Check(Field<byte[]>(server, "_lastSnapshot").Length == body.Length,
                "relay accepts the complete maximum snapshot");

            // Use the live client's actual endpoint guard, without waiting on UDP scheduling.
            using var endpoint = new NetTransport(0);
            var sender = new IPEndPoint(IPAddress.Loopback, endpoint.LocalPort);
            NetSession.StartClient("127.0.0.1", endpoint.LocalPort);
            NetSession.ApplyMatchState(Call<MatchStatePacket>(server, "BuildState", 1.0), false);
            byte[] control = new byte[SessionStatePacket.Size + 1]; control[0] = (byte)PacketType.SessionState;
            session.Write(control.AsSpan(1));
            void Receive(IPEndPoint from, byte[] data) => typeof(NetSession).GetMethod("Handle", Private)!
                .Invoke(null, new object[] { new ReceivedPacket(from, data, data.Length), NetSession.Clock });
            Receive(sender, control);
            NetSession.ApplyRoster(Call<RosterPacket>(server, "BuildRoster"));
            byte[] packet = new byte[body.Length + 1]; packet[0] = (byte)PacketType.Snapshot; body.CopyTo(packet, 1);
            Receive(new IPEndPoint(IPAddress.Loopback, sender.Port == 65535 ? 65534 : sender.Port + 1), packet);
            Check(NetSession.SnapshotsReceived == 0, "snapshot from an unexpected server endpoint is ignored");
            Receive(sender, packet);
            Check(NetSession.LastSnapshotFrame == 1 && NetSession.RemoteStateValid[7]
                && NetHealthSync.TryGet(55, out var healthState) && healthState.Available,
                "maximum snapshot reaches the final player and health entry on the client");

            BinaryPrimitives.WriteUInt32LittleEndian(body, 100);
            body[health + 2] = NetHealthSync.MaxSpawns + 1;
            Send(server, Owner, PacketType.Snapshot, body);
            body.CopyTo(packet, 1); Receive(sender, packet);
            Check(Field<uint>(server, "_snapshotFrame") == 1 && NetSession.LastSnapshotFrame == 1,
                "oversized health count cannot poison either snapshot baseline");
            body[health + 2] = NetHealthSync.MaxSpawns;
            BinaryPrimitives.WriteInt16LittleEndian(body.AsSpan(body.Length - NetHealthSync.EntrySize), 0);
            Send(server, Owner, PacketType.Snapshot, body);
            body.CopyTo(packet, 1); Receive(sender, packet);
            Check(Field<uint>(server, "_snapshotFrame") == 1 && NetSession.LastSnapshotFrame == 1,
                "duplicate health ID cannot partially commit a snapshot");
            NetSession.Stop(); NetHealthSync.BeginRoom();
        }

        private static void SessionOrdering()
        {
            NetSession.StartPlayback();
            var state = new SessionStatePacket { Policy = ServerSessionPolicy.Lobby, Phase = SessionPhase.InMatch,
                MatchId = 10, AuthorityEpoch = 10, Revision = 1, MaxPlayers = 8, OwnerSlot = 255,
                WorldProfile = MphRead.Mods.Multiplayer.MatchWorldProfile.Resolve(8),
                Match = new MatchDefinition { RoomKey = "MP3 PROVING GROUND", Mode = GameMode.Battle } };
            DeliverSession(state);
            NetSession.ApplyMatchState(new MatchStatePacket { MatchId = 10, AuthorityEpoch = 11,
                RoomKey = state.Match.RoomKey, Mode = (byte)GameMode.Battle }, false);
            state.Revision = 2; state.Phase = SessionPhase.Lobby;
            DeliverSession(state);
            Check(NetSession.IsPlaying && NetSession.SessionRevision == 1 && NetSession.AuthorityEpoch == 11,
                "old-epoch session cannot reset gameplay after newer match control");
            state.AuthorityEpoch = 11; state.Revision = 1; state.Phase = SessionPhase.InMatch;
            DeliverSession(state);
            Check(NetSession.ServerSession?.AuthorityEpoch == 11, "new epoch may restart session revision");
            state.MatchId = 9; state.Revision = 2; state.Phase = SessionPhase.Lobby;
            DeliverSession(state);
            Check(NetSession.IsPlaying && NetSession.ServerSession?.MatchId == NetSession.CurrentMatchId,
                "newer session revision cannot roll back match identity");
            NetSession.Stop();
        }

        private static void TeamConvergence()
        {
            int maxPlayers = PlayerEntity.MaxPlayers;
            bool teams = GameState.Teams; int teamCount = GameState.TeamCount;
            PlayerEntity previous = PlayerEntity._players[7];
            try
            {
                NetSession.StartServerAuthority(_ => { }, () => { });
                NetSession.ApplyMatchState(new MatchStatePacket { MatchId = 1, AuthorityEpoch = 1 }, false);
                var roster = RosterPacket.Create(); roster.MatchId = 1; roster.AuthorityEpoch = 1;
                roster.Revision = 1; roster.Count = 1; roster.Slots[0] = 7; roster.Generations[0] = 1;
                NetSession.ApplyRoster(roster);
                var player = (PlayerEntity)RuntimeHelpers.GetUninitializedObject(typeof(PlayerEntity));
                typeof(PlayerEntity).GetProperty(nameof(PlayerEntity.SlotIndex))!.SetValue(player, 7);
                player.TeamIndex = 0; player.Health = 50; PlayerEntity._players[7] = player;
                PlayerEntity.MaxPlayers = 8; GameState.Teams = true; GameState.TeamCount = 4;
                var activated = (bool[])typeof(NetSlotManager).GetField("_activated", Private)!.GetValue(null)!;
                activated[7] = true;
                roster.Teams[0] = 3; roster.Revision++;
                NetSession.ApplyRoster(roster); NetSlotManager.Sync();
                Check(player.TeamIndex == 3 && player.Health == 50,
                    "late roster corrects active slot-seven team without reinitializing the player");
                GameState.Teams = false; NetSlotManager.Sync();
                Check(player.TeamIndex == 7, "FFA restores slot scoring identity for an active player");
            }
            finally
            {
                PlayerEntity._players[7] = previous; NetSlotManager.Reset(); NetSession.Stop();
                PlayerEntity.MaxPlayers = maxPlayers; GameState.Teams = teams; GameState.TeamCount = teamCount;
            }
        }
    }
}
