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

A **third** ground, `orbited`, is bound to the other end of the chain — the SUCCESSOR instead of the
predecessor, so it exists on `wp` no. 1 as well, but never on the last waypoint of a plan:

| | with a successor | last waypoint of the plan |
|---|---|---|
| "reached" | additionally: two failed approaches to the fix (closest approach, opened by more than the capture radius, closed again, opened again) = `orbited` | unchanged — a fix with nothing after it is the route's destination, and "still trying" is the correct state |

This solves the case a capture circle cannot: a fix the aircraft cannot close **because of the wind**.
The capture circle is a GROUND test of fixed radius while the circle the aircraft can fly lives in the
air mass. [MESS] `missions/wx-orbit.fbm` — at 9,000 m in 18.6 m/s of crosswind the closest approach to a
steerpoint dead ahead is 614 m (114 m outside the circle), followed by a permanent limit cycle at −59.1°
of bank, 99.2 s per lap; the same file in calm air captures the same fix with 4 m to spare. With the
rule the fix goes `by=orbited` at t = 311.6 s and the route finishes at t = 485.4 s (exit 0). The
derivation, the threshold of two and why it is bound to the successor: `doc/systems.md`, section 7.5.1.

That the rule is bound to the successor is again what lets the deliberate use of the property survive:
`bfm-basic.fbm`/`gun-turning.fbm` declare their defender's turn as a SINGLE `wp` — no predecessor and no
successor, therefore neither `passed` nor `orbited` (re-measured unchanged: bandit `activeWp` stays 0 for
the whole run, −58.9° resp. −57.7° of bank, range to the fix 1,217…2,428 m, zero `WP_REACHED` lines).

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
**independently** — own plan copy, own computation, own approach record, no call into the other.
`events.log` says per line which of the three paths took effect:
`nav WP_REACHED … by=capture|passed|orbited` resp. `mission WP_REACHED … by=capture|passed|orbited`.

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

### The four new objective kinds (`C12`) — **built**

Five of the ten campaign specs cannot state what they measured
([`../campaigns/INDEX.md`](../campaigns/INDEX.md)): W5 and O2 have no *identify* and no *do-not-fire*,
O5's entire success condition is **something not happening**. This is the contract that closes it;
the grammar is in [`syntax.md`](syntax.md).

#### The rule that constrains every candidate

> **A judge measures what the aircraft DID, never what it knew.** Every existing objective obeys it — a
> waypoint is a position, a `kill` is a health bit. A new kind is admissible only if it is checkable
> against **observed** facts: this unit's own sample, its private plan copy, and the roster the OWNER
> fills from registers it holds itself.

That is the whole test, and it is what decides the shape of `identify` below.

#### A SIXTH kind, `suppress` (`C26`) — **built 2026-07-28**

```
objective suppress unit <callsign> [emitting <s>]      # default 0
objective suppress team <team>     [emitting <s>]
```

| | Rule |
|---|---|
| Fulfilled when | the named unit's **cumulative radiating time over the run** is ≤ `<s>` |
| Violated when | — it is not a FAIL condition; it is simply unmet |
| Decided | in `Finalize` — **deferred**, because a position that has gone quiet can still come back on. It joins `HasDeferredObjective()` and moves nothing for a mission that declares none |
| `FBObjectiveCovers` | **false**, like every non-`kill` kind: wanting a position QUIET is not declaring that it should die |
| Roster price | **one bool**, `FBUnitObservation::Emitting`, filled by the OWNER from the signature the unit publishes at the barrier (`Sig_.Radar[b].Mode != None`) — the identical construction `CombatEffective` and `ReleasedWeapon` already use. It is the FIRST NON-MONOTONE roster field, and that is safe because the judge's ACCUMULATOR is monotone, which is what the rule needs |
| Telemetry | **none new.** A position already publishes `site_beam0`/`site_beam1` and the dwell is reconstructible from them |
| Events | `mission SUPPRESSED` (with the seconds and the allowance) / `mission SUPPRESSION_LOST` |

**It passes the rule above:** radiating-or-not is an observed fact about a published signature, the
same class of fact as a health bit, and the declaring unit is never asked what it heard.

