# Mission output per run, and the example missions

**Source of this file:** the former `doc/mission-format.md` (split in the Phase-3 mirror rebuild), sections
"Ausgabe je Lauf (`--out DIR`)" and "Beispiele". Translated 1:1 from the German original; no revision
of content.

The per-topic telemetry columns live with their topics ([`sensors.md`](sensors.md),
[`avionics.md`](avionics.md), [`weapons.md`](weapons.md), [`combat.md`](combat.md)); this file holds
the file layout, the attribution rules, the damage channel and the mission catalogue.

---

## Spec

### The files

- `telemetry.csv` — the primary actor (index 0). Canonical name, unchanged.
- `telemetry_<callsign>.csv` — every further unit, same schema. One file per unit instead of one wide
  line: the columns follow the unit's MODULE, and a shared line would either force all modules into one
  schema or make the header depend on the cast.
- `events.log` — `t=SEC LEVEL tag EVENT key=val …`, greppable.

### Damage events and columns

(`core/FBDamageModel`, see "Schadensmodell" in CLAUDE.md): per hit one `damage DAMAGE` line (zone,
distance to the airframe structure, fragment energy in J/m², explosive mass, closure, burst point in
the body frame, the two bitmasks), per affected system one `damage SYSTEM` line
(`system=… state=degraded|failed`) and, if the hit makes the unit combat-ineffective, exactly one
`damage KILL` line. In `telemetry.csv` four columns are appended at the very end (existing ones never
shift): `dmg_hits`, `dmg_failed`, `dmg_degraded` (bitmasks over `FBSystemId`) and `dmg_effective`. The
block validity columns (`blk_*`) show the same event from the avionics bus's point of view: a failed
system switches its block to `0` (invalid).

### Unit attribution in the log

If a mission has MORE THAN ONE unit, every line belonging to an actor carries `unit=<callsign>` as its
first field (`core/FBLog.h`'s `FBLogUnitScope`) — including the module-internal ones (`nav`, `pilot`)
and the monitor's. With exactly one unit the field is omitted: the mission's lines ARE that unit's,
there is nothing to distinguish (and older regression baselines stay byte-identical).

### Partial results

Before the combined `RESULT` line the runner emits, for more than one unit, one machine-readable
`UNIT_RESULT` line per actor:

```
t=222.1 INFO mission UNIT_RESULT unit=lead result=SUCCESS reason="all waypoints reached" team=friendly \
    decisive=0 lat=46.9683 lon=7.05104 altM=3211.24 telemetry=out/telemetry.csv
```

`result` is `SUCCESS|FAIL|TIMEOUT|CRASH|LOC|NONE` (NONE = a unit without objectives), `decisive=1`
marks the unit whose verdict ENDED the run (on SUCCESS, none). The closing `RESULT`/`SUMMARY` lines
carry the same `unit=` attribution as that unit.

### Examples

- `sim/missions/payerne-takeoff-only.fbm`, `sim/missions/payerne-takeoff.fbm` — Payerne (LSMP) runway
  23, ground start; threshold coordinates and length checked against `fb-tiles`' `/elev` endpoint (DEM
  ~441 m at the threshold).
- `sim/missions/payerne-pair-datalink.fbm` — two friendlies flying apart, terminals configured to
  6 nm: shows latency (`dl_age`), net cycle and the range loss (`TRACK_LOST`).
- `sim/missions/payerne-flight-datalink.fbm` — five friendlies in range, each with a different terminal
  configuration: the switch matrix (POWER/XMT/filter) in one run.
- `sim/missions/payerne-airstart.fbm` — the same airspace but a pure air start (~10 nm SW of Payerne,
  2500 m ASL, 300 kt, gear up, 60 % fuel) — no `runway` line, no taxi/rollout.
- `sim/missions/payerne-landing.fbm` — the landing training ground: air start ~9 nm lined up on the
  RWY23 final, ~500 m AGL, then `land` — SUCCESS = standstill on the runway.
- `sim/missions/payerne-full.fbm` — the target mission: ground start at Payerne → three waypoints →
  `land` on the same runway as the departure.
- `sim/missions/payerne-pair.fbm` — **two** friendly F-16s in an air start beside each other, each with
  its own waypoints; SUCCESS only when BOTH have reached their objectives.
