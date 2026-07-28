# Mission verdict and combat objectives

**Source of this file:** the former `doc/mission-format.md` (split in the Phase-3 mirror rebuild), sections
"Urteil — je Einheit, dann kombiniert" and "Kampfziele (`objective`)". Translated 1:1 from the German
original; no revision of content.

Syntax of the `objective` line: [`syntax.md`](syntax.md). Who owns the judges and how the runner
combines them: [`runtime.md`](runtime.md). The exit codes are in [`INDEX.md`](INDEX.md).

---

## Spec

### Two incorruptible instances per actor

Never seen by the module (CLAUDE.md "Kein Cheaten"):

- `core/FBFlightMonitor` — the physical K.O. (crash / loss of control). **The K.O. of ANY unit ends
  the run** (a departing airframe should not keep integrating in the background); RESULT names the
  unit (`unit=`), exit 2.
- `core/FBMissionMonitor` — the MISSION verdict, **one instance per unit WITH objectives** (non-empty
  flight plan OR at least one `objective` line; a unit with neither has nothing to achieve, gets no
  monitor and therefore does not appear in the verdict). It carries its OWN copy of
  `FBFlightPlan`/`FBRunway`/the `objective` lines from the mission file (never the module's own, live
  mutated one) and reads progress only from observed position resp. from an observed roster (below) —
  a module cannot report itself to SUCCESS and cannot give information about its opponent.

### The combination rule

The ONLY place where N verdicts become one (`missions/FBMissionRunner.cpp`):

| Overall verdict | Condition | Exit |
|---|---|---|
| CRASH / LOC | a unit has a physical K.O. **that was not the declared objective of another** | 2 |
| FAIL    | a unit with objectives has failed **decisively** (touchdown off the runway OR shot combat-ineffective without that being anybody's combat objective) | 1 |
| TIMEOUT | a unit with objectives did not reach its objectives before the timeout | 3 |
| SUCCESS | every unit with objectives whose loss was not the declared objective of another reached its objectives | 0 |

The run ends at the first DECISIVE failure (there is nothing left to prove), at the first physical
K.O. (no wreck keeps integrating in the background) and otherwise as soon as every unit with
objectives has a verdict.

### When a waypoint is "reached" — and what a leg is

Two consecutive `wp` lines are a **LEG**: the line from the previous to the active waypoint. It is
mission data, not interpretation — which is why the FIRST waypoint of a plan has none (there is no
declared approach path to it; the unit comes from wherever it happens to be).

That has two consequences, and both apply to the guidance AND to the verdict:

| | with a leg (from `wp` no. 2) | without a leg (`wp` no. 1) |
|---|---|---|
| Guidance | holds the LINE (`FBAutopilot::SetDirectLeg`, cross-track plus path angle against the ground track) | flies the BEARING to the point (unchanged) |
| "reached" | capture circle 500 m **OR** beyond the perpendicular through the waypoint (= passed) | capture circle 500 m only |

The second path solves a real case: a waypoint **inside one's own turn radius** can never be reached
by circle — the jet orbits it from outside, forever (`missions/test-wp-inside-turn.fbm` constructs
exactly that: WP1 lies 1,000 m beside WP0, the tightest circle of this jet at 300 kt has a 1,400 m
radius, so the closest approach is 1,000 m = twice the capture circle). Without the passage rule the
mission runs into TIMEOUT (exit 3); with it, WP1 is ticked off after 15.1 s as `by=passed` and the
route continues (exit 0).

That the rule is bound to the leg is the condition for the other use of the property to survive:
`bfm-basic.fbm`/`gun-turning.fbm` produce their defender's permanent turn DELIBERATELY with a single
waypoint at the centre of its turn — a first waypoint, therefore no leg, therefore neither path
control nor passage (re-measured unchanged: −58.2° bank, −5.25 °/s resp. −54.1° / −4.61 °/s).

`FBNavSystem::AdvanceWaypoint` (guidance) and `FBMissionMonitor` (verdict) apply the same geometry
**independently** — own plan copy, own computation, no call into the other. `events.log` says per line
which of the two paths took effect: `nav WP_REACHED … by=capture|passed` resp.
`mission WP_REACHED … by=capture|passed`.

### Combat objectives (`objective`)

Without an `objective` line the **flight plan is the entire verdict** — the original rule, unchanged.
An `objective` line makes this unit's objective explicit, and then the block is the **complete
statement**: the flight plan is only evaluated if it is in there as `objective waypoints`. That is not
a detail but the reason why an intercept may keep its briefed vector `wp` line without a decided
engagement running into a TIMEOUT at a never-reached vector point.

| Objective | Fulfilled when | Violated when |
|---|---|---|
| `survive` | still combat-capable at the END of the run — **never earlier**, see below | the unit is shot combat-ineffective → immediate FAIL |
| `waypoints` | its own flight plan is worked off (with `land`: standstill on the runway) | — |
| `kill unit <callsign>` | the named unit is combat-ineffective (`core/FBSystemHealth::CombatEffective`) | — |
| `kill team <team>` | EVERY unit of that team in the mission is (at least one must exist) | — |

SUCCESS means: all objectives fulfilled. **`survive` cannot be fulfilled early** — "still combat
capable" only becomes true when there is no run left in which one could be shot down (an opponent's
missile can still be in the air after he himself has died). A unit with `survive` therefore stays
without a verdict until the end of the run and is then evaluated: objectives met and still combat
capable → SUCCESS ("objectives met, survived"), otherwise TIMEOUT. A unit WITH `kill` but WITHOUT
`survive` thereby declares explicitly that its own loss is not a failure — a simultaneous shoot-down
of both sides is then a trade and not the failure of both.