**Destroyed implies suppressed, and it is not a special case:** `Radar` failed → the block goes Invalid
→ `Emission()` returns `None` → the bit is false forever. The implication falls out of
[`../weapons.md`](../weapons.md) §8's coupling and cost nothing to write.

**The false positive is named rather than hidden:** a position that was never woken scores a
suppression nobody earned — the exact dual of `deny release` scoring a jettisoning striker as denied.
The reading rule is the mission's: `suppress` is only a result when it is paired with something the
attacker DID, and the two proof files (`suppress-quiet.fbm`, `suppress-killed.fbm`) both say so in
their headers.

#### The four kinds

| Objective | Fulfilled when | Violated when | Decided |
|---|---|---|---|
| `identify unit <callsign> range <m> hold <s>` | this unit has held a planar range ≤ `<m>` to the named unit for a cumulative `<s>` | — | latches on fulfilment |
| `protect unit <callsign>` / `protect team <t>` | the named unit(s) are still combat-effective at the END of the run — at least one must exist | any named unit goes combat-ineffective → immediate FAIL of THIS unit | `Finalize` |
| `no_fire` | this unit released no weapon and fired no gun burst for the whole run | any release or burst → immediate FAIL | latches on violation, confirmed at `Finalize` |
| `deny release unit <callsign>` / `deny release team <t>` | the named unit(s) released nothing for the whole run | — | `Finalize` |
| `avoid zone <name> [exposure <s>]` | this unit's cumulative dwell inside the DECLARED cylinder `<name>` stays at or below `<s>` (default 0) | — | `Finalize` (a zone can still be entered) |

#### What each one costs the roster — and where the honesty line runs

The roster is `FBUnitObservation { Id, Team, CombatEffective }` per non-weapon actor
([`../core.md`](../core.md) §5.5). Two of the four need nothing; two need exactly one field each, and
both new fields are filled by the OWNER from facts the owner already holds — never asked of a module.

| Kind | Needs | Verdict |
|---|---|---|
| `protect` | **nothing.** It is the exact dual of `kill`: the same bit, read the other way | free. This is why it is first |
| `avoid zone` | one **monotone dwell** per declared zone, from the judge's own copy of the geometry and the observed position — the identical currency `identify`'s planar range already uses. **Roster cost: nothing**, it asks about the declaring unit's own sample, like `waypoints`. `FBObjectiveCovers` returns false for it, like every non-`kill` kind (round `C23`, [`../air-defence-network.md`](../air-defence-network.md) §4) |
| `no_fire` | one **monotone bit**, `ReleasedWeapon` | the runner already drains `Stores().TakeRelease()` and `Guns().TakeBurst()` itself in the growth phase ([`runtime.md`](runtime.md) §7 step 11). The bit is a by-product of a loop that exists |
| `deny release` | the **same bit**, pointed at another unit | one bit buys both, which is why they are specified as a pair rather than as two mechanisms |
| `identify` | one **float**, `RangeM` — the planar range from the judged unit to that roster entry, per tick | the owner computes ranges between published poses already (`ClosestApproach`, the CPA resolution). Same currency, same truth |

**Net cost of the whole vocabulary: one monotone bit and one float on `FBUnitObservation`.** Both are
observations of the same kind the health bit already is. Nothing else about the judges changes.

**One correction from building it.** The price above counts the ROSTER, and the roster answers "what
about the OTHERS". `no_fire` asks about the declaring unit itself, and the monitor does not know which
roster entry it is — it has no id, deliberately. So the same bit also travels on
`FBMissionMonitorSample`, next to `CombatIneffective`, which is the identical construction the health
bit already uses (`FBSimUnit::BuildMissionSample` fills both from registers the owner holds). One bit,
two views, no third mechanism — but it is a field more than the estimate said, and the estimate was
counting the wrong struct rather than the wrong number.

**One rule the price table did not name.** Three of the four are "deferred" in the same sense `survive`
is: `protect`, `no_fire` and `deny release` cannot be banked early, because none of them is monotone
until the run is over (the protectee can still be hit, the trigger can still be pulled). The SUCCESS
gate in `Tick` therefore reads `!HasDeferredObjective()` where it read `!HasSurviveObjective()` — for a
mission that declares none of the new kinds the two are the same predicate over the same objectives,
which is why nothing moved. `identify` is NOT deferred: a dwell that has been flown cannot be un-flown,
so it latches the tick it completes.