- `sim/missions/payerne-pair-fail.fbm` — the same pair with a waypoint unreachable for `two` at a tight
  timeout: `lead` reaches its objectives, the overall verdict is negative anyway and names `two` as the
  unit that decided it.
- `sim/missions/payerne-four.fbm` — a flight of four in an air start, each unit in its own altitude
  block: the scaling case for `fb-gym --threads` (four roughly equally expensive airframes).
- `sim/missions/payerne-mixed.fbm` — deliberately unequal load: `roller` starts on the ground at the
  threshold, `cruiser` is already in cruise — the stress test of the lockstep barrier.
- `sim/missions/payerne-radar-acm.fbm` — the radar reference geometry: two jets on perpendicular,
  straight, equally high legs. The target starts outside every ACM box, swings through the nose and
  finally leaves the antenna range — no contact → build-up → lock → coast → loss in ONE run.
- `sim/missions/payerne-radar-bore.fbm` — the same file with ONE word changed (`acm_bore` instead of
  `acm_hud`): same geometry, narrower volume, measurably later detection.
- `sim/missions/payerne-radar-iff.fbm` — one interrogator, three targets crossing the nose one after
  the other: a friend with a transponder (friendly), a friend without (unknown), an enemy WITH a
  transponder (unknown — not hostile).
- `sim/missions/cmd-avionics.fbm` — the avionics command/validity demonstrator (not a flight test):
  briefed entries in both latency classes, all four acknowledgement outcomes, both own-policy reasons,
  plus a switched-off radar altimeter (`invalid`) beside the anyway-held radar picture (`held`).
- `sim/missions/attack-ccrp.fbm` — the air-to-ground reference run: 19 km level approach, CCRP release
  of a Mk-82 on a declared `target_soft`, impact 22.2 m beside the target, target destroyed,
  `objective kill unit` fulfilled (SUCCESS, exit 0). Measure with `--elev const` (flat 0 m base:
  computation plane, target altitude and impact ground are then the same number).
- `sim/missions/attack-ccip.fbm` — the same approach on the CCIP cue, built with `objective survive` so
  that the run continues past the hit to the end of the egress turn and back into the route: the
  COMPLETE attack sequence in one run.
- `sim/missions/attack-late.fbm` — the cross-check: the same file with ONE line more
  (`set pilot_attack_bias_s 2.0`, release two seconds after the cue). 482 m miss distance instead of
  22 m, target standing, TIMEOUT (exit 3) — the measure of what the computation achieves.
- `sim/missions/attack-hardened.fbm` — the same good release against `target_hard`: same miss distance,
  no effect, `result=INTACT`. The fragility classes are a model, not decoration.
- `sim/missions/test-wp-inside-turn.fbm` — waypoint sequencing at its edge: WP1 lies 1,000 m laterally
  from WP0 and therefore INSIDE this jet's tightest turn circle (~1,400 m at 300 kt), so it is
  unreachable by capture circle (closest approach 1,000 m, measured). Over the leg axis it is ticked
  off after 15.1 s as `by=passed` and WP2 flown normally — SUCCESS (exit 0); without the rule the jet
  orbits it at −58.9° bank until TIMEOUT (exit 3). See "When a waypoint is reached" in
  [`verdict.md`](verdict.md).
- `sim/missions/wx-orbit.fbm` — the same edge made by the WIND instead of by the geometry: at 9,000 m in
  18.6 m/s of crosswind the closest approach to a steerpoint dead ahead is 614 m (114 m outside the
  capture circle) and the jet settles into a permanent −59.1° orbit, 99.2 s per lap. The `orbited`
  ground ticks it off at t = 311.6 s and the route finishes at t = 485.4 s (SUCCESS, exit 0). The same
  file with `wx calm` captures the same fix with 4 m to spare — the wind is the whole difference.
- `sim/missions/bfm-pointblank.fbm` — a 0.8 nm head-on entry as a synthetic SWINGING STIMULUS for the BFM
  roll-rate limiter: the closest entry that still holds a lock for the whole run. Read off it: peak roll
  rate against the cap the source declares (recursion limiter 1.37 ×, plant inversion 0.89 ×). TIMEOUT
  (exit 3) by design; the verdict is the roll trace. See §5.7 in [`../pilot.md`](../pilot.md).