The observation a `kill` objective is checked against is a **roster**: per non-weapon actor the
callsign, the team and the one bit that its own health register publishes (`FBUnitObservation`,
`core/FBObjective.h`). The runner builds it once per tick from the registers HE owns and shows it to
every monitor — no module is asked about its opponent and none about itself.

### Two teams with opposed objectives — a duel has a winner

The rule that turns two opposed verdicts into one is **a single one and it is declaration-based**, not
team- or "player-side"-based:

> The loss of a unit is **EXPECTED** if it was the declared objective of another — the unit is
> combat-ineffective AND another unit has declared a `kill` objective that names it (or its team). An
> expected loss is still reported as the FAIL OF THAT UNIT (`UNIT_RESULT`), **but does not decide the
> run** — neither as a mission FAIL nor, when the wreck later hits the ground, as CRASH. The overall
> verdict then comes from the remaining units.

With that a duel has a winner (SUCCESS) and a loser (FAIL) instead of twice FAIL. If both sides shoot
each other down (a trade), no verdict is decisive and the run reports the FAIL of the first unit —
nobody came home, and the line says so instead of inventing a winner. Missions without `objective`
lines know no expected loss and therefore combine exactly as before (re-measured: 132 of 132 output
files of the existing missions byte-identical).

If a run ends through an EXPECTED physical K.O., all still-open monitors get their verdict at that
point (the same evaluation as at timeout) — otherwise the shooter with a `survive` objective would
never have got one.

### Shoot-down as a mission verdict

If a unit loses its engine, its flight controls or its structure through a weapon hit
(`core/FBSystemHealth::CombatEffective`, see "Schadensmodell" in CLAUDE.md), its own
`FBMissionMonitor` closes with FAIL — "combat ineffective (weapon damage)" resp. "…(survive objective
lost)", depending on whether it declared objectives.

This is explicitly a MISSION verdict and not a physical one: the unit is not frozen and not marked, it
keeps flying as long as the physics allows, and the physics monitor then judges it like any other
aircraft (usually CFIT, when the wreck reaches the ground). In the `UNIT_RESULT` line the MISSION
verdict takes precedence over the later CRASH for a shot-down unit: the shoot-down explains the
impact, the impact explains nothing. An undamaged wreck (CFIT, departure) still reports CRASH/LOC.

Consequences for mission design:

- A unit WITH objectives ends the run as soon as it is shot down — unless its shoot-down was the
  declared objective of another; then the mission runs on until the shooter has its own verdict
  (`bvr-duel-decided.fbm`).
- A unit WITHOUT objectives carries no `FBMissionMonitor` at all and cannot end the run; its
  shoot-down is then observable until impact (`damage-amraam.fbm`, exit 2 = CRASH — this mission
  deliberately declares NO combat objective, because its subject is the 340 s of aftermath following
  the hit).

### Landing as the last waypoint

If a unit's flight plan ends on a `land` line (`FBWaypointType::Land`, always the runway threshold), a
different SUCCESS rule applies to THAT last waypoint than to a `wp`: not a simple capture-and-continue
but **standstill on the assigned runway** — gear with ground contact, groundspeed below ~2 kt,
position inside the runway footprint (0 m longitudinal, 15 m lateral margin). Merely overflying the
threshold at flying speed is not yet a landing. `pilot/FBPilot`'s phase machine supplies the
guidance for it — `Approach` (`FBAutopilot::Course`,
[`../modules/f16/navigation-ils.md`](../modules/f16/navigation-ils.md)) → `Flare` → `Rollout` — but
the VERDICT stays with the monitor.

## State

| Item | State |
|---|---|
| `core/FBFlightMonitor` | built; physical, module-agnostic, structural/gear truth from the pinned JSBSim model |
| `core/FBMissionMonitor` | built; own plan copy, waypoints, off-runway touchdown, timeout, combat-ineffective, objectives against the roster |
| Combination in the runner | built; the expected-loss rule is declaration-based |
| Waypoint passage rule | built and measured (`test-wp-inside-turn.fbm`, 15.1 s `by=passed`) |
| Landing standstill rule | built; margins 0 m longitudinal / 15 m lateral |

## Gaps

| Gap | Detail |
|---|---|
| Only four objective kinds | `survive`, `waypoints`, `kill unit`, `kill team` — no "protect", no time window, no area objective |
| `survive` produces TIMEOUT, not FAIL, when the objectives are unmet at the end | the run has no verdict class for "survived but achieved nothing" |
| Runway footprint margin is fixed | 0 m longitudinal / 15 m lateral; the mission cannot declare it, and the `runway` line carries no width |
| A trade reports the FAIL of the first unit | deliberate (no invented winner), but the line does not say "trade" as such |

## Knowledge

- **Why the mission monitor keeps its own plan copy.** The module mutates its flight plan while flying
  (waypoint advance). A judge reading that copy would be judging the module's own bookkeeping instead
  of the observed position. The two therefore apply the same geometry twice, independently, and
  `events.log` records which path triggered in each.
- **Why the expected-loss rule is declaration-based and not team-based.** A team-based rule would need
  a notion of "player side"; a declaration-based one needs only the `objective` lines already in the
  file. It also degrades correctly: a mission without `objective` lines has no expected losses and
  combines exactly as it did before the rule existed.
- **Why `survive` is evaluated only at the end.** "Still combat-capable" is not monotone during the
  run: an opponent's missile can be in the air after the opponent is dead. Evaluating early would let
  a unit bank a SUCCESS it can still lose.
- **Why the leg, and not the waypoint, carries the passage rule.** The passage test needs a direction
  to define "beyond the perpendicular". The first waypoint of a plan has no declared inbound
  direction, so the test has no definition there — and that absence is exactly what the BFM missions
  use to build a stable permanent turn out of a single waypoint.
