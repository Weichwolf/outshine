# Formation — the flight as a fighting unit

**Source state:** commit `9b526da` plus this round. Like [`duels.md`](duels.md), the subject is
neither a class nor a directory: a flight cuts through `core/` (the identity), `units/` (what it
publishes), `sensors/` (the channel that carries it), `pilot/` (the decisions it changes) and
`missions/` (how it is declared). Putting it inside [`pilot.md`](pilot.md) would bury the datalink
half of it; putting it in [`sensors.md`](sensors.md) would bury the pilot half. It therefore gets its
own file, on the precedent `duels.md` already set — and the mirror rule of
[`INDEX.md`](INDEX.md) keeps its exception list honest by naming it.

| Place | Role |
|---|---|
| `sim/src/core/FBFlight.h` | `FBFlightId` (name + position) and `FBFlightReport` (what a member tells its flight) |
| `sim/src/core/FBDatalinkTrack.h` | the PPLI's flight half: the sender's flight, position and report |
| `sim/src/sensors/FBDatalinkSystem.cpp` | fills it, and the `fl` filter index that is now a declaration |
| `sim/src/pilot/FBFlightPicture.h` / `.cpp` | the shared picture, the assignment, the cover state, the `flt_*` channels |
| `sim/src/pilot/FBPilot.cpp` | phase `Formation`, the wingman's station, the sort in the intercept, the cover deferral |
| `sim/missions/pair-*.fbm`, `four-4v4-asym.fbm` | the measurement rigs |
| `sim/tools/fb_flight_report.py`, `sim/tools/variants-flight.txt` | the analysis tool and the flight tournament |

Convention: **[MESS]** = measured in this campaign, **[HERL]** = derived from a named relation,
**[SETZ]** = a declared setting.

---

## Spec

Before this round a "flight" was an appearance. `fl` in the datalink's contact filter meant "the first
unit of that faction in mission order"; two jets of one side flew two private wars, could prosecute
the same target while a third went unengaged, and nothing in the tree could say so. This is the
contract that replaces it.

| Contract | Acceptance / measurement anchor |
|---|---|
| A flight is DECLARED, not inferred | a `.fbm` unit block carries `flight <name> <position>`; position 1 is the lead. A flight without a position 1 is a parse error, and two units cannot share a position |
| Without a declaration nothing changes | every piece of flight behaviour is a no-op: all 79 stock missions byte-identical on every column they ever had, the 14 `flt_*` columns appended and inert |
| The `fl` datalink filter becomes a statement | the filter index is the DECLARED position where one exists, and the old mission-file ordinal where none does |
| A wingman holds a station on a MOVING point | and the point is a MESSAGE with an age, not a position. Acceptance: `pair-formation.fbm` holds it on straight legs and returns to it after a 90° turn, without a standing offset and without a pursuit oscillation |
| The station's two channels are separate | across and vertical through the existing path law (`FBAutopilot::SetDirectLeg`), along-track through the throttle. That separation is what excludes the pursuit oscillation, and it is the reason the law introduces no new gain, time constant or integrator |
| A target is not engaged twice while another is free | measured per tick per unit as `flt_dup && flt_free > 0`; the acceptance is **zero**, over every unit of every formation mission |
| Nobody is left unengaged while a shooter is spare | the assignment is a minimum-cost matching over ALL members before anybody doubles up |
| The assignment rests only on what the sensors delivered | the picture is built from `FBState`'s Datalink and Radar blocks and nothing else. A mate reports a target POINT, never an identity — this radar does not know whom it sees, so it cannot tell anybody |
| A flight without a channel sorts by CONTRACT or not at all | the MiG-29 has no cooperative terminal; its only sort is the one briefed before takeoff, applied by each pilot to the picture he personally has. **No track list, no identity feed** ([`modules/mig29/datalink-gci.md`](modules/mig29/datalink-gci.md) §5.3) |
| The flight keeps ONE member free | a member does not fire a round that would bind it while a mate is already bound. Identical rule on both airframes; the PRICE is the weapon's own obligation, so it is nearly free for an AIM-120 and expensive for an R-27R |
| Every flight quantity is measurable from OWN perspective | 14 `flt_*` telemetry columns plus `flight SORT_ASSIGN` / `SORT_DROP` / `COVER_DEFER` / `SPLIT` events |
| A flight doctrine is mission text, not a class | `dl=` / `sort=` on a tournament variant line; the flight tournament runs element against element with a fitness written before flights existed |
| Every run is deterministic | one fingerprint over `--threads 1/2/4` × 3 repeats per mission |

### Round `F5` (2026-07-31) — the shape becomes mission data

**Added before the round's first line of code.** The table above is untouched; this section adds three
contracts. It exists because [`doctrine-evolution.md`](doctrine-evolution.md) `E-20` MEASURED what F5
costs: `pilot_flight_shape` is not a key at all, `set pilot_flight_shape 1` is rejected at t = 0.0 and
the run exits 1, so the owner goal's *Verband* gene has never been reachable — and with G5 also blocked,
two of the five growths the goal names are not keys.