#### `identify` — the design decision, and the one that was rejected

Two designs exist and they are not equivalent.

| | **(A) the judge measures the GEOMETRY** — recommended | (B) the judge measures a SENSOR EVENT |
|---|---|---|
| Check | planar range between two published poses, held for a declared dwell | "this unit obtained a discriminating event on unit N" |
| Who supplies it | the owner, from the same poses the CPA runs on | the owner, by correlating an anonymous contact back to a unit |
| Anti-cheat | **safe by construction.** It asks nothing of any module and needs no sensor at all | **hazardous.** Producing the observation means building a contact→unit correlation function. Even though the judge is allowed to know the truth, that function's *existence* is the identity leak the architecture is shaped to prevent |
| What it measures | the ACT — you flew the pass | the RESULT — you saw it |
| Cost of the difference | a pilot that flies the box with its eyes shut still scores | — |

**Built as (A), and the stated cost is paid.** The sensor half is not lost — it is read out of
`events.log` (`iff IFF_REPLY`, `vis RECOGNISED`, `vis IDENTIFIED`), which is how every combat mission in
the tree is already read ([`INDEX.md`](INDEX.md) rule 5: the file's header comment carries the binding
reading rule). A verdict that cannot be cheated plus a measurement that can be grepped beats one
verdict that can be cheated.

**Aspect is deliberately left out.** A real identification pass is abeam, not astern, and the bearing
would be free from the same poses. It is not taken because nothing in W5's sources gives an aspect
(§Knowledge 2 of that file already marks the abeam box `[SET]`), and a second `[SET]` number multiplies
the arbitrariness without adding a measurement. Named as a one-line extension if a source turns up.

#### What is deliberately NOT in the vocabulary

| Candidate | Verdict |
|---|---|
| **`escort`** | **not a kind.** It is `protect unit X` plus a station requirement, and the station requirement is exactly `identify`'s geometry test with a long dwell. Two lines express it; a fifth kind whose check is the union of two existing checks earns nothing |
| **A general `deny`** — "the opponent did not achieve his objective" | **not checkable and not invented.** It would require one monitor to read another's verdict, and a judge consulting a judge is not a judge. `deny release` is the checkable part of what O5 means; `protect` is the rest |
| **Time windows** (`before <s>` / `after <s>`) | deferred. A window is a *modifier* on any kind, so it multiplies the grammar by the number of kinds — and no campaign mission in the set needs it as a **verdict**. O5's time-on-target slip is a measurement out of telemetry, not a pass/fail |
| **`no_fire` with an exception** ("unless fired upon first") | deferred with a reason: "fired upon" is an RWR *warning*, not a fact, and making a verdict depend on what a unit heard breaks the rule at the top of this section. W5-10 declares the exception in its header as a reading rule instead |
| **Target priority / value** | that is `C15`/formation work, not a verdict |

#### What the vocabulary still cannot say, stated plainly

O5 asks for three things in descending order of value: the ordnance did not reach the target, the
package's timing was broken, the defender survived. After `C12` the vocabulary expresses **the first
and the third**. The second is still a telemetry read, and `o5-10`'s "three numbers, no single verdict"
remains the correct form for that mission. A jettisoning striker counts as having released, so
`deny release` scores it as a failure of the denial — a real false negative, named rather than hidden.

#### The conservation rule, and how it is enforced

> **A mission without a new line judges byte-identically to today.**

Not hoped for — structural, on five separate counts:

| Mechanism | Effect |
|---|---|
| Every new spelling is a NEW token | `objective kill unit X` parses and behaves exactly as before |
| `PlanJudged_ = Objectives_.empty() \|\| HasObjective(Waypoints)` is **unchanged** | the new kinds do not touch whether the flight plan is judged, so no existing verdict moves |
| The new roster fields default to "nothing happened" (`ReleasedWeapon = false`, `RangeM = +inf`) and are read **only** by the new kinds | a mission with no new objective never reaches the new branches |
| `FBObjectiveCovers` returns false for all four | **the load-bearing one** — see below |
| The SUCCESS strings are extended, never rewritten | the existing sentences (`"all waypoints reached"`, `"stopped on the runway"`, the `", objectives met"` suffix) are in every measured `events.log` and stay byte-for-byte |

