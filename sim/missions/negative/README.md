# `missions/negative/` — the files that must NOT run

A subdirectory and not a `.fbm` beside the others, because everything in `sim/missions/*.fbm` is a
mission the regression loop runs and expects a verdict from. These carry a declaration that must be
REFUSED, so their pass criterion is an exit code and a message, not telemetry.

| File | Invocation | Required outcome |
|---|---|---|
| `clock-bad-format.fbm` | `fb-gym --mission missions/negative/clock-bad-format.fbm` | exit 1, `RESULT result=FAIL reason="parse: line N: 'time ...' is not YYYY-MM-DDThh:mm:ssZ ..."` — no spawn, no telemetry |
| `clock-flag-collision.fbm` | `gpu_native --mission missions/negative/clock-flag-collision.fbm --utc 946684800` | exit 1, `RESULT result=FAIL reason="mission declares 'time ...' and a client clock override ... is set"` — the collision is checked BEFORE the spawn, so it needs neither a tile server nor a GPU |

The same file **without** `--utc` is a normal, valid mission — which is the point: the error is the
COLLISION, not the file.