| # | Contract | Acceptance / measurement anchor |
|---|---|---|
| **F5a** | **A shape is briefed as DIMENSIONLESS RATIOS, never as metres.** Three `Scale` keys — `pilot_flight_spread_frac`, `pilot_flight_trail_frac`, `pilot_flight_stack_frac` — each a multiple of the airframe hook it names. §2.2's boundary is unchanged and there is no new syntax: a mission cannot write a distance into a formation any more than it can write one into an energy rule | `fb-gym --pilot-keys` prints three more `scale` rows with their hooks; `static_assert(ScaleBandsAreDimensionless())` still compiles, and a key carrying a metre value does not |
| **F5b** | **`FormationTrailM` stops being 0, and that is F5's OWN defect rather than a convenience.** F5 names it: *"a four-ship is four abreast rather than two elements in trail"*. The new default is **one spread aft** — no new number enters the tree, it is `FormationSpreadM` measured along the course line instead of across it | [DERIVED] `FormationTrailM() = FormationSpreadM() = 1852 m`. The blast radius is DECLARED in advance and is exactly the missions that fly a third flight position: **14 files, 13 of them campaign rungs**. Every other mission stays byte-identical on every column |
| **F5c** | **The old shape stays REACHABLE, and is one brief away.** Line abreast is `pilot_flight_trail_frac = 0`, so nothing measured before this round becomes unrepeatable — it becomes a named doctrine instead of the only one | a run with `set pilot_flight_trail_frac 0` reproduces the pre-round telemetry of an affected mission BYTE-IDENTICALLY. That is the round's own regression control |

**What this round may NOT do.** It may not make the three keys genes of a *different* shape than the one
the goal names: the gene is the formation the flight is briefed, not a new airframe number. And the
default multiplier of `Scaled()` is 1.0, so a hook that is 0 makes its key degenerate at one rail —
which is exactly [`duels.md`](duels.md) D3's trap, and F5b exists so this file does not walk into it.

---

## State

**Built and measured this round.** Five missions, one new pilot component, one new core value type, one
datalink payload extension, one analysis tool and a tournament mode. Nothing scripted: every jet
decides from its own blocks.

### The regression condition, first

The rule the whole round is built against is that a mission which declares no flight must fly exactly
as it did. **All 79 stock missions are byte-identical on every column that existed before, and their
`events.log` is identical line for line** (paths and wall-clock fields normalised). The 14 `flt_*`
columns are appended at the end and inert: across all 79 runs they take exactly two distinct values,
differing only in `flt_bound` — which is 1 for a jet that has a round in the air and is the one flight
quantity that has a meaning without a flight.

### What each mission measured

| Mission | exit | What it proves | The number |
|---|---|---|---|
| `pair-formation.fbm` | 0 | the station law, in isolation: no opponent, no weapons, no radar | straight-leg station error **45.2 m median / 84.4 m max** (final leg, 3,400 ticks); through each 90° turn **1,936.7 m** and **1,909.6 m** peak, both recovered; overall median **63.5 m** over 8,999 ticks |
| `pair-2v2-f16.fbm` | 3 | the symmetric reference case: two F-16 elements, everything else equal | the cooperative pair holds DIFFERENT targets in **1,036 of 1,109** ticks in which both were assigned = **93 %**; `dup && free` **0** for all four units |
| `pair-2v2-asym.fbm` | 3 | the asymmetric case: cooperative sort against contract sort | the SARH binding as a ratio: fulcrum **17.3 s** bound per shot against viper **0.3 s** — a factor of **58**; `flt_src` reads `cooperative` for the vipers and `contract` for the fulcrums; the F-16 element kills fulcrum1 (`damage KILL` t = 339.2) |
| `pair-cover.fbm` | 0 | the cover rule firing, in isolation | viper2 held its own trigger **7.8 s** (`flt_defer_s`) while viper1 was bound (149.0 → 162.0 s) and fired 0.8 s after that window closed. `flt_both_s` **0.0** for both: the flight never had nobody free |
| `four-4v4-asym.fbm` | 3 | the sort with more than one degree of freedom: four shooters, four contacts | distinct targets per engaged member — cooperative **0.962**, contract **0.750**. One in four contract shooters is doubled up; the cooperative flight is within 4 % of one target each |

### With and without the flight logic

The control is the same file with the `flight` lines deleted — the sort then has no flight to be a
flight of, every `flt_*` column is inert, and each jet flies the single-ship intercept it flew before
this round existed.

| | `pair-2v2-f16` WITH | the same file WITHOUT |
|---|---|---|
| rounds fired | 6 | 6 |
| rounds that arrived inside a warhead radius | **1** (5.69 m from viper1, 51.1 kJ/m² in the nose zone, six systems failed) | **0** in the full 600 s |
| ticks with both members on the same target while another was free | 0 | n/a (no assignment exists) |