**The `FBObjectiveCovers` rule, spelled out because missing it silently breaks every duel verdict.**
"Covers" means *somebody declared this unit's loss as their objective*, and that is what makes a loss
EXPECTED and therefore non-decisive. `Survive`, `Waypoints`, `Identify`, `Protect`, `NoFire` and
`DenyRelease` **never** cover a unit; only `KillUnit`/`KillTeam` do. A `protect` declaration must not
make the protected unit's loss expected — it is the exact opposite of a declaration that it should die.

**And it is measured as an exit code, not as a reading of the source.**
`missions/objective-covers-none.fbm` is built for nothing else: a defender that names the striker twice
(`identify` + `deny release`) and declares no `kill`, a striker that declares `survive` and is then shot
down. Its loss is covered by nobody, so its own FAIL decides the run — exit 1 at t=70.7 with
`decisive=1` on the STRIKER's `UNIT_RESULT`. **Counterfactual, run:** with `FBObjectiveCovers` patched
to return `FBObjectiveNames` for the C12 kinds, the same file exits **0** ("objectives met, survived") —
the loss becomes expected, the run runs to the timeout and the defender is credited with a denial it
only achieved because the unit it was denying is dead. One line of source, two verdicts.

The same counterfactual over `escort-protect-lost.fbm` returns **exit 1 either way**, and that is worth
recording rather than hiding: inside a single mission a wrong `protect` cover is NOT observable, because
the protector's own immediate FAIL is decisive whatever the protectee's loss counts as. `protect`'s half
of the rule is therefore carried by the exhaustive `switch` (a new kind that forgets a case does not
compile, `-Werror=switch`) plus the two kinds that CAN be isolated, not by a mission of its own.

**Acceptance, measured (round `C12`):** the regression gate at full strength against the pre-round
binary — **260/260** `telemetry*.csv` of the **85** pre-round `sim/missions/*.fbm` byte-identical and
**85/85** `events.log` identical modulo `wallS`/`speedup`/`--out` path, at `--threads` **1, 2 and 4**
(three full passes, all three identical). The eight new missions are deterministic over the same three
thread counts (285/285 telemetry files, 93/93 logs between the 1- and the 4-thread pass). Same bar as
`C2`, and green for the same reason: nothing new is written unless something new is declared.

[`../core.md`](../core.md) §5.5 carries the four `FBObjectiveKind` values, the `FBObjectiveScope`
discriminator and the two new `FBUnitObservation` fields; that file is the type reference.

#### The eight missions the round left behind

Seven of them are two files apart by ONE number, which is the tree's usual way of turning a rule into a
measurement; the eighth is the cover proof above.

