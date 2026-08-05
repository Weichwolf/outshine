# f16 — missions

296 `.fbm` in [`../src/missions/`](../src/missions), plus two subdirectories that the regression glob
deliberately does not reach. Syntax, verdict rules and telemetry belong to the engine and are **not
repeated here**: [`doc/missions/INDEX.md`](../../../doc/missions/INDEX.md).

Every mission carries its own reading rule in its header; that header is binding, the exit code alone
is not.

| Family | ~n | Intent |
|---|---|---|
| `payerne-*` | 14 | the reference sortie: take-off, waypoints, landing to a full stop |
| `w1…w5-*`, `o1…o5-*` | 100 | the campaign rungs — one per `.fbc` step ([campaign.md](campaign.md)) |
| `ar-*` | 30 | the arena instrument, 30 synthetic rungs, all top-of-ladder |
| `sat-*` | 11 | saturation: many actors against one picture |
| `mig29-*` | 16 | the red airframe on its own — envelope, gun, radar, optics |
| `bfm-*`, `duel-*`, `gun-*`, `bvr-*` | 25 | close and beyond-visual engagement geometry |
| `sam-*`, `net-*`, `rwr-*`, `cm-*` | 25 | the ground layer, its net, and what warns of it |
| `mk82-*`, `mk84-*`, `tank-*`, `attack-*`, `arm-*` | 25 | air-to-ground: carriage, release, lethal radius |
| `vis-*`, `wx-*` | 12 | eyes and weather |
| rest | — | single-subject probes named after their subject |

| Subdirectory | Why it is not in `missions/*.fbm` |
|---|---|
| `negative/` (8) | declarations that must be REFUSED — pass criterion is an exit code and a message |
| `scale/` (111) | GENERATED casts for the actor-scaling bench; no reading rule, no verdict |
