# Mission data — weather (`wx`)

**Source of this file:** the former `doc/mission-format.md` (split in the Phase-3 mirror rebuild), section
"Wetter (`wx`)". Translated 1:1 from the German original; no revision of content.

This file is the **mission-author's view** of the weather hook: the one line, the three providers, the
precedence rule and what a wind measurably does. The server side of the data (`/wx`, the FBWX format,
the GFS run) is in [`../world/weather.md`](../world/weather.md).

---

## Spec

One line, mission-wide, at most once, optional. It selects the **implementation of the weather hook**
(`core/FBWeatherProvider`, the sibling of `FBElevationProvider`) for the whole run:

| Line | Provider | Meaning |
|---|---|---|
| *(none)* / `wx calm` | `core/FBCalmWeather` | no wind, no cloud, unlimited visibility. **The default** — every mission without a `wx` line flies exactly as before the hook existed (byte-identical telemetry, re-measured). |
| `wx wind <dirDegFROM> <speedKt>` | `core/FBConstantWindWeather` | ONE vector everywhere on earth and at every altitude. `dirDegFROM` is the direction it blows FROM (0..360, meteorological), `speedKt` 0..300. Deliberately unphysical: no shear, no boundary layer — a MEASURING INSTRUMENT whose answer is a closed formula. |
| `wx fixture <name\|path>` | `core/FBFixedWeather` | a real FBWX blob from `fb-tiles`' `/wx` (GFS 0.25°, subsampled to 0.5°): wind at 10 m plus 850/700/500/250 hPa, geopotential per surface, cloud cover total/low/mid/high, cloud ceiling, visibility. A name WITHOUT `/` is resolved under the client's asset directory (`mods/f16/src/`), a path with `/` is taken literally. |

Values outside the bands, a second `wx` line or an unknown keyword are parse errors; a declared fixture
blob that cannot be read is a **runtime FAIL** (exit 1) and not a silent fallback — a run in the wrong
atmosphere would be worse than no run at all.

**Precedence rule, the same for every client:** the mission ALWAYS wins. Without a `wx` line the
client's default applies — `fb-gym` and `gpu_native`: `calm` (a measurement must be reproducible), the
browser: **live** (one `GET /wx` per session, `calm` until then). That is why there is no CLI flag:
weather is part of the SCENARIO like the runway and the spawn, not a capability of the client like
`--elev`.

### How the wind acts

No module, no pilot and no controller sees the provider. The OWNER of the unit (`FBMissionRunner`, in
the browser the frame loop) samples it at the decision rate at the unit's position and lays the vector
through `FBSimUnit::UpdateWind` → `FBFdm::SetWindNedMs` into JSBSim's `FGWinds`; JSBSim subtracts it
from the ground velocity and computes the aerodynamics on the difference. A pilot therefore EXPERIENCES
wind only as drift on his instruments (heading against ground track in the Platform block), just as in
a real aircraft. The vector is only written on CHANGE — in calm air the channel is never touched.

## State

| Item | State |
|---|---|
| `wx` line | built; three providers, parse-error bands, runtime FAIL on an unreadable fixture |
| Wind wiring | built; sampled by the owner, written into `FGWinds`, only on change |
| Gym fixture | `mods/f16/src/data/wx-2026-07-27T00Z.wxb`, a byte-exact copy of `tiles/testdata/wx-gfs-2026-07-27T00Z-step2-v1.wxb` (GFS cycle 2026-07-27 00Z, analysis step, FBWX v1, 720×361, 20 fields, 8,317,984 B, sha256 `acded02…9ede`) |
| Drift check | `make -C sim test-weather` → `build/fb-test-weather` parses the fixture and recomputes the sample values published in [`../world/weather.md`](../world/weather.md) from an INDEPENDENT decoder (ecCodes 2.41), with the quantisation step of each field as the tolerance. The same binary is the probe: `fb-test-weather <blob> <lat> <lon> <altM>` prints wind, visibility, cover and cloud base at a point. |

### Measured — crosswind

Measured on `mods/f16/src/missions/wx-crosswind.fbm` (straight flight north, `wx wind 270 20`, against the same
run with `wx calm`, window 200–300 s):

| Quantity | Expectation | Measured | Deviation |
|---|---|---|---|
| Drift correction angle (heading − ground track) | `asin(10.289 / 180.019)` = 3.2765° | 3.3078° | 0.031° (0.95 %) |
| Groundspeed | `sqrt(TAS² − Vw²)` = 179.725 m/s | 179.717 m/s | 0.008 m/s (0.004 %) |
| Ground track | unchanged (the guidance compensates) | 359.95° against 359.998° calm | 0.046° |

