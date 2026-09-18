using System;
using System.Collections.Generic;
using System.Linq;
using Avalonia.Controls;
using MphRead.Mods.Network;

namespace MphRead.Mods.Launcher.Gui
{
    internal sealed class LobbyRosterPanel : StackPanel
    {
        private readonly StackPanel _players = new() { Spacing = 5 };
        private readonly Dictionary<byte, LobbyPlayerRow> _rows = new();
        private string _layout = "";
        private byte? _selected;
        public event Action<byte>? PlayerSelected;
        public LobbyRosterPanel(Control identity, Control administration)
        {
            Spacing = 6;
            Children.Add(new Caption("Players")); Children.Add(_players);
            Children.Add(new Caption("Your player")); Children.Add(identity); Children.Add(administration);
        }
        public void Refresh(RosterPacket roster, SessionStatePacket session)
        {
            int teams = LobbyRules.TeamCount(session.Match);
            string layout = teams + ":" + string.Join(",", Enumerable.Range(0, roster.Count).Select(i => $"{roster.Slots[i]}/{roster.Teams[i]}"));
            foreach (byte slot in _rows.Keys.Where(slot => !Enumerable.Range(0, roster.Count).Any(i => roster.Slots[i] == slot)).ToArray()) _rows.Remove(slot);
            for (int i = 0; i < roster.Count; i++)
            {
                byte slot = roster.Slots[i];
                if (!_rows.TryGetValue(slot, out var row))
                {
                    row = new LobbyPlayerRow(roster, i, session.OwnerSlot);
                    row.Selected += () => { _selected = slot; PlayerSelected?.Invoke(slot); foreach (var pair in _rows) pair.Value.SetSelected(pair.Key == slot); };
                    _rows.Add(slot, row);
                }
                row.Update(roster, i, session.OwnerSlot); row.SetSelected(_selected == slot);
            }
            if (_layout == layout) return;
            _layout = layout; _players.Children.Clear();
            for (int team = -1; team < (teams == 0 ? 0 : teams); team++)
            {
                if (teams > 0 && team == -1 && !Enumerable.Range(0, roster.Count).Any(i => roster.Teams[i] < 0)) continue;
                if (teams > 0) _players.Children.Add(new Caption(team < 0 ? "Unassigned" : UiText.Team(team)));
                for (int i = 0; i < roster.Count; i++) if (teams == 0 || roster.Teams[i] == team) _players.Children.Add(_rows[roster.Slots[i]]);
            }
        }
    }
}
