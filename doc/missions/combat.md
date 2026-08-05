# Mission data — combat: BFM, BVR intercept, pilot variants and the tournament

**Source of this file:** the former `doc/mission-format.md` (split in the Phase-3 mirror rebuild), sections
"Kampf-Missionen (`set task bfm`)", "Abfang-Missionen (`set task intercept`)" and "Piloten-Varianten
(`set pilot_*`) und das Turnier". Translated 1:1 from the German original; no revision of content.

This file is the **mission-author's view**: which `set` lines put a unit into a combat phase, what the
phase then does, and which telemetry columns carry the verdict. The pilot's control laws themselves
are in [`../pilot.md`](../pilot.md).

---

## Spec

### Combat missions (`set task bfm`)

A unit with `set task bfm` flies no waypoints but **BFM** (`pilot/FBPilot`'s Bfm phase): it regulates
against the **locked radar contact** and against nothing else. Everything it knows about the opponent
is built by `pilot/FBBfmTrack` from consecutive contacts (position plus estimated velocity vector);
registry, world truth and datalink tracks are unreachable for the pilot. A BFM mission therefore
mandatorily contains `set datalink off` (otherwise the sensor restriction would only be claimed) and an
auto-lock mode (`set fcr_mode acm_hud` or similar — CRM does not lock by itself).

Examples: `mods/f16/src/missions/bfm-basic.fbm` (pursuer 2 nm behind), `bfm-offset.fbm` (pursuer laterally
offset, aspect ~90° = angular disadvantage), `bfm-merge.fbm` (head-on merge, aspect 180°),
`bfm-blind.fbm` (offset merge — the pass BREAKS THE LOCK and shows extrapolation + search +
reacquisition).

**The phase employs TWO weapons since 2026-07-29** — the gun (`BfmGunfire`, the funnel) and a
**short-range infrared round** (`BfmMissileShot`), whichever is in parameters, at most one action per
decision tick and the gun first. The missile shot needs nothing new in the mission text: master arm
briefed, an IR round on a rail and the same auto-lock mode the phase already requires. Its five gates
and their derivation are [`../pilot.md`](../pilot.md) §5.11; what a mission author sees is that a
`set task bfm` unit carrying `aim9`/`r73` now fires them and that such a fight can end in exit 0 rather
than only in TIMEOUT (`mods/f16/src/missions/duel-merge.fbm`, `duel-merge-stern.fbm`).

**The search is target-motion aware.** It does not fly to the last MEASURED position (he is long gone
from there) but to the DATUM from `pilot/FBBfmTrack::Datum`: last vector propagated forward as long
as that prediction is worth more than the last look (up to 2/ω), uncertainty radius
`min(0.5·V·ω·t², V·t)`, and the weave width is exactly its angular width from here. The weave phase
begins at the START OF THE SEARCH instead of at the mission clock — otherwise the random moment of
contact loss decides the success. Measured over 16 merges of the same geometry (a sweep over the loss
time): before, 6 of 11 contact losses were reacquired (34/80/81/86/170/240 s, five times never); after,
11 of 11 (10…141 s, median 39 s).

**A fight has no waypoint objective — such missions end in TIMEOUT (exit 3), and deliberately so.** The
pursuer declares no objectives (no `wp`), the opponent flies its defined manoeuvre (in the four
missions: a SINGLE waypoint at the CENTRE of its turn, which the direct guidance cannot reach → a
stable permanent turn at the bank limit). That it is the FIRST waypoint is load-bearing and not
incidental: a first waypoint has no leg, therefore neither path control nor passage ticking (see "When
a waypoint is reached" in [`verdict.md`](verdict.md)), and the circle stays exactly the circle the
mission file describes. `core/FBMissionMonitor` then has nothing to judge and correctly says TIMEOUT;
the verdict about the fight is read by the analysis out of the telemetry (`bfm_*` columns below).

Additional telemetry columns (source `pilot/FBBfmTrack`, appended at the very end — existing columns
never shift): `bfm_pursuit` (`none`/`search`/`lead`/`pure`/`lag`), `bfm_valid` (estimate young enough
to pursue), `bfm_locked` (radar holds him NOW), `bfm_age` (s since the last real look), `bfm_rng` (nm),
`bfm_ata` (degrees off nose, + = right), `bfm_aspect` (degrees AT THE TARGET: 0 = exactly in his six,
180 = head-on), `bfm_hca` (heading crossing angle), `bfm_clos` (kt, + = closing), `bfm_es` (own energy
height = altitude + v²/2g, ft — the only energy figure that comes from OWN instruments), `bfm_gcmd`
(commanded g), `bfm_ctrl` (control position NOW), plus the three integrals
`bfm_engaged` / `bfm_lock_s` / `bfm_ctrl_s` (s) — lock retention rate and time in the control position
are therefore readable from the LAST line. Every one of these quantities is computable from the unit's
own perspective; anything that needs world truth (e.g. the TRUE aspect angle) belongs in the analysis,
not in the pilot.

### Intercept missions (`set task intercept`)

A unit with `set task intercept` flies an **intercept beyond visual range** (`pilot/FBPilot`'s
Intercept phase). The counterpole to `bfm`: BFM is flown with the NOSE and the lock never goes away —
an intercept is flown with the SENSOR, and the whole art is when one points it at what. The phase is a
small state machine of its own (`pilot/FBEngagement`, column `eng_state`):

| State | What the pilot does |
|---|---|
| `search` | Fly the briefed vector (the active waypoint IS the vector: bearing, range, altitude), in SEARCH mode, antenna elevation set to his own altitude band — and **do NOT lock**: a lock is a warning to the opponent. |
| `closing` | A contact stands on the scope: pursuit course, co-altitude with the contact, antenna centred on the reflector — still without a lock. |
| `attack` | Inside the briefed lock range: designate (TMS forward over the command bus), read the launch zone from the FireControl block, and shoot as soon as `range <= Rtr` AND the target lies inside the seeker acquisition cone. |
| `support` | A missile is flying: HOLD the lock (the uplink guides it) and **turn away to the edge of the antenna cone** (crank). Held until the time of flight predicted by the FCC — the seeker takes over earlier, but until impact there is no reason to fly towards the target. |
| `defend` | Somebody has a solution on this aircraft: **turn across the threat bearing** (90°, where one's own radial velocity is zero and a pulse-Doppler seeker cannot separate aircraft and chaff cloud) and throw countermeasures. Both over the command bus, after a human reaction time. |
| `abort` | Nothing left to shoot, or the fight has fallen below the intercept range: turn away cold. |

**When a threat warning demands an answer** is the core rule: a seeker on one's own aircraft (RWR mode
`missile`) always; a merely TRACKING radar (`track`) only when one's own attack has nothing left to
gain — the shot is away and needs no more guidance, or there never was one to take. Otherwise one loses
the engagement by turning away before one's own shot.

**And when it is resumed.** Once the threat is over (no more seeker, hold time expired) the pilot asks
exactly three instruments: weapons on the racks (Stores block), no BINGO in the warning block, a
radiating radar. If one is missing he disengages (`abort`) — that is the honest break-off condition.
Otherwise he goes back into `search`, and that then searches the DATUM of the last-seen opponent
instead of the briefed vector: heading, altitude band, antenna elevation and weave width all come from
`pilot/FBBfmTrack::Datum`. If he has never seen anything, the datum is invalid and everything stays
exactly the briefed vector. Measured on `bvr-duel.fbm`: before, both flew on along their vector after
the defensive manoeuvre and separated for 474 s out to 70.7 km, each with a missile aboard; now both
turn back (largest separation 55.6 km at t≈280 s), go back into `closing` at t≈355 s and into `attack`
at t≈365 s, and the second shot falls at t=527 s (43.6 m miss distance, defeated in the notch like the
first).

The numbers are a module matter (`modules/f16/FBF16Pilot`): lock from 16 nm, shot at Rtr, crank to 45°
(APG-68 gimbal angle 60° minus reserve), break-off below 5 nm, search mode = CRM. Generic (a pilot
property, not an aircraft one) remain the 1.0 s reaction time and the 0.5 s between two control actions
— both IN ADDITION to the bus latency of the respective control class.

Like BFM missions, intercept missions end in **TIMEOUT (exit 3)**, and for the same reason: an
engagement has no waypoint objective. The verdict stands in the LAST LINE of the `eng_*` columns
(source `pilot/FBEngagement`, appended at the very end — existing columns never shift):

| Column | Meaning |
|---|---|
| `eng_state` | see above |
| `eng_tgt_nm` / `eng_ata` / `eng_aspect` / `eng_clos` / `eng_locked` | the current geometry of the contact being worked (−1 = none) |
| `eng_detect_s` / `eng_lock_s` | **time to detection**: first firm contact, first lock |
| `eng_shot_s` / `eng_shot_nm` / `eng_shot_ata` / `eng_shot_aspect` | **shot range and geometry** |
| `eng_shot_rtr_nm` / `eng_shot_raero_nm` / `eng_shot_rmin_nm` | the launch zone AT THE MOMENT of the shot — a shot is only as good as the geometry it fell in |
| `eng_tta_s` / `eng_tti_s` | the two predictions of the fire control computer (to self-guidance, to impact) — against which the flown time of flight is measured |
| `eng_support_s` / `eng_support_f` / `eng_pitbull` | **was the guidance held to self-guidance** — seconds with a lock inside the support window, as a fraction of it, and the verdict at the end of the window |
| `eng_threat_s` / `eng_react_s` | **reaction time to the threat warning** (measured from the moment the warning DEMANDED an answer, not from the first symbol) |
| `eng_defend_s` / `eng_chaff` / `eng_shots` | seconds in the defence, chaff CARTRIDGES actually expelled (the CMDS set's count, not the count of switch throws), shots fired |
| `eng_es` / `eng_es_min` | **energy state over the course**: energy height now and its minimum since the start of the engagement |

Examples: `mods/f16/src/missions/bvr-intercept.fbm` (one-sided: a non-shooting target, the whole chain search →
detection → shot → guidance → hit), `bvr-duel.fbm` (**two-sided**: two AI jets, both armed, both with
RWR and countermeasures — nearly mirror-symmetric and therefore a stalemate; since the pilot returns to
the datum after the defence it is a stalemate WITH a second and third attempt instead of two jets
flying apart, which is why the timeout there is 700 s), `bvr-duel-decided.fbm` (the same pair, DECIDED:
6,000 m and 150 kt of energy difference, otherwise identical — the higher/faster one has the larger
Rtr, shoots first, and the other never gets to shoot; SUCCESS against FAIL), `bvr-defend.fbm` +
`bvr-defend-blind.fbm` (the defence pair: identical shot, ONE line of difference — `set rwr on|off` —
i.e. reacting versus non-reacting AI).

### Formation missions (`flight`, `set task formation`, `set brief_sort`)

A unit that carries a `flight <name> <position>` line ([`syntax.md`](syntax.md)) flies as part of an
ELEMENT, and three things change for it. All three are no-ops without the declaration, which is the
regression condition of the whole feature.

| Mission data | What it does |
|---|---|
| `flight <name> <pos>` | identity: position 1 is the lead. It makes the datalink's `fl` contact filter a statement rather than a mission-file ordinal, and it is what the sort, the station and the cover rule are all defined against |
| `set task formation` | the pilot starts in phase `Formation`: a wingman holds a combat-spread station on the LEAD's datalink report, the lead flies the mission's own route. Available on `f16` and on `mig29` — but the MiG has no cooperative terminal, so it has no lead report to hold and falls straight through to its own plan, which is the doctrine and not a gap |
| `set brief_sort left\|right\|near\|far` | the briefed sort CONTRACT: which end of the picture this member takes, measured against the FLIGHT's axis (the briefed vector). It is the only sort a flight without a shared picture has, and therefore the MiG-29's only sort. A flight WITH a shared picture never consults it |

Inside `set task intercept` a declared flight additionally sorts its targets from what the flight
collectively sees, and defers a shot that would leave the flight with nobody free. Fourteen `flt_*`
telemetry columns and the `flight SORT_ASSIGN` / `SORT_DROP` / `COVER_DEFER` / `SPLIT` events report
all of it. The rules, the derivations and the measurements are in [`../formation.md`](../formation.md);
the missions are `mods/f16/src/missions/pair-formation.fbm`, `pair-2v2-f16.fbm`, `pair-2v2-asym.fbm`,
`pair-cover.fbm` and `four-4v4-asym.fbm`.

### Pilot variants (`set pilot_*`) and the tournament

The decision numbers of an intercept are properties of the PILOT, not of the airframe. They stand as
defaults in the module (`modules/f16/FBF16Pilot`) and are overridable per unit as mission data:
`set pilot_<param> <value>` → `FBPilot::ApplyTuning` → `pilot/FBPilotTuning`. A **variant is
therefore a line in a mission file**, not a new class and not a new build. A parameter that is not set
stays the pilot's own number — a mission without a `pilot_*` line flies unchanged.

| Key | Band | What it decides |
|---|---|---|
| `pilot_speed_kt` | 150…900 | speed at which the intercept is flown (kt TAS) |
| `pilot_lock_nm` | 1…40 | range at which he designates — the lock is the warning to the opponent |
| `pilot_shot_rtr` | 0.1…3.0 | release at this multiple of Rtr (>1 = beyond Rtr) |
| `pilot_shot_ata_deg` | 1…60 | how far off-nose one still shoots |
| `pilot_shot_spacing_s` | 0…120 | spacing of two shots at the same target |
| `pilot_crank_deg` | 0…60 | how far the supported shot is turned away (gimbal limit 60°) |
| `pilot_abort_nm` | 0…40 | below this the intercept is over |
| `pilot_beam_deg` | 0…180 | defensive turn against the threat bearing (90° = pure beam) |
| `pilot_chaff_s` | 0.2…60 | dispense interval during the defence |
| `pilot_defend_hold_s` | 0…120 | how long the defence is held after the last warning |
| `pilot_react_s` | 0…30 | human reaction time to a threat warning (default 1.0 s) |
| `pilot_action_s` | 0.1…30 | one control action per this time (default 0.5 s) |
| `pilot_gun_burst_s` | 0.1…1.0 | length of ONE trigger press (default 0.5 s ≈ 50 rounds) |
| `pilot_gun_tol_frac` | 0.05…1.0 | how tightly the pipper is held before the shot, as a fraction of the funnel tolerance (default 0.35) |
| `pilot_bfm_ctrl_min_nm` | 0.05…5.0 | near edge of the control position in a turning fight |
| `pilot_bfm_ctrl_max_nm` | 0.05…10.0 | …and its far edge — a gun position lies IN the funnel (600…3,000 ft), a missile position outside |

Both — reaction and action time — remain IN ADDITION to the bus latency of the respective control class
(`core/FBCommandBus`); no variant can answer faster than the jet allows. An unknown key or a value
outside the band is a runtime FAIL like any other bad `set` line — a mistyped tournament number does
not silently fly the default.

**The tournament runner** is a script, not a build target: `sim/tools/fb_tournament.py` (stdlib
Python). From a variant list (`sim/tools/variants-bvr.txt`) it writes a `.fbm` file for every pair and
**both side assignments**, runs them over `fb-gym --threads N` and evaluates telemetry plus
`UNIT_RESULT`. The fitness dominates on outcome (kill +1000, own loss −1200, landed hit +150, never
shot −250) and only below that orders by craft (shot geometry inside the launch zone, support fraction,
shot lead, defence, energy) — the craft sum can never turn an outcome around within a run. The output
names BOTH sides per pairing with their individual items; the ranking separates `outcome` from `craft`.

```
sim/tools/fb_tournament.py --variants sim/tools/variants-bvr.txt --out /tmp/t --geometry split \
    --threads 2 --check-determinism
```

`--flight N` (1, 2 or 4) turns each side into an ELEMENT of N flying one doctrine, with the objective
changed from `kill unit` to `kill team`; a variant line then also accepts `dl=on|off` (does the
element use its cooperative net) and `sort=<contract>`, so a FLIGHT doctrine is a text line too.
`--flight 1` writes byte-identical missions to the previous script (120 pairings compared, 0 differ),
so `variants-bvr.txt` and `variants-mixed.txt` are unaffected. See
[`../formation.md`](../formation.md) for the measured ranking.

## State

| Item | State |
|---|---|
| `set task bfm` | built; own control law on manual stick, datum-based search, thirteen `bfm_*` columns plus three integrals |
| `set task intercept` | built; seven-state engagement machine, sixteen `eng_*` columns |
| `set task attack` | built — see [`weapons.md`](weapons.md) |
| `set pilot_*` | built; sixteen keys with bands, unknown key or out-of-band value = runtime FAIL |
| Tournament | `sim/tools/fb_tournament.py`, a script — no build target, no production path |
| `flight` / `set task formation` / `set brief_sort` | built; see [`../formation.md`](../formation.md) for the contract, the numbers and the gaps |

## Gaps

| Gap | Detail |
|---|---|
| Combat missions have no verdict of their own | BFM and intercept missions end in TIMEOUT (exit 3) by construction; the verdict is read out of telemetry, not out of the exit code |
| The opponent's permanent turn is a construction | it comes from a single unreachable first waypoint, not from a declared manoeuvre |
| `pilot_*` covers the intercept and the gun, not everything | the attack phase has two of its own (`pilot_attack_bias_s`, `pilot_attack_ccip_m`); other phases have none |
| Reaction and action time are not aircraft hooks | they describe the pilot, deliberately, so they cannot be tuned per airframe |

## Knowledge

- **Why BFM and intercept are opposites.** BFM is flown with the nose against a lock that never goes
  away; an intercept is flown with the sensor, and the decision is when to point it at what. That is
  why one is a control law on the manual stick and the other a state machine on direct guidance.
- **Why the datum and not the last measured position.** The last look is where he WAS. The datum is the
  centre of the region he can be in now (last vector propagated, but only to 2/ω, where the turn error
  is as large as the whole displacement), its radius is `min(0.5·V·ω·t², V·t)`, and the weave width is
  its angular width from here. Measured: reacquisition went from 6/11 to 11/11.
- **Why the weave phase is anchored at the start of the search.** Anchored to the mission clock, the
  random moment of contact loss would decide where in the weave the search begins — and therefore
  whether it succeeds.
- **Why a lock is issued as late as possible.** A lock is a warning to the opponent. `search` and
  `closing` therefore run without one; only the attack state designates.
- **Why a variant is a text line.** With `FBPilotTuning` as a table of {set?, value} read only through
  `Tuned(param, own_value)`, a population of variants is a set of mission files instead of a set of
  classes — and the tournament runner writes missions instead of code.
- **Why fitness dominates on outcome.** Craft (shot geometry, support fraction, energy) may order
  within equal outcomes but must never turn an outcome around; otherwise a well-flown loss could
  outrank a scrappy kill.