| Mission | Exit | What it measures |
|---|---|---|
| `qra-identify.fbm` | 0 | the pass: inside the 2,000 m box from t=100.8 to t=151.5, closest approach 454.0 m, `mission IDENTIFIED` at t=130.8 with `heldS=30.1`; `no_fire` held to the timeout. The sensor half is beside it in the log (`radar IFF_REPLY … reply=none`, t=1.9) and is deliberately NOT the verdict |
| `qra-identify-tight.fbm` | 3 | the same file with `range 300`: the parallel tracks are offset 500 m, so the box is never entered, zero `IDENTIFIED` lines, TIMEOUT |
| `qra-weapons-hold.fbm` | 1 | the same intercept, armed, briefed pickle at t=20: `sms RELEASE` t=20.4 → FAIL "weapon released (no_fire objective lost)" at t=20.6, 110.2 s before the identification would have latched |
| `escort-protect.fbm` | 0 | CAP kills the inbound striker at t=54.3, 17.7 s before its CCIP release point; the installation lives; `protect` answered in `Finalize` |
| `escort-protect-lost.fbm` | 1 | the same file with the CAP's pickle 15 s early — refused as `out_of_context` ("target beyond Raero", 31.6 km vs 26.9 km), so the striker releases at t=72.0, the depot dies at t=82.3 and the CAP FAILs at t=82.4, `decisive=1` |
| `deny-release.fbm` | 0 | the striker is killed at t=70.6; its own briefed pickle at t=90 is refused by its own SMS (`CMD_ACK … reason=system_failed`) — the denial is earned inside the simulation |
| `deny-release-broken.fbm` | 3 | the same file with the striker's pickle at t=15: the kill still succeeds, `survive` still holds, and the run is still not a success. This is the one thing `kill` cannot say |
| `objective-covers-none.fbm` | 1 | the cover rule, above |

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
| The four new objective kinds (`C12`) | **built.** `identify`/`protect`/`no_fire`/`deny release` in `core/FBObjective.h` + `core/FBMissionMonitor`, the two roster fields filled by the runner (`missions/FBMissionRunner.cpp`) and by the browser loop; eight missions, one per case; the pre-round tree byte-identical at `--threads 1/2/4` |
| `FBObjectiveCovers` for the C12 kinds | built as an exhaustive `switch` returning false, and measured as an exit code (`missions/objective-covers-none.fbm`, 1 correct / 0 under the patched counterfactual) |
| The sixth kind, `suppress` (`C26`) | **built.** `FBObjectiveKind::Suppress` + `FBMissionMonitor::NoteEmitting` (a monotone accumulator over a non-monotone roster bit), deferred like `survive`; two missions, one per outcome; the pre-round tree byte-identical at `--threads 1/2/4` |

## Gaps

| Gap | Detail |
|---|---|
| Still no time window and no area objective | deliberate, see above. A window is a modifier on every kind, so it multiplies the grammar; O5's time-on-target slip stays a telemetry read |
| Nothing out of the C12 refusal list moved | a general `deny`, `escort`, target value, `no_fire` with an exception — each still refused for the reason in the Spec, not for lack of time |
| A **ninth** kind is specified elsewhere and not built | `objective suppress unit\|team [emitting <s>]` — [`../air-to-ground.md`](../air-to-ground.md) §5.2 (`C26`). It passes this file's own test (radiating-or-not is an observed fact about a published signature, the same class as the health bit), it is **deferred** like `protect`, `FBObjectiveCovers` returns false for it, and its roster price is **one bool** (`Emitting`, filled by the owner at the barrier) — the first non-monotone roster field, with a monotone accumulator in the judge. Its false positive is named there: a site nobody ever woke scores a suppression nobody earned, so the kind is only a result when the mission pairs it with something the attacker did |
| `identify` carries no ASPECT | a real identification pass is abeam, not astern, and the bearing would be free from the same poses. Left out because no source in the set gives an aspect (a second `[SET]` number would multiply the arbitrariness). One-line extension when one turns up |
| `no_fire` does not reject a trailing token | `objective no_fire tomorrow` parses as `no_fire`, exactly as `objective survive tomorrow` has always parsed as `survive`. Making one keyword strict would be an asymmetry; making all of them strict is a separate, format-wide decision |
| A `protect` cover cannot be isolated in one mission | measured, see the counterfactual above: the protector's own FAIL is decisive whatever the protectee's loss counts as. The rule is held by the exhaustive `switch` and by the two kinds that can be isolated |
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
- **Why the objective vocabulary is bounded by the roster and not by what a mission would like to say.**
  Every kind that exists is a comparison against something the OWNER observes: a position, a health bit.
  The moment a kind needs something else, the honest move is to say which field it needs and let that be
  the price — `identify` needs one float, `no_fire`/`deny release` share one monotone bit. A kind whose
  price is "the judge reads another judge" or "the judge correlates an anonymous contact" is not
  expensive, it is inadmissible, and it is refused rather than approximated (`C12` Spec above).
- **Why the leg, and not the waypoint, carries the passage rule.** The passage test needs a direction
  to define "beyond the perpendicular". The first waypoint of a plan has no declared inbound
  direction, so the test has no definition there — and that absence is exactly what the BFM missions
  use to build a stable permanent turn out of a single waypoint.
