using System.Diagnostics;
using System.Net;
using System.Net.Sockets;
using MphRead;
using MphRead.Mods.Launcher.Gui;
using MphRead.Mods.Network;

int checks = 0;
void Check(bool condition, string name) { checks++; if (!condition) throw new Exception(name); }
ServerStatus Status(SessionPhase phase = SessionPhase.Lobby, bool join = false, int players = 2,
    int max = 8, byte protocol = NetConfig.ProtocolVersion, bool online = true, bool legacy = false) => new()
    { Online = online, LobbyEnabled = !legacy, Legacy = legacy, Phase = phase, AllowJoinInProgress = join,
      Players = players, MaxPlayers = max, Protocol = protocol };
var lobby = ServerPresentation.From(Status());
Check(lobby.CanJoin && lobby.IsLobby && lobby.JoinLabel == "Join Lobby", "available lobby");
var match = ServerPresentation.From(Status(SessionPhase.InMatch, true));
Check(match.CanJoin && match.IsPlaying && match.JoinLabel == "Join Match", "joining running match");
Check(!ServerPresentation.From(Status(SessionPhase.InMatch)).CanJoin, "join disabled");
Check(!ServerPresentation.From(Status(SessionPhase.Starting, true)).CanJoin, "load barrier");
Check(ServerPresentation.From(Status(SessionPhase.PostMatch)).CanJoin, "returning lobby");
Check(ServerPresentation.From(Status(players: 8)).IsFull, "full capacity");
Check(!ServerPresentation.From(Status(players: 9)).CanJoin, "overfull capacity");
Check(!ServerPresentation.From(Status(protocol: 1)).IsCompatible, "protocol mismatch");
Check(ServerPresentation.From(Status(protocol: 0, legacy: true)).CanJoin, "legacy unknown protocol");
Check(ServerPresentation.From(Status(max: 0, legacy: true)).CanJoin, "unknown legacy capacity");
Check(!ServerPresentation.From(Status(online: false)).CanJoin, "offline");
Check(ServerPresentation.From(Status(protocol: 1, players: 8)).State == "Update required", "compatibility takes precedence");
foreach (var phase in Enum.GetValues<SessionPhase>())
foreach (bool online in new[] { true, false })
foreach (int players in new[] { 0, 7, 8 })
foreach (byte protocol in new byte[] { 0, 1, NetConfig.ProtocolVersion })
{
    var value = ServerPresentation.From(Status(phase, true, players, protocol: protocol, online: online));
    Check(value.CanJoin || value.Reason.Length > 0, "every blocked state explains why");
}
using var preCancelled = new CancellationTokenSource(); preCancelled.Cancel();
Check(!NetLaunch.Connect("127.0.0.1", 1, "test", Hunter.Samus, cancellationToken: preCancelled.Token), "pre-cancelled join");
using var silentPeer = new UdpClient(new IPEndPoint(IPAddress.Loopback, 0));
using var cancel = new CancellationTokenSource(100);
var timer = Stopwatch.StartNew();
Check(!NetLaunch.Connect("127.0.0.1", ((IPEndPoint)silentPeer.Client.LocalEndPoint!).Port, "test", Hunter.Samus,
    cancellationToken: cancel.Token), "in-flight cancellation");
Check(timer.Elapsed < TimeSpan.FromSeconds(2) && !NetSession.Active, "cancellation closes transport promptly");
Console.WriteLine($"[online-ui-check] PASS: {checks} checks; no UI initialization or game assets.");
