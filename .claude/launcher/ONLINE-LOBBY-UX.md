# Online browser and persistent lobby

The Online page separates the server list/search from selected-server details.
`ServerPresentation.From` is the pure source of availability, compatibility,
capacity, phase, action labels and blocked-action explanations. Protocol 12 and
all packet layouts are unchanged. Unknown legacy capacity/protocol is not treated
as a known incompatibility. Starting is a temporary blocked UI state; continuous
servers still join their running match directly.

Favorites and recent endpoints retain the existing bookmark store. Refresh restores
selection by endpoint; filtering clears unavailable selections. Direct Connect is
an expansion: check the address first, then use the same detail/action surface.
Directory failure, an empty directory and an empty search have different messages.

Join runs off the UI thread with an optional `CancellationToken` on `NetLaunch.Connect`.
Back or Cancel Join cancels that worker, waits for it to stop, closes its session,
and restores the same browser and polling. No second connection is allowed during
this interval. Cancellation racing with Welcome still wins at the UI boundary.
Failure uses the existing DNS/refusal/protocol/timeout diagnostics.

`LobbyContext` carries display identity through LaunchPlan into StartScreen. A
locally created server is labeled as local with its port, never as a public invite
to 127.0.0.1. StartScreen retains the same LobbyScreen and NetSession between rounds.

## Lobby ownership and editing

LobbyScreen coordinates three small controls:

- LobbyRosterPanel groups by the authoritative TeamLayout, retains row controls
  across ping updates, and exposes visible selection for player administration.
- LobbyMatchPanel defaults to a readable summary for guests and hosts. Hosts enter
  an explicit Edit Match flow; Apply needs a valid changed draft, Cancel restores
  the server values. A changed revision reloads the draft with feedback. Apply waits
  for both the command acknowledgment and matching authoritative state, allowing
  those packets to arrive in either order.
- LobbyChatPanel renders all 64 retained messages, distinguishes system messages,
  follows the bottom only when already there, and retains manual scroll position.
  Enter sends; Escape leaves text entry without clearing the draft or leaving.

The header shows identity, endpoint, population, phase, local ping and connection
state. YOU/HOST/READY are text, not color alone. Connection degradation disables
mutation controls and chat. Host transfer changes controls in place. Confirmation
rechecks roster revision, ownership and availability before a removal or transfer.
Leave explains ownership migration when other players remain.

Ready/Start/Leave stay outside the scrolling content. LobbyRules.Validate supplies
the start reason. Editing, command submission and loading block duplicate Start.
Authoritative errors stay visible; transient notices use a bounded queue with four
seconds per message. Join/leave system chat is not duplicated as a notification.

## Responsive input

Wide and medium windows use roster/match columns plus chat. Below 720 points, or
in a surface shorter than 550 points, the lobby uses Players/Match/Chat tabs and
the same footer. The browser uses Servers/Details tabs at narrow widths. Checks
consider the actual surface as well as logical size because desktop scaling can
otherwise conceal a short physical window. Controller tab navigation prefers the
focused/deepest enabled tab group. Existing focus navigation brings scrolled
controls into view; touch and keyboard use the same controls.

## Verification

- `dotnet run --project tools/online-ui-check/online-ui-check.csproj -c Release`:
  87 pure state/cancellation assertions without UI initialization or game assets.
- `-gamepadcheck`: browser availability/filter/selection, draft validation and stale
  revisions, role changes, pending Ready, chat Escape/scroll, lost connection and switching
  between Online and Offline at compact widths.
- `-uimatrix DIR`: 302 captures including 150 new online/lobby state captures at
  1920x1080, 1280x720, 960x600, 800x400, 430x860 and 860x430. Each capture checks
  focus reachability and scrolling. Mock lobby injection never opens a socket.
- `-netlobbytest`: real loopback persistent lifecycle, two rounds, migration,
  teams, rebind and continuous rotation. Existing CI builds desktop, dedicated
  server and Android targets, and checks that game assets are absent.

Headless captures verify rendering/layout/input behavior, not physical controller
hardware, Android touch ergonomics or a live macOS gameplay window.