Same geometry, same doctrine, same number of rounds — and only the coordinated run puts one on a jet.
It is one sample of a stalemate-prone arena and is stated as such; the repeatable claims are the two
above it (93 % split, 0 violations) and the tournament below.

### The flight tournament

`fb_tournament.py --flight N` turns each side into an element of N flying one doctrine, and a variant
line gained the two keys a FLIGHT doctrine needs: `dl=on|off` (does the element use its cooperative
net) and `sort=<contract>`. With `--flight 1` the generated mission text is byte-identical to the
previous script's on every pairing of both existing variant files (**120 missions compared, 0
differ**, ignoring the generated header comment).

`tools/variants-flight.txt`, both seats, 20 pairings, 40 runs per geometry:

| variant | mirror fitness | mirror kill/lost | split fitness | split kill/lost |
|---|---|---|---|---|
| `f16_net` (dl on) | 977.1 | 0 / 0 | **940.9** | **1 / 0** |
| `f16_net_left` (dl on + contract) | 977.1 | 0 / 0 | **940.9** | **1 / 0** |
| `f16_solo` (dl off) | **1097.8** | 0 / 0 | 770.0 | 0 / 0 |
| `mig_pair` (contract) | **−480.6** | 0 / 0 | **−1302.2** | 1 / 2 |
| `mig_solo` (no contract) | −534.2 | 0 / 0 | −1400.4 | 0 / 1 |

Three things fall out of it, and the second is a finding against the round rather than for it:

1. **On the geometry where anything is decidable at all, the channel is worth an outcome band.**
   `split` (the 6,000 m / 150 kt energy difference): `f16_net` 940.9 with a flight kill against
   `f16_solo` 770.0 with none. On `mirror` nobody in the whole field kills anybody — the symmetric
   F-16 stalemate of [`pilot.md`](pilot.md) gap 2.3, now with two aircraft a side.
2. **On `mirror` the unnetworked element scores HIGHER (1097.8 against 977.1), and it is a fitness
   artefact rather than a result.** The fitness pays 150 per burst landed and counts the opposing
   FLIGHT's hits for each member; an element that concentrates both jets on one bandit therefore banks
   more hits than one that splits across two, and in an arena where nobody dies the concentration wins
   on points. It is left standing rather than patched: the weights were written before flights existed
   and re-tuning them to make this round look good is exactly what a fitness must not be for.

   **RE-MEASURED under the lexicographic fitness (round `E1`, [`doctrine-evolution.md`](doctrine-evolution.md)
   §State "Exhibit A"), and it did NOT turn around:**

   | geometry | old | new order | what carries it |
   |---|---|---|---|
   | `mirror` | solo 1097.8 > net 977.1 | solo **1.000** > net 0.625 | **0.9 points of `shot lead`** on an exact tie at V = 4 / M = 2 in all 8 runs — the 120.7-point `hits landed` gap is gone, the direction is not |
   | `split` | net 940.9 > solo 770.0 | net **V = 4.25, M = 2.25** > solo V = 4.00, M = 2.00 | the cooperative doctrine is ahead at BOTH deciding levels |

   The reading the two rows force: **`mirror` is a saturated geometry and no doctrine claim may be made
   on it at all.** The arena gate now measures that directly — 100 % modal outcome class, 1 of 9
   doctrine levers — and the tournament prints `decided at level: V 2  M 0  C 18 (of 20 runs)` under
   the same field. The finding of this row is therefore no longer "the fitness pays for the wrong
   thing" (it does not any more) but "the geometry answers nothing".

3. **`f16_net` and `f16_net_left` score identically to the digit**, which is the information hierarchy
   confirming itself: a flight with a shared picture never consults its contract.

Determinism: one fingerprint over `--threads 1/2/4` × 3 repeats for each of the five missions (45
runs, 5 fingerprints), and the tournament's own `--check-determinism` reports **0 of 80 files
differing** between `--threads 2` and `--threads 1` over 20 pairings.

### Gates

All eight harnesses rc=0, `verify-models` and `verify-layers` green, `nm` gate 0 Dawn/WebGPU symbols
in `fb-gym`, native and WASM both build. Frame proof: `gpu_native --mission pair-formation.fbm
--interval 40` writes `mission_0000..0002.png` — the flight mission renders through the oracle. It
does **not** show the wingman, and cannot: `FBUnitsStage` is still a NoOp
([`render/units-visual.md`](render/units-visual.md)), so no unit but the ownship is drawn. The proof
of the formation is the numeric one.

---

## Gaps

