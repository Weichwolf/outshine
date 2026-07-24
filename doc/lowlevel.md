# LOWLEVEL autopilot — terrain-following at 450 kt / 500 ft

The LOWLEVEL mode flies the F-16 low and fast, hugging the terrain and steering toward valleys.
Vertical and lateral are separate laws over a shared coarse-DEM oracle (`FBTerrainField`, Terrarium
tiles via `fb_stream_dem`; async on WASM, synchronous native). Guidance feeds the normal
`FBFlightControl` FLCS chain — honest `*-cmd-norm` commands, no position cheat.

## Modes (lateral)

| Mode | Select | Behaviour |
|---|---|---|
| **Fan** (default) | default | Reactive terrain fan chooses the heading toward the lowest valley. |
| Fixed heading | `?hdg=N` / `FB_LL_FIXHDG=1` | Holds heading N; the vertical law still terrain-follows. |
| A* planner | `?plan=1` / `FB_LL_PLANNER=1` | Far-planner (Dijkstra over z9 height cost) steers by pure-pursuit. |
| Loiter | `?ap=loiter` / `FB_AP_MODE=loiter` | The old bank-to-circle (measurement recipes use this). |

Native `--fly` and the browser both default to **Fan**. Loiter-based gate/`[home]` recipes must set
`FB_AP_MODE=loiter` explicitly.

## Vertical — AGL-hold with look-ahead

Hold `FB_LL_AGL` (150 m) over the terrain. The look-ahead field is z13 (matches `/elev`, `FB_DEM_Z`).
Law = **latest safe climb**: local proportional hold to AGL, MAXed against the climb each corridor
sample needs to clear it — but a ridge is only anticipated once within `(1+ClimbLeadMargin)` of the
latest max-rate-climb distance (`dH/ClimbCap·v`) plus a reaction-lag distance (`ReactTimeS`, the FLCS
climb build-up). So the jet hugs low terrain and pulls up late-but-safe. Guards:

- **Hard floor** — a smooth climb ramp 100–130 m AGL; the commanded AGL never crosses `FloorM` (100 m).
- **Descent guard** — do not sink below AGL over the highest terrain within `DescentGuardM` (5 km): the
  clear-at-envelope rule, so it never dives into a col right before the next peak.
- **Speed reduction** — if a face demands more climb than `ClimbCap`, slow toward `MinSpeedMs` (a face
  rising faster than the jet can climb at 450 kt is out-climbed by cutting speed, not flown into).

### Envelope limit (accepted)

At 450 kt / 500 ft the highest Alps exceed hug capability — a 3000–4000 m massif with a >30° face rises
faster (~140 m/s) than any climb rate. There, LOWLEVEL **climbs to clear at the envelope** (AGL rises
over the massif) rather than hugging. In a stress-test straight cross of the extreme Alps this left a
few brief `AGL < 100` grazes (min ~67 m, **no ground contact**). The 100 m floor is the target for
normal operation; over extreme terrain "no ground contact" wins. In wander/fan operation the lateral
law routes around such confrontations, so they are rare. Tunable via `DescentGuardM` (higher = safer +
higher mean; lower = tighter hug + graze risk).

## Lateral — reactive terrain fan

Once per frame: sweep `FB_FAN_N` (11) rays over `FB_FAN_ARC` (±75°) around the committed heading, each
sampled to `FB_FAN_RANGE` (12 km). Ray cost = distance-weighted terrain height (near counts ~1.7×) +
worst-obstacle + **fence penalty** (a ray leaving the 500 km circle is expensive) + **straight-bias**
(∝ off-axis angle). The min-cost ray sets the target heading, **eased** (`FB_FAN_EASE` /s, frame-rate
independent) with a **turn deadband** (`FB_FAN_TURNDB`, ~10°): hold heading until a valley is clearly
off-axis, then a decisive turn. The vertical look-ahead samples along the **chosen** ray.

Purely reactive (no far goal): facing an impassable wall it makes a decisive, smooth turn-around and
follows the valley back, rather than wandering onward. The A* planner (`?plan=1`) is the optional far
layer for directed wander.

### Wings-level discipline

Roll rest state is **wings level**: heading error within `HeadingDeadbandDeg` (±4°) → bank command
exactly 0 (no roll wobble). Past it, bank ∝ error beyond the deadband, capped at `BankCapDeg` (45°).
Turn deadband + heading-ease give long straight segments and committed, promptly-levelled turns — how
low-level is actually flown.

## Telemetry (1 Hz)

- `[agl]` — alt/agl/ground/vs/bank/hdg/mode (the AGL metric; bank shows wings-level discipline).
- `[lowlevel]` — tgtAgl / gndHere / gndAhead / tgtVs / DEM decodes.
- `[fan]` — ray costs min/mid/max, chosen index, hdg/tgtHdg (watch the sensor decide).
- `[plan]` / `[plan-perf]` — planner goal/waypoints and replan ms (A* mode only).

Verified: fan mean AGL 243 m vs 392 m fixed-heading over the same pre-Alps run (36% lower — valleys are
flatter along-track); zero floor violations; the track curves and threads the valleys.
