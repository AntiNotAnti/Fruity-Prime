"""Summarize run-windows.ps1 evidence; completion is not gameplay acceptance."""
import argparse
import collections
import csv
import json
import re
from pathlib import Path

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("roots", nargs="+", type=Path)
parser.add_argument("--output", required=True, type=Path)
args = parser.parse_args()

weapon_columns = "attempted spawned localHits authorityHits predictions claims rescues refusals predictedDamage authorityDamage localHeadshots authorityHeadshots meanRewind clampPercent".split()
weapons = "PowerBeam Missile Imperialist Magmaul ShockCoil Judicator Battlehammer VoltDriver Enemy alt/bomb".split()
rows = []
for root in args.roots:
    for profile_file in sorted(root.glob("*/profile.json")):
        folder = profile_file.parent
        row = {"run": folder.name, "root": str(root), **json.loads(profile_file.read_text(encoding="utf-8-sig"))}
        row["completed"] = True
        for peer in ("ALPHA", "BRAVO"):
            path = folder / f"{peer}.log"
            log = path.read_text(errors="replace") if path.exists() else ""
            row["completed"] &= "what this client saw" in log and "2 player(s) in scene" in log
            for weapon in weapons:
                matches = re.findall(rf"^{re.escape(weapon)} ((?:[\d.]+ ?)+)$", log, re.M)
                if matches:
                    values = matches[-1].split()
                    for column, value in zip(weapon_columns, values):
                        key = f"client_{column}"
                        row[key] = row.get(key, 0) + float(value)
            heads = re.findall(r"headshots: (\d+) predicted, (\d+) agreed.*?, (\d+) downgraded", log)
            if heads:
                for column, value in zip(("headPredicted", "headAgreed", "headDowngraded"), heads[-1]):
                    row[column] = row.get(column, 0) + int(value)
            claims = re.findall(r"hit claims: (\d+) declared, (\d+) applied, (\d+) already resolved.*?, (\d+) void \(dead shooter\), (\d+) void \(victim down\), (\d+) refused, (\d+) unanswered, (\d+) repeats", log)
            if claims:
                for column, value in zip(("declared", "applied", "resolved", "deadShooter", "deadVictim", "refused", "unanswered", "repeats"), claims[-1]):
                    row[column] = row.get(column, 0) + int(value)
        server_file = folder / "netlog-server.txt"
        server = server_file.read_text(errors="replace") if server_file.exists() else ""
        hitreg = re.findall(r"hitreg .*", server)
        if hitreg:
            for column in ("rewound", "meanRewind", "clamped", "historyMiss", "claimsIn", "rescued", "dupIn", "voidShooterIn", "refusedIn"):
                match = re.search(rf"\b{column}=([\d.]+)", hitreg[-1])
                if match:
                    row[f"server_{column}"] = float(match[1])
        hits = re.findall(r"stage=authority-hit .*?damage=(\d+)", server)
        row["server_hitEvents"] = len(hits)
        row["server_damage"] = sum(map(int, hits))
        keys = re.findall(r"key=(\S+) stage=spawn ", server)
        counts = collections.Counter(keys)
        row["server_spawnEvents"] = len(keys)
        # Shared keys can be legitimate multi-projectile/continuous events. This
        # is a correlation aid, never an automatic phantom-shot verdict.
        row["server_repeatedSpawnKeys"] = sum(count - 1 for count in counts.values())
        rows.append(row)

args.output.parent.mkdir(parents=True, exist_ok=True)
fields = list(dict.fromkeys(key for row in rows for key in row))
with args.output.with_suffix(".csv").open("w", newline="") as stream:
    writer = csv.DictWriter(stream, fieldnames=fields)
    writer.writeheader()
    writer.writerows(rows)

summary = {}
for age in (False, True):
    group = [row for row in rows if row["PressAge"] == age and row["completed"]]
    if not group:
        continue
    totals = {key: sum(row.get(key, 0) for row in group) for key in (
        "client_spawned", "client_localHits", "declared", "applied", "resolved", "deadShooter",
        "deadVictim", "refused", "unanswered", "repeats", "headPredicted", "headAgreed",
        "headDowngraded", "server_hitEvents", "server_damage", "server_spawnEvents",
        "server_repeatedSpawnKeys", "server_rewound", "server_clamped")}
    rewound = totals["server_rewound"]
    totals["meanRewindFrames"] = sum(row.get("server_rewound", 0) * row.get("server_meanRewind", 0) for row in group) / max(1, rewound)
    totals["runs"] = len(group)
    summary[str(age)] = totals
result = {"runs": len(rows), "completed": sum(row["completed"] for row in rows), "pressAge": summary,
    "limits": "Seeded local UDP measurements. Short runs and unequal shot exposure do not establish causality. Repeated spawn keys are not a ghost-shot count."}
args.output.with_suffix(".json").write_text(json.dumps(result, indent=2) + "\n")
print(json.dumps(result, indent=2))