| # | Thing | Known from |
|---|---|---|
| **F1** | **A separated wingman rejoins badly, and past a point not at all.** After a defensive turn pulls it out, the station error runs to tens of kilometres and the along-track law cannot close it: [MESS, `four-4v4-asym`] viper4 held station at a **20.4 km median** error for 1,894 ticks. The `SPLIT` rule bounds the damage (beyond the pilot's own commit range the flight is declared split and the member flies its own plan, which took the same case from 44.8 km to 20.4 km) but does not fix the rejoin. What is missing is a REJOIN as its own behaviour — a cut-off toward the lead's future position rather than a tail chase up his course line | this round |
| **F2** | **MEASURABLY IMPROVED (2026-07-31), and by a change that was not aiming at it.** [MESS, the 251-mission regression across G5's default, five independent missions] `flt_switch` falls **117 → 15** (`four-4v4-asym`), **179 → 17** (`w1-09-lfe-four`), **178 → 60** (`w3-09-saturation`), **48 → 3** (`pair-2v2-asym`), **42 → 9** (`w1-04-bvr-pair`) — 65 to 94 % — and `flt_dup` goes to **zero on four of the five**. The mechanism is this gap's own second cause read forwards: F2 named *"a per-frame jitter in the contact list that the settle-time hysteresis does not catch"*, and EMCON ([`duels.md`](duels.md) D3c) silences the radar wherever a mate's report already carries the picture, so the jittering list is not built in the first place. **This is not a doctrine shift** in [`doctrine-evolution.md`](doctrine-evolution.md) §6's sense — no arena passed — but it IS the churn falling in the channel this gap declares as its own measurement. What is still open: the hysteresis itself is untouched, and on `w3-09-saturation` the churn falls by 66 % while `flt_dup` does not move at all | this round, `E11` **REOPENED 2026-08-03: the improvement was a DEFECT, and the size of it is now unknown.** The EMCON silence F2 credits was ONE-WAY — `pilot/FBPilot` could not command the radar back on once the block head went Invalid ([`pilot.md`](pilot.md) §7.6b, `X-6`) — so the jets in those five missions were not flying quiet, they were flying blind for the rest of the run, and a radar that is off builds no contact list to churn. Re-measured on the same five, latched → repaired, `flt_switch` summed over the units and `flt_dup` counted in ticks: `four-4v4-asym` **16 → 21** (dup **0 → 1,108**), `w1-09-lfe-four` **18 → 29** (0 → 1,197), `w3-09-saturation` **60 → 173** (609 → 3,260), `pair-2v2-asym` **4 → 11** (0 → 319), `w1-04-bvr-pair` **10 → 14** (0 → 6,298). Churn rises by 17–188 % and `flt_dup` stops being zero anywhere. **What CANNOT be said** is how much of D3c's original 65–94 % survives: the pre-`D3c` numbers quoted above were taken on a build this tree no longer has, and re-flying it is not part of the repair round. F2's channel is open at the F2-old numbers until somebody re-measures it against a radar that stays on; the hysteresis itself is still untouched. |
| **F2-old** | **The sort still re-sorts more than the geometry moves.** [MESS, `pair-2v2-f16`] 19–35 assignment changes per unit over a ~430 s engagement, and a repeating one-tick flip is visible in `events.log`. Two causes were separated and one fixed: the symmetric yielding loop (fixed, below), and a per-frame jitter in the contact list that the settle-time hysteresis does not catch because the OLD assignment leaves the block for a tick. Both candidate fixes were measured and rejected (below) | this round |
| **F3** | *(round `E1` read this as INERT off `xmirror`; round `E2` **corrected it** — `xmirror`'s east seat is the MiG, which has no datalink, so the element measured never had a mate's bound-bit. With `dl=on` on BOTH sides the rule fires where the launch is far enough for the round to bind: [MESS, `--flight 2`, rails 0 / 1.0 / 3.0] `flt_defer_s` 0.0 / **6.3** / 6.3 s on `split` against 0 / 0 / 0 on `mirror`, `far`, `xclose`, `xmirror`, and `flt_both_s` (5.6, 4.6) → (0, 0). The binding is the round's time-to-active, i.e. a function of LAUNCH RANGE, so it is the 12 000 m / 500 kt seat and not the aircraft that makes the rule reachable.)* **The MiG has no cover channel at all**, so the rule that keeps one member free is exactly unavailable where the weapon makes it most valuable. It is the round's sharpest asymmetry and it is a real one — but there IS a sourced candidate channel that was not tried: the SPO-15 has **no IFF and warns of every radar, friendly included** (`datalink-gci.md` §3), and a leader illuminating for an R-27R publishes a Guidance-mode emission. A wingman could infer "my leader is bound" from its own receiver, with the documented ambiguity that it cannot tell that emitter from a hostile one | this round |
| **F4** | **`flt_dup` counts SHARING, not the violation.** The acceptance metric is `dup && free > 0`, computed by the analysis tool; the column alone reads 1 whenever two members share a target, which is the correct behaviour when there is nothing else to shoot at. A reader of the raw column will misread it | this round |
| **F5** | **BUILT (2026-07-31) — G1 is a key, and the first measurement of it is a NEGATIVE that stands.** Three `Scale` ratios (`pilot_flight_spread_frac`/`_trail_frac`/`_stack_frac`), `FormationTrailM` 0 → one spread aft, and line abreast reachable as `trail_frac = 0` — PROVED byte-identical against the pre-round telemetry on all 15 files of `four-4v4-asym`. Blast radius declared before the run (14 files with a third flight position) and measured after: **6 of 251 missions moved, one exit code** (`o3-10-october-six` 2 → 3, on the file's own six measurements: same DESTROYED/IMPACT/INTACT, same `aimErrM` 60.0 m over 8 deliveries, one fewer `monitor KO`, two more `SUCCESS`). **The negative:** on the twelve GENERATED geometries at `--flight 4` the six shape levers move **0** outcome classes. On the campaign breadth they move **13 cells**, and that is what took the gate from 0 informative to 3 ([`doctrine-evolution.md`](doctrine-evolution.md) §State `E8`) | `E-20`, `E8` |
| **F5-old** | *(blocks G1 of [`doctrine-evolution.md`](doctrine-evolution.md); `tools/fb_evolve.py` prints the blocker at start and refuses the gene. **CONFIRMED as a hard block, round `E2`:** the gene is not a key at all — a mission carrying `set pilot_flight_shape 1` gets `module SET_INVALID_VALUE … reason="no such pilot parameter, or out of range"` and `mission SET_REJECTED` at t = 0.0 and **exits 1** before the first tick, so there is nothing to measure until F5 itself is built.)* **A flight cannot be given a formation other than combat spread.** `FormationSpreadM`/`TrailM`/`StackM` are airframe hooks, not mission data, so a mission cannot brief a wedge, a trail or a wall — and `FormationTrailM` defaults to 0, so a four-ship is four abreast rather than two elements in trail | this round |
| **F6** | **The lead has no flight-level decision.** He sorts himself first and everybody else follows; there is no "commit", no "bracket", no "grinder", and no way for the lead to send the wingman anywhere. Every tactic in this file is emergent from one shared cost function | this round |

### Rejected approaches (do not retry without a new argument)

| Approach | Why rejected |
|---|---|
| **Symmetric yielding** — every member honours every mate's declared target | it is a feedback loop whose delay is the net cycle, and it oscillates at exactly that period. [MESS, `pair-2v2-f16` first cut] both vipers swapped targets every **1.0 s for 60 consecutive cycles**, each believing the other was on the one it had just left, and `flt_switch` reached 62–65 per unit. Replaced by SENIORITY (a claim counts only from a lower position number, the lead honours nobody): switches **64 → 19** on the lead and **62 → 33** on the wingman |
| **Age-compensating the contact range** in the cost function (`R − closure·LookAgeS`, physically the right correction for a look-old measurement) | it made the metric it was meant to fix **worse**: [MESS, `pair-2v2-f16`] wingman switches **33 → 87**, lead **19 → 23**. The correction moves this jet's contact points but not the correlation gate against a mate's differently-aged report, so it buys accuracy in the cost and loses it in the correlation. Reverted |
| **Holding an assignment through a one-tick contact drop** (`kAssignHoldS` = the net cycle) | measured no effect at all — switches unchanged to the digit on every unit of `pair-2v2-f16`. Removed rather than kept as insurance: a mechanism with no measured effect is weight |
| **Capping the wingman's along-track speed correction at `sqrt(2·a·spread)`** (the closure it can cancel over one spread) | too tight for a rejoin. [MESS, `four-4v4-asym`] a wingman 40 km behind its station commanded only +94 m/s and sat at a **40–48 km error for 230 s with no trend**. The cap was removed: `sqrt(2·a·|e|)` is self-limiting anyway (42 kt at 100 m of error) and the airframe caps the top end |
| Making the F-16's cover deferral bite by shortening its own binding | there is nothing to shorten. [MESS] an AIM-120 fired at Rtr head-on goes active **0.3 s** after launch, so no plausible pair of decisions overlaps. The rule is genuinely almost free for this weapon, and `pair-cover.fbm` exists to make it observable rather than to make it matter |

---

## Knowledge

### 1. The flight as identity

A flight is declared beside the team and for the same reason: it is BOTH mission data and world
identity, so `core/FBFlight.h` sits next to `core/FBTeam.h` and `FBUnit` carries an `FBFlightId` next
to its `FBUnitTeam`. Duplicating it — a mission notion of "who leads" and a world notion — would let
the two disagree.

```
flight <name> <position>          # actor-scoped, at most once per block
```

`position` 1 is the lead, 2..8 the wingmen (8 because a flight can never be larger than the
cooperative track list that carries it, `kMaxDatalinkTracks`). Parse errors: a position outside 1..8,
a name that is not file-safe or is 16+ characters (it travels in a PPLI field), two units at the same
(name, position), a second `flight` line in one block, and — checked at end of file, like a `kill
unit` forward reference — **a flight with no unit at position 1**. Every piece of formation behaviour
is defined against the lead, so a flight without one is not a flight.

`FBFlightId::Declared()` is `Position > 0 && !Name.empty()`, and every consumer treats undeclared as
"no flight" rather than "flight 0".

### 2. What travels, and what deliberately does not

The flight rides the PPLI the cooperative terminal already sends — no channel of its own, and
therefore the terminal's range, its 1 Hz cycle, its three-cycle hold and its age come for free.
`FBDatalinkTrack` gained three things (`core/FBDatalinkTrack.h`):

| Field | Where it comes from | Why it may travel |
|---|---|---|
| `FlightName`, `FlightPos` | the sender's registry identity, like its callsign and team | a cooperative net is one's own faction; identity is what it is FOR |
| `Report.Engaging` + `Report.Tgt{Lat,Lon,Alt}` | the sender's pilot | a **point**, never a track and never an identity. This radar does not know whom it sees (`core/FBRadarContact` is anonymous), so it cannot tell anybody |
| `Report.Bound` | the sender's `FBEngagement` | "the round I launched still needs me" — one bit, and the whole cover mechanism |

The receiver correlates the reported point against its OWN echoes and **may fail to**. That failure is
the honest property of a shared picture and it is modelled rather than assumed away (§5.2).

`FBUnitSignature::Flight` publishes it at the same tick barrier as the pose, so no receiver ever reads
half of it, and it is empty for a unit in no flight — a store, a ground target or any aircraft the
mission did not put in one.

**The `fl` filter is now a statement.** `FBDatalinkSystem::Cycle` passes `AcceptContact` the sender's
declared position minus one where a declaration exists, and the old registry ordinal where none does.
`FBF16Datalink`'s `fl` (only flight leads) therefore selects position 1 in a mission with flights and
behaves exactly as before in one without — which is why `payerne-pair-datalink.fbm` and every other
stock mission is byte-identical.

### 3. `pilot/FBFlightPicture` — the flight as the pilot has it

`FBBfmTrack`'s sibling, and built under the same rule: **only from `FBState` blocks**. Its include
list is `FBFlight.h`, `FBState.h`, `FBTelemetry.h`, `FBFdm.h` — no registry, no world, no other unit's
pilot. It therefore inherits the perception boundary of [`sensors.md`](sensors.md) §1 rather than
reopening it.

Per decision tick it builds a member list: this jet out of its own state, everybody else out of a
datalink track whose `FlightName` matches. The two are deliberately the SAME type, `FBFlightMember`;
the only difference is `AgeS`, and that difference is the entire reason a station is held on a report
rather than on a position.

The pilot's own numbers arrive per call in `FBSortParams` (turn rate, commit range, briefed axis,
switch margin) rather than being duplicated as hooks here: this class decides WHO takes WHAT, and
every quantity it needs already exists one level up.

### 4. The station — position keeping on a moving point

#### 4.1 The law, and why it is two channels

Aiming `Direct` at the station is pure pursuit of a point moving at combat speed: the bearing error
never closes and the loop is the same regime that produced the merge roll problem
([`pilot.md`](pilot.md) §5.7.3). So the station is flown as two channels that never talk to each
other:

| Channel | Mechanism | Why it is safe |
|---|---|---|
| across + vertical | `FBAutopilot::SetDirectLeg` onto the lead's COURSE LINE THROUGH the station (leg origin = the station, target = 60 nm ahead of it **along the lead's heading**) | it is the existing path-following law with its existing derivation ([`systems.md`](systems.md)): a cross-track distance to a LINE, not a bearing to a point |
| along | the throttle | `dv = sqrt(2·a·|e_along|)` with `a = BfmBrakeMs2()` — the closure this airframe can still cancel over the remaining distance. Same closed form as the BFM closure schedule. **No gain, no time constant, no integrator, and no cap**: the square root is self-limiting (42 kt at 100 m of error) and the airframe caps the top end |

The target point is computed **from the station**, not from own position — otherwise the line the
autopilot holds is the chord between the station and this jet rather than the lead's course.

Measured (`pair-formation.fbm`, a lead flying two 90° turns with a wingman doing nothing but hold
station):

| segment | median | p90 | max |
|---|---|---|---|
| straight leg 1 (t 0–120) | 137.2 m | 326.7 m | 412.8 m |
| turn 1 + recovery | 73.3 m | 1,703.9 m | 1,936.7 m |
| straight leg 2 | 85.5 m | 736.4 m | 1,360.3 m |
| turn 2 + recovery | 293.8 m | 1,640.4 m | 1,909.6 m |
| straight leg 3 (t 560–900) | **45.2 m** | 81.0 m | 84.4 m |

The turn peaks are a real cost, not an error: a station one nautical mile abeam swings through the
lead's turn radius and the wingman has to fly the outside of it. What matters is that there is no
standing offset — the third leg settles at 45 m and stays there.

#### 4.2 The geometry, and the one bound that is derived

A flight is decomposed into ELEMENTS of two: `k = position − 1`, `element = k/2`,
`inElement = k mod 2`. Lateral offset is `(inElement ? spread : 0) − element·2·spread`, so position 2
sits one spread right of the lead and the second element sits two spreads left with position 4 tucked
inside position 3. Altitude steps by `k · stack`, so no two aircraft in a flight are co-altitude.

`FormationSpreadM` = 1,852 m (1 nm) is **[SET] with a derived lower bound**, and the bound is worth
stating: the station is held on a report that may be up to `kDropAfterCycles · kNetPeriodS` = 3 s old,
so at 450 kt TAS (231.5 m/s) it carries **695 m** of pure report age. A spread below that would let the
wingman steer through its leader without anything having been computed wrongly. 1 nm is the same order
of magnitude with a 2.7× margin. `FormationStackM` = 150 m and `FormationTrailM` = 0 are [SET].

#### 4.3 Where the station is flown

Two places, and both are the same function:

- **phase `Formation`** (`set task formation`) — the transit case, and what `pair-formation.fbm` flies.
  The LEAD in this phase flies the mission's route: the flight follows him, he follows the mission.
- **inside `Intercept`, while the wingman has nothing assigned.** Two jets independently sweeping the
  same briefed vector are not a flight, they are two singles with one job. The wingman therefore holds
  station through `Search`/`Idle` and falls out of it the moment his OWN radar gives him something the
  sort can assign. His antenna is unaffected — only the guidance fields of the tick's command are
  replaced, so the cockpit work below runs unchanged.

**A station has a range.** Beyond the pilot's own commit range the two jets are no longer inside each
other's engagement geometry and are not a flight but two singles; the member then logs `flight SPLIT`
once and reverts to its own plan (§Gaps F1).

### 5. The sort — who takes whom

Three levels of information, applied in the order of what they are worth.

#### 5.1 The cost — time to a shot

A fighter's job against a contact is to point at it and close to its own commit range, so the cost is
those two terms and nothing else:

```
t(member, contact) = |ATA| / omega  +  max(0, R − R_commit) / V
```

`ATA` and `R` come from this jet's own radar contact placed in the world (bearing and elevation angle
are the block's world-referenced pair, so no look-old body vector is un-rotated through a
now-current attitude) and from the member's PPLI. `omega` is `FBPilot::CornerTurnRateDegS()` and
`R_commit` this pilot's own lock range: **what a peer's own numbers are cannot be known, so this pilot
assumes his mates fly as he does** — the same modelling choice `FBTrackDatum` already makes about the
OPPONENT ([`pilot.md`](pilot.md) §6.2).

#### 5.2 The matching, and the seniority rule

1. **Declarations.** A senior member's reported target point is correlated against this jet's own
   echoes, in three dimensions, inside a gate that grows with the report's age
   (`1,000 m + age · 300 m/s` — the point keeps moving while the message travels). The claim counts
   only if it is **unambiguous**: two aircraft in combat spread are one spread apart while a report is
   as stale as the net cycle, so if the second-best echo is within twice the distance of the best, the
   correlation is a coin toss and nothing is claimed.
2. **A greedy minimum-cost matching** over what is left, strictly ordered — lowest cost first, ties by
   flight position and then by track number. Every member computes the identical matching from the
   same shared data, so a cooperative flight agrees without exchanging a word about it.
3. **Surplus shooters double up** on the contact they reach soonest. Nobody goes home while something
   is unengaged; that is the "nobody unengaged" half of the contract, and it is why `flt_dup` alone is
   not the violation metric (Gaps F4).

**Seniority is the reason it converges.** A claim is honoured only from a LOWER position number, and
the lead honours nobody — his matching IS the flight's. Symmetric yielding was built first and
measured: it is a feedback loop with the net cycle as its delay and it oscillated at exactly that
period (Rejected approaches). A flight resolves that the way a flight resolves everything: the lead's
call stands.

**Hysteresis.** The turn is already inside the cost, so what a swap costs on top of it is the settle
time of the new single-target track — `FBPilot::kInterceptTrackSettleS`, 2.0 s. A re-assignment that
saves less than that buys nothing.

#### 5.3 The contract — the sort of a flight with no channel

`set brief_sort left|right|near|far`. It is a BRIEF, not a switch: nothing in the jet is operated, and
it is what a flight agreed before anybody saw anything. Each member sorts the contacts it personally
has by the briefed key — left/right measured against the FLIGHT's axis (the briefed vector), never
against its own nose, because a rule anchored to one's own heading would mean something different to
every member and could not be a contract — and takes the `position − 1`-th of them.

It is dynamic (it applies to what he actually sees) and it is blind (he cannot know what the others
see). That is precisely the doctrine of an aircraft flown to the merge by somebody else, and the
measured consequence is §7.

The three levels are strictly ordered: a declaration beats the matching, the matching beats the
contract, and a flight with no mates on the net and no contract sorts nothing at all.

### 6. Cover — the flight keeps one member free

> A member does not fire a round that would BIND it while a flight-mate is already bound.

**Bound** means the round in the air still needs THIS jet — `FBEngagement::SupportComplete()`, which
already knows the difference: an active round's obligation ends at seeker activation, a semi-active
round's runs to impact. The rule is therefore **one rule with one definition on both airframes**, and
everything that differs between them comes from the weapon.

The physical reason is not "two rounds are wasteful" but that a supporting shooter is flying at its
own antenna: it holds the lock, cranks no further than the gimbal allows, and — where the round is
semi-active — is forbidden to defend at all (`FBPilot::SupportInhibitsDefend`, the MiG's hook). A
flight in which every member is supporting has nobody who can answer a launch.

**The deferral has a bound and it is derived from the same quantity:** a member holds at most as long
as its own shot would bind it (`max(shot spacing, own predicted time to impact)`). After that, two
rounds in the air beat one.

Measured:

| | AIM-120 (F-16) | R-27R (MiG-29) |
|---|---|---|
| bound per shot | **0.3 s** [MESS `pair-2v2-asym`] | **17.3 s** [MESS, same run] |
| `eng_pitbull` | 1 — the seeker took over | 0 — it never does |
| what the rule costs | almost nothing | an entire engagement |
| can the flight apply it? | yes: `flt_defer_s` **7.8 s** measured on `pair-cover.fbm` | **no** — the aircraft has no channel on which "I am bound" could travel |

The last row is the round's sharpest finding and it is not a defect of this implementation: it is the
doctrine contrast of `datalink-gci.md` §5.1 arriving at its consequence. The aircraft whose weapon
makes cover most valuable is the one that cannot organise it.

**The rule is also bounded by the net cycle**, and that is measured too: [MESS, `pair-cover` without
its range stagger] two F-16s that reached firing parameters 0.4 s apart both fired and spent **7.9 s
bound together** — the "mate is bound" bit had not arrived yet. A rule cannot act on what has not been
transmitted, which is why `pair-cover.fbm` staggers its two bandits by 3 km.

### 7. The asymmetry, as one number

Same run, same geometry, four shooters a side (`four-4v4-asym.fbm`), counted over every tick in which
at least two members of a flight were engaged:

| flight | sort | distinct targets / engaged members |
|---|---|---|
| viper (F-16, cooperative) | computed from the shared picture, deconflicted by the lead's claim | **0.962** |
| fulcrum (MiG-29, contract) | briefed before takeoff, applied to each pilot's own picture | **0.750** |

One in four contract shooters is prosecuting a target another member of its own flight already has;
the cooperative flight is within 4 % of one target each. Neither number is a setting — they are what
the two channels produce on the same geometry.

### 8. The channels — `flt_*` and the events

Fourteen columns, appended at the end of every unit's telemetry (source `flt`, registered last in
`FBSimUnit::StartTelemetry`), all computable from own perspective:

| Column | Meaning |
|---|---|
| `flt_pos` | this unit's declared position; 0 = no flight, and then every column below is inert |
| `flt_mates` | flight-mates currently in the picture (0 for a flight with no channel) |
| `flt_src` | 0 none, 1 cooperative, 2 contract |
| `flt_assign` | the radar track number this jet is prosecuting; 0 = none |
| `flt_switch` | cumulative re-assignments (Gaps F2) |
| `flt_dup` | this jet shares its assignment with a mate — the VIOLATION is `flt_dup && flt_free > 0` |
| `flt_free` | contacts in this jet's own picture that nobody in its flight is on |
| `flt_bound` | this jet's launched round still needs it |
| `flt_mate_bound` | a flight-mate's does |
| `flt_both_s` | cumulative seconds with both true — the flight had nobody free |
| `flt_cover_s` | cumulative seconds bound while a mate was free |
| `flt_exposed_s` | cumulative seconds bound while a warning demanded an answer |
| `flt_defer_s` | cumulative seconds this jet's own trigger was held for the flight's cover |
| `flt_sta` | station error in metres this tick, −1 when no station is being held |

Events: `flight SORT_ASSIGN` (track, previous track, source, mates, free contacts, duplicate flag),
`flight SORT_DROP`, `flight COVER_DEFER` (track, range, the cap it is holding against) and
`flight SPLIT` (the lead is beyond rejoin range).

`sim/tools/fb_flight_report.py` reads them back and recomputes nothing:
`tools/fb_flight_report.py /tmp/run` for one run, `--diff A B` for with-against-without.

### 9. How to run it

```
make -C sim gym
for m in pair-formation pair-2v2-f16 pair-2v2-asym pair-cover four-4v4-asym; do
    build/fb-gym --mission missions/$m.fbm --out /tmp/fm/$m
done
tools/fb_flight_report.py /tmp/fm/*
tools/fb_tournament.py --variants tools/variants-flight.txt --out /tmp/tflight --flight 2 \
                       --geometry split --timeout 420 --check-determinism
```

The control for every claim is the same mission with its `flight` lines removed.