- `sim/missions/mig29-radar-notch.fbm` — the N019's Doppler envelope against its own documented numbers.
  One MiG and two targets that beam at different RANGES, because the thresholds are range-dependent: the
  far one crosses beyond 8 nm (`notchMs=41.67` = 81 kt), the near one inside 5.4 nm (`notchMs=16.668` =
  32.4 kt), and both events carry the measured target radial velocity beside the threshold that rejected
  it. `RADAR_DROP … coastS=6` is the documented 6-second inertial track. TIMEOUT (exit 3) by design.
- `sim/missions/mig29-rwr-blind.fbm` — "using your radar blinds your RWR forward", with the emitter held
  constant. The MiG hears a radiating F-16 from the first sweep; at t = 40 s a GCI call is typed in and
  its third entry brings the N019 to ILLUM, at which point the SPO-15's forward hemisphere goes dark and
  the warning disappears — while the F-16's `fcr_on` never changes. TIMEOUT (exit 3) by design.
- `sim/missions/mig29-irst.fbm` — the KOLS, all three of its terms in one trace: a tail-on target seen at
  19.6 km and a 103°-aspect one not seen until 15.2 km (same type, same field — the aspect law), the
  laser stepping `irst_lock_nm` from −1 to 3.2 nm at its 6 km limit, and a fourth aircraft above a GFS
  cloud deck that is never detected at all (`irst_masked`). The first tactical effect weather has on a
  sensor in FlightBox. TIMEOUT (exit 3) by design.
- `sim/missions/mig29-intercept.fbm` — ground-controlled interception: the MiG starts SILENT, the
  controller's BRAA is typed in over three command-bus entries (8.0 s from call to radiating radar), the
  N019 finds the target one frame later, and the opposing RWR lights up 0.1 s after ILLUM. Then the
  generic intercept phase DISENGAGES, because this module composes no weapon — which is the phase
  machine refusing to pretend, and the reason `RADAR_DESIGNATE` is absent from the trace. TIMEOUT
  (exit 3) by design.

## State

| Item | State |
|---|---|
| Telemetry files | one CSV per unit; the primary actor keeps `telemetry.csv`, further units `telemetry_<callsign>.csv`, stores `telemetry_<callsign>_<type>_<n>.csv` |
| Event log | `events.log`, one greppable line per event, `t=SEC LEVEL tag EVENT key=val` |
| Attribution | `unit=` as the first field on every actor line, from N>1 onward; omitted at N=1 |
| `UNIT_RESULT` | emitted per actor from N>1 onward, with `decisive=` |
| Column order | append-only; new sources are always registered at the end (`units/FBSimUnit::StartTelemetry`) so no measured column ever loses its position |

## Gaps

| Gap | Detail |
|---|---|
| No schema version in the CSV | the header is the schema; a consumer must compare column names, there is no version field |
| `events.log` is not machine-typed | key=val is greppable and stable, but there is no declared event catalogue a tool could validate against |
| Append-only ordering has a visible cost | `blk_rwr`/`blk_cmds` stand at the end of the line instead of with the other `blk_*` — see [`avionics.md`](avionics.md) |
| No per-run manifest | the mission file, the elevation provider and the thread count are not written into the output directory |

## Knowledge

- **Why one file per unit and not one wide line.** A unit's column set follows its MODULE. A shared
  line would either force every module into one schema or make the header depend on the cast of the
  mission. The per-unit file also needs no special case at N=1 — the lines stay byte-identical to a
  single-unit run from before multi-unit existed.
- **Why attribution is omitted at N=1.** With one unit there is nothing to distinguish: the mission's
  lines ARE that unit's. Omitting the field is what keeps older regression baselines byte-identical.
- **Why new telemetry sources are appended at the end.** Registration order is column order. Inserting
  a source in the middle would shift every column to its right and invalidate every measurement ever
  recorded against a position.
- **Why `decisive=` exists.** With several units the run ends on one unit's verdict. Without the flag,
  a reader of N `UNIT_RESULT` lines could not tell which of them stopped the run.
