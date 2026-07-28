# `missions/negative/` — the files that must NOT run

A subdirectory and not a `.fbm` beside the others, because everything in `sim/missions/*.fbm` is a
mission the regression loop runs and expects a verdict from. These carry a declaration that must be
REFUSED, so their pass criterion is an exit code and a message, not telemetry.

| File | Invocation | Required outcome |
|---|---|---|
| `clock-bad-format.fbm` | `fb-gym --mission missions/negative/clock-bad-format.fbm` | exit 1, `RESULT result=FAIL reason="parse: line N: 'time ...' is not YYYY-MM-DDThh:mm:ssZ ..."` — no spawn, no telemetry |
| `clock-flag-collision.fbm` | `gpu_native --mission missions/negative/clock-flag-collision.fbm --utc 946684800` | exit 1, `RESULT result=FAIL reason="mission declares 'time ...' and a client clock override ... is set"` — the collision is checked BEFORE the spawn, so it needs neither a tile server nor a GPU |
| `objective-self-protect.fbm` | `fb-gym --mission missions/negative/objective-self-protect.fbm` | exit 1, `reason="parse: line 14: unit 'viper' cannot have itself as a target"` — `protect`/`identify`/`deny release` on oneself; `survive` is the spelling that exists |
| `objective-identify-unknown.fbm` | `fb-gym --mission missions/negative/objective-identify-unknown.fbm` | exit 1, `reason="parse: unit 'viper': 'objective identify unit ghost range 2000 hold 30' names no unit in this mission"` — resolved against the whole cast at end of file, exactly like `kill unit` |
| `objective-identify-nobox.fbm` | `fb-gym --mission missions/negative/objective-identify-nobox.fbm` | exit 1, `reason="parse: line 14: 'objective identify' needs 'range <metres>' with a positive value"` — there is no default box, and a zero one is a geometry nothing can be inside |

The same file **without** `--utc` is a normal, valid mission — which is the point: the error is the
COLLISION, not the file.