### Measured — a release in wind misses, and that is physics, not a defect

The fire control computer (`modules/f16/FBF16FireControl`) integrates the stored ballistics table
against STILL air; nothing in the jet hands it a wind vector. The bomb then falls through an air mass
that is moving, and lands elsewhere. `mods/f16/src/missions/wx-ccrp-wind.fbm` is `attack-ccrp.fbm` plus
`wx wind 360 25`; measured on the `stores DELIVERY` line of both runs:

| Crosswind | `aimAcrossM` | Displacement | Ground target |
|---:|---:|---:|---|
| 0 (calm) | 9.88 m | — | DESTROYED |
| 25 kt | 22.67 m | +12.79 m | DESTROYED |
| 50 kt | 34.97 m | +25.09 m | DESTROYED |
| 100 kt | 55.04 m | +45.16 m | **INTACT** |

The displacement is markedly SMALLER than "wind × fall time" (at 25 kt that would be 127 m): a 227 kg
bomb at 220 m/s couples so weakly to the air laterally that its lateral relaxation time is around two
minutes — in ten seconds of fall time it notices almost nothing. That is a property of the weapon and
the release altitude, not a correction missing somewhere.

### Measured — a steerpoint the guidance cannot close

`mods/f16/src/missions/wx-orbit.fbm`, `wx wind 338 39` (this is the GFS fixture's own 9,000 m wind restated as a
closed form: u +7.37 / v −18.58 m/s), eastbound leg, so 18.6 m/s stands across it:

| Quantity | Calm | In wind |
|---|---:|---:|
| Closest approach to a steerpoint dead ahead, 38 km run-in | 495.6 m | **614.3 m** |
| Captured (500 m circle)? | yes, t = 167.3 s | **no** |
| What follows | route continues, SUCCESS t = 316.6 s | a permanent orbit: range 1,793…4,851 m, −59.1° bank, 99.2 s per lap |

Four metres of margin is the whole difference. The cause is a mismatch of frames — the capture circle is
a GROUND test of fixed radius while the circle the aircraft can fly lives in the air mass — plus the fact
that a fix without a leg is flown by the BEARING law, which controls the nose rather than the ground
track. The answer is the `orbited` sequencing ground; derivation in
[`../systems.md`](../systems.md) §7.5.1.

## Gaps

| Gap | Detail |
|---|---|
| No shear, no boundary layer in `wx wind` | one vector everywhere and at every altitude — deliberately a measuring instrument, not weather |
| The DIRECT bearing law still controls the nose, not the ground track | that is what leaves a standing lateral drift on the run-in to a leg-less fix (above). Fixing it at the root would let the jet CLOSE such a fix instead of being sequenced past it, but it moves every index-0 trajectory in the tree, including the four BFM defenders' deliberate orbits |
| The 10 m surface is anchored at 10 m ASL, not above ground | the one approximation, documented in the provider; a terrain-dependent boundary-layer wind would need the elevation hook as a second input |
| Only the analysis step | `wx fixture` carries an f000 blob; there is no time axis, so a session longer than the run sees the same atmosphere |
| No cloud rendering from the provider | the provider is consumed by `FBWorld::SetWeather/Weather()`; terrain masking and the rendering side are open — see [`../render/clouds.md`](../render/clouds.md) |
| The fire control gets no wind | deliberately: nothing in the real jet hands it one, and the resulting miss is measured rather than corrected away |

## Knowledge

- **Why there is no `--wx` CLI flag.** Weather is part of the scenario, like the runway and the spawn.
  A flag would make the same mission file mean different things on different clients, and a measurement
  would stop being reproducible from the file alone.
- **Why the mission always wins.** The two defaults differ on purpose — the gym needs reproducibility
  (`calm`), the browser wants to look like today (`live`). A `wx` line overrides both, so a mission
  built for a measurement measures the same thing everywhere.
- **Why an unreadable fixture is a FAIL and not a fallback.** Falling back to `calm` would produce a
  run in the wrong atmosphere that looks like a valid one. The exit code is cheaper than the wrong
  number.
- **Why the wind is written only on change.** In calm air the channel is never touched at all, which is
  what makes "every mission without a `wx` line is byte-identical to before the hook existed" true
  rather than merely likely.
- **Why the format has a mirror on the sim side.** `core/FBWxFormat.h` mirrors `tiles/src/wxfmt.h`
  because `core/` must not point at `tiles/`. Against the two drifting apart stands a checker rather
  than a convention (`make -C sim test-weather`).
