# Journal — the chronicle of the rounds

**What this file is:** one line per finished round, in the order they happened — commit, what it
built, what it measured. It is history, not a plan and not a contract.

- **What each area must do** → the `## Spec` section of its topic file ([`INDEX.md`](INDEX.md)).
- **What is built right now** → the `## State` section of that same file.
- **What is missing, and what was tried and rejected** → its `## Gaps` section.
- **What comes next, in order** → [`roadmap.md`](roadmap.md).

Every round adds a line here; nothing here is ever rewritten to look better. Rejected approaches are
kept — in the Gaps of the file they belong to, with their measurements.

State of the entries below: commit `793e1fe` + the model-root/delta round (2026-07-27).

## Maturity per area

| Area | State | Doc |
|---|---|---|
| FDM adapter | **finished** — instanceable, IC-sealed, damage and stores channels | [fdm.md](fdm.md) |
| Core / avionics bus | **finished** — typed blocks with three-state validity, command bus with acknowledgement | [core.md](core.md) |
| Mission orchestrator | **finished** — four steps, no mission knowledge in the code | [missions/runtime.md](missions/runtime.md) |
| Multi-unit | **finished** — cast as mission data, thread per unit in the gym, deterministic | [missions/runtime.md](missions/runtime.md) |
| Formation | **built** — roles as mission data, station keeping, target sort, cover rule; rejoin and lead-level tactics missing | [formation.md](formation.md) |
| Sensors | **built** — datalink, radar, RWR, IRST, countermeasures. Without terrain masking. | [sensors.md](sensors.md) |
| Weapons | **built** — AIM-120, Mk-82, M61A1, ground targets, damage model | [weapons.md](weapons.md) |
| Pilot AI | **in progress** — takeoff/route/landing, BFM, BVR intercept, air-to-ground all fly; refinement ongoing | [pilot.md](pilot.md) |
| Renderer | **built** — stage split complete. Units and weapons still invisible. | [render/renderer.md](render/renderer.md) |
| HUD | **built** — generic default HUD + full F-16 symbology, coverage AA | [modules/f16/module.md](modules/f16/module.md) |
| Cockpit displays | **not started** — the values are on the bus, the presentation is missing | [clients/clients.md](clients/clients.md) |
| HOTAS | **not started** — deliberately last, it is only a mapping | [clients/clients.md](clients/clients.md) |

## Chronology

### 2026-07-28 — clouds: the proof set becomes reproducible, and the grain is measured rather than assumed (this round)

The R5 rebuild was built and merged with three things open. All three were taken to a number; two of
them ended somewhere other than where the gap text expected, and that is recorded as such.

**The proof set is reproducible now, and the recipe is the finding.** The stored PNGs could not be
reproduced from the committed source (99.88 % of pixels differed — tuning drift, re-measured on the
merge). The cause of the *irreproducibility* was never the clouds: the screenshot venue streams tiles
while it renders, so a short run frames a half-built quadtree. Holding the camera still for 180 frames
and writing only the last one puts the streamer at `pending=0`, and the converged tile set is a pure
function of the camera — **12 frames, two independent runs, byte-identical sha256, in both SVS and
EVS**. The consequence is that the fly-through had to become a LADDER through one column rather than a
flown mission: a moving camera never converges, so a flown sequence cannot be hashed. Named in Gaps.

**The march grain fell 13 %, not the 50 % the gap hoped for, and the honest reason is in the way.**
Removing the erosion term entirely dropped the grain from 0.0263 to 0.0149, so the erosion — 1.6 km
wide, 300 m tall, against a 260 m step — is what a 6–12 node march undersamples. Two changes went in:
a **composite trapezoid** in place of the jittered rectangle rule, and a **band-limit of the erosion
against the march's own step** (`ErodeFlat`, a new parameter of the SHARED density function, so
`--cloudcheck` still covers it — AGREE at 1.90·10⁻⁵). Grain 0.0329 → 0.0285 (3×3 high-pass), cost
+5 %. The bigger win is one the gap did not ask for: the rectangle rule rendered a thick deck **3.7 %
too dark**, and the trapezoid renders it to +0.8 % of the converged reference. Five alternatives were
built, measured and rejected — no jitter (−58 % grain, but 72 px contour bands), half-amplitude jitter,
a front-loaded geometric step ladder (+39 %, worse: it is right for a steep crossing and wrong for the
grazing far field), a stratified 4×4 ordered jitter (−1 %: stratification fixes the block mean, the
artefact is per-pixel), and an amplitude fade of the erosion, which measured BEST and was rejected
because the improvement was a 3.5 % brighter deck in the metric's denominator.

**One ceiling, three étages: a weight, not a choice.** The predecessor clamped the reported ceiling
into the band of the lowest broken deck and carried two discontinuities — the base stuck at the band
edge while the ceiling walked on (4.2 km of assertion the data contradicts, measured), and the choice
of deck flipped whenever a cover crossed 0.5. It is now an ownership weight with a 2 000 m handover:
étages hand the ceiling over continuously, a deck below a tenth of the sky cannot own one, and the
outermost edges saturate because there is nothing to hand to. The Payerne proof corridor is unchanged
to the metre. The price is named and measured: during a handover the deck slides at up to 325 m/km
against the data's own 135 m/km.

**And the question the owner actually asked — do you see them when you fly?** Yes, on this run.
Probing the committed fixture point by point: over the Swiss box (90 points) 60 % of points carry
≥ 25 % cover in some étage and a ceiling is reported at 71 % of them; Payerne itself is 75.7 % low
cloud with a 2 991 m ceiling. The distribution is strongly bimodal — that is GFS' own layer
diagnostics, not a defect here — and one run is one atmosphere, so a mission that needs a guaranteed
sky still has to say `wx fixture`.

One defect was found that predates the round and is NOT fixed: **the underside of an optically thick
deck receives no light**. The slant optical depth to the sun is ≈ 57, all three multi-scatter octaves
evaluate to zero, and the base ends up the ambient floor times the sky — dark blue where a real
overcast is bright grey. It is an ambient/multi-scatter model change, not a march change, so it is
named with its numbers instead of patched in the middle of a proof round; the cirrus frame shows the
same code producing Beer damping and a silver rim as soon as the deck is thin.

Gates: all targets clean, `verify-layers`/`verify-models` green, nine harnesses rc=0, `fb-gym` still
GPU-symbol-free, WASM builds, `--cloudcheck` AGREE, and 14 telemetry CSVs over six missions
byte-identical to `HEAD` — the clouds do not touch the physics, and that is now measured rather than
argued.

### 2026-07-28 — out of single fighters, an air force: the FLIGHT

A "flight" was an appearance. `fl` in the datalink's contact filter meant "the first unit of that
faction in mission order", two jets of one side flew two private wars, and nothing in the tree could
tell whether they had prosecuted the same target while a third went unengaged. This round makes it a
mechanism, in five pieces, and every one of them is a no-op without the declaration that turns it on.

**Roles are mission data.** `flight <name> <position>` in a `.fbm` unit block, beside `team` and for
the same reason: it is both mission data and world identity (`core/FBFlight.h`, `FBUnit`), so the
cooperative datalink reads it off the registry as it reads the team. Position 1 is the lead, a flight
without one is a parse error, and `fl` now selects it.

**The wingman holds a station on a moving point**, in two channels that never talk to each other:
across and vertical through the path law that already exists (`SetDirectLeg` onto the LEAD's course
line through the station), along through the throttle at `sqrt(2·a·|e|)` with the airframe's own
measured brake authority. That separation is the law — a `Direct` at the station is pursuit of a point
moving at combat speed, which is the regime that produced the merge roll problem. Measured
(`pair-formation.fbm`): **45.2 m** median station error on a straight leg, 1,937 m peak through a 90°
turn, no standing offset.

**The sort is three levels of information**, applied in order of worth: what a mate SAYS (a target
POINT, correlated against one's own echoes — never an identity, because this radar does not know whom
it sees), what the flight can WORK OUT (a greedy minimum-cost matching on time-to-a-shot, identical on
every member), and what was AGREED before takeoff (`set brief_sort`, the only sort an aircraft without
a cooperative terminal has). Measured: the cooperative pair holds different targets in **93 %** of the
ticks both were assigned, and `dup && free` — a target engaged twice while another was free — is
**0 over every unit of every formation mission**.

**Cover is one rule whose price is the weapon.** A member does not fire a round that would bind it
while a mate is already bound; a bound shooter flies at its own antenna and a flight with nobody free
cannot answer a launch. Measured: an AIM-120 binds **0.3 s**, an R-27R **17.3 s** — a factor of 58 —
and `pair-cover.fbm` measures **7.8 s** of real deferral with the flight never left uncovered. The
MiG cannot apply the rule at all, because "I am bound" has no channel on its aircraft; that is the
round's sharpest finding and it is the doctrine contrast arriving at its consequence.

**The asymmetry as one number** (`four-4v4-asym.fbm`, same run, same geometry): distinct targets per
engaged member, cooperative **0.962** against briefed contract **0.750**.

Five missions (`pair-formation`, `pair-2v2-f16`, `pair-2v2-asym`, `pair-cover`, `four-4v4-asym`), one
analysis tool (`fb_flight_report.py`), a `--flight N` mode for the tournament with `dl=`/`sort=` on a
variant line, 14 appended `flt_*` columns. **All 79 stock missions byte-identical** on every column
they ever had and line for line in `events.log`; one fingerprint per new mission over `--threads
1/2/4` × 3. Rejected with their measurements: symmetric yielding (a 1 Hz oscillator, 60 consecutive
swaps), age-compensating the contact range (switches 33 → 87), a blink hold (no effect), and capping
the along-track correction at one spread (a wingman stuck 40 km out for 230 s). Full contract,
derivations and gaps: [`formation.md`](formation.md).

### 2026-07-28 — the F-16's roll law gets an END: the merge becomes two-sided

The previous round moved the merge's blocker onto the F-16: with the MiG acquiring and flying aggressive
locked pursuit, the F-16 departed `duel-merge` at t=18.0 (`LOC`). **Diagnosis first.** The regime is not
"a reversal": it is a head-on pass at **898 kt of closure** whose line of sight sweeps at up to
**543 °/s** — 34 × the airframe's corner turn rate — against `gun-bfm`'s 17.7 °/s and `bfm-basic`'s
6.2 °/s. There the commanded lift direction rotates WITH the aircraft, the roll error never closes, and
the law rolled **290° in 3.1 s** on a steering error of 10–20°. Neither existing guard sees it: the
conversion guard has exactly this premise but tests the target's angular OFFSET (its zone floors at 90°,
the merge never exceeded 76.6°) and only freezes the turn SENSE, which permits an unbounded roll in that
sense; the rate cap did bind and held 103–109 °/s against its declared 90.

**The fix is the missing half of the same closed form, not a fourth hook.** §5.7.2 derived the cap as
"180° in `kBfmTurnTimeS`" and applied it as a PEAK — next to a judge whose rule is a SUSTAINED quantity
(|ω| > 60 °/s for 3 s), so 90 °/s is 1.5 × the threshold and survivable only while no geometry holds it.
The law always takes the short way, so no correction can require more than 180° of roll in one direction;
the same sentence therefore also bounds the roll flown per window. The cap is scaled by the share of a
reversal still open (`cap · (1 − |∫p dt over kBfmTurnTimeS| / 180)`), which is exact at an empty window
and has the fixed point `p = cap/2` — 45 °/s for the F-16 — because `cap · T = 180`. That margin against
the judge is thin on purpose: measured, it is what the softer variants do not have.

**Measured.** `duel-merge`: longest |ω| > 60 stretch **2.9 → 0.8 s**, roll per 2 s window
**195.8 → 110.8°**, run **18.0 → 232.3 s**, no F-16 departure; viper `lock_s` 16.1 → 28.2, fulcrum
14.2 → **79.4** and the campaign's first WVR employment (12 GSh-301 rounds, t=195.2, miss). 16-approach
sweep (now a committed tool, `sim/tools/fb_bfm_sweep.py`, instead of a scratch script every round has
to guess back): departures **6 → 0**, kills **5/16 → 7/16**, peak roll rate **182.2 → 103.4 °/s** (2.02 × → 1.15 ×
the cap), seconds above the cap **63.2 → 5.4**. Rejected with their numbers: the bang-bang form of the
same constraint (sweep departures 3, `gun-bfm` loses its kill) and giving the MiG back the derived 90 °/s
peak (restores `mig29-bfm`'s control position, costs a MiG departure at t=103.6).

**Re-baseline:** 79 missions, **73 byte-identical**, 6 moved, ONE verdict change — `gun-dry` 1 → 3, whose
twelve rounds all still arrive but into the nose zone at 25.7 kJ/m² instead of the centre at 153. Two
costs are declared rather than explained away: `bfm-blind` and `mig29-bfm` lose their control position
(`bfm_ctrl_s` 48.4/88.2 → 0.0), causally, because a sustained conversion roll is now half the peak. A
matching chaos measurement bounds how much of the sweep is signal at all: perturbing `gun-bfm`'s spawn in
0.8 m steps over ±3 m flips it between KILL (t=77.9…197.1) and no kill in 2 of 8 samples. Determinism
1/2/4 threads × 3 on all six moved missions plus `payerne-pair`/`-four`; nine harnesses, `verify-models`,
`verify-layers`, `nm`, native frame proof out of the merge, WASM rebuilt and hash-verified against the
baseline build. What the merge STILL does not test is the R-73/GSh-301 thesis, and the reason is now
honest: 190 of its 232 s have the F-16 blind, 143 the MiG, 133.6 both — after the first pass neither ACM
box re-acquires, both jets sink and the MiG loses that race. New gaps: `pilot.md` 2.8 (the lift-vector
law is singular for a downward demand above 1 g — the mechanism that TRIGGERED the roll) and 2.9
(close-combat re-acquisition + the BFM floor).

### 2026-07-28 — MiG-29 value round: the merge acquisition and the second dispenser

Two coupled gaps, both closed. **(A) The N019 acquires in a turning merge.** Its documented close-combat
modes (CC/VS/BORE) are azimuth PENCILS (±1.75°/±1.5°/±1.25° — the vertical reading DCS-FM p.12 forces),
and a pencil cannot hold a manoeuvring nose-on merge target: `duel-merge` fulcrum lock_s was **0**. A
BROAD forward auto-lock volume was added, `FBMig29Radar::kAcm*` — ±37° azimuth [T4 §7.1, read as azimuth
against the vertical reading CC takes; the two sources genuinely conflict], a [SET] ±30° nose-centred
vertical band (measured threshold: ±25° never firms, ±30° acquires, wider buys nothing), Doppler-EXEMPT
like CC (a co-speed merge is the low-closure case), frame **0.75 s** [DERIVED from T4's "1-2 s lock" over
the generic two-look firming, the same construction as the RAD 3.0 s frame]. The pilot SELECTS it in the
fight phase: a new generic hook `FBPilot::BfmRadarModeOrdinal` + `BfmSelectRadarMode`, the F-16 leaving it
−1 (byte-identical, its `acm_hud` is mission-set). Measured: `duel-merge` `n019 MODE acm` t=0.5,
`RADAR_LOCK` t=3.8 (3.32 nm), **lock_s 14.2**; `mig29-bfm` improves 203→296 lock_s / 5.3→88 ctrl_s.

**(B) The BVP-30-26 dispensers.** `modules/mig29/FBMig29Cmds`, a `sensors/FBCountermeasureSystem`
override: 60 cartridges [DOC], a [SET] 30/30 split (a named source gap), 5/5 BINGO and the three geometry
programmes on the generic slot machine ([SET] values, schema from the source — the F-16 ALE-47 pattern).
Wired in (cycled RWR→CMDS at 10 Hz, `CmDispense`/`CmConsent`/`CmdsMode` routed, `cmds_*`/`brief_flare_s`
keys, gated on the `Countermeasures` health id the damage layout already zoned). Its flares seduce the
AIM-9 through the SAME deterministic model that seduces the R-73 (`sensors/FBIrstSystem::SelectFlare`):
`mig29-defend.fbm` measures `FLARE_SEDUCED tgtIntensity=0.16` (head-on/dry) and the round expiring 16.0 m
wide, against an astern control detonating 0.04 m out. The defensive asymmetry (D5) is now two-sided.

**The finding the merge exposed.** With the MiG now flying aggressive LOCKED pursuit, the F-16's own
UNCAPPED BFM roll law (defaults 90 °/s, `BfmSearchRollCap` 1.0) goes into a full-deflection roll-reversal
PIO at the close-in high-closure reversal and DEPARTS at t=18 (`duel-merge` exit 3→2, result=LOC). It is
CAUSAL — with the MiG's ACM disabled the F-16 does not depart and the run is exit 3 again. Per the
campaign rule a loss to an AI defect is not a result, so this is NOT "the MiG wins the merge"; the
R-73/GSh-301 thesis stays untested and the merge blocker moved a third time (departure → acquisition →
the F-16's roll law). The F-16 cap is F-16-scoped and would touch its byte-identity, so it is deferred.

**Gates.** All **57 F-16-only** stock missions byte-identical (before/after `telemetry*.csv` SHA-256);
`test-corner` unchanged (380 / 16.18 / 5.44); MiG non-combat missions byte-identical; the BVR duels
(`duel-offset`/`duel-emcon`) move only in `cmd_*` bookkeeping (the intercept CmDispense is no longer
rejected `NotImplemented`, flight state and outcome identical). Determinism 1/2/4 threads ×3 = one
fingerprint on `mig29-defend`, `duel-merge`, `mig29-bfm`. `verify-models`, `verify-layers`, WASM and
native all green; proof frame `notReadyDraws=0 violation=0`.

### Foundation (24–25 Jul)

| Commit | Section |
|---|---|
| `59f08c8` | module architecture runtime-polymorphic, nine system slots with NoOp defaults |
| `c9206eb`…`2099cb0` | renderer stage split in four slices — at the end zero inline shaders in `FBRenderer.cpp` |
| `4cb92e8` | HUD stopgap → generic default HUD in the displays slot |
| `2f3c277`, `8997eec`, `6f160af` | HUD font: coverage AA instead of alpha test, split into generic font system / MAX7456 hook, 16×16 glyphs from B612 Mono, the same AA technique for all strokes |
| `6802a6d`, `d31b1a9` | F-16 main HUD with the real combiner aperture, legibility for 720p |

### Pilot AI and the control loop (26 Jul)

| Commit | Section |
|---|---|
| `681c5f8` | pilot-AI framework: `FBPilot`, units, airframe controls |
| `65d334c` | mission runner + telemetry — **the control loop itself**, the prerequisite for everything that follows |
| `e49d335` | phase 1: takeoff flies |
| `e4d7c26` | telemetry/log architecture: declarative sources, central bus, `FBLog` |
| `705c90a` | lib/client split: core lib, `fb-gym`, elevation hook, baked Swiss DEM |
| `28e74e5` | `FBFlightMonitor` — incorruptible physics K.O., model-derived |
| `92fe8a4` | mission orchestrator down to four steps, declarative spawn, `FBMissionMonitor` |
| `8cd3a74` | phase 3: landing — `payerne-full` flies fully autonomously |
| `bf4ee62` | **hardening**: silent wrong values, aborts, client divergence — see "Defect classes found" |

### Multi-unit (26 Jul)

| Stage | Commit | What it built |
|---|---|---|
| 1 | `c1bc9de` | FDM instanceable — `FBFdm` as an object, no global instance |
| 2 | `c08a168` | the actor is ONE object (`units/FBSimUnit`) |
| 3 | `2c03704` | the formation is mission data — two jets fly |
| 4 | `6d7ed5a` | thread per unit in the gym, lockstep barrier, bit-identical |
| 5 | `9190e7c` | datalink — units see each other through a system |
| 6 | `4049a7b` | FCR radar with ACM modes, anonymous contacts, IFF |
| 7 | `b375bef` | BFM manoeuvre AI — flies on radar contacts alone |
| 8 | `071ea2b` | avionics data model: output blocks with validity + command bus |

### Knowledge base (26 Jul)

`2dd1142`, `e22f228`, `c4e96e7` — the official ED documentation distilled into `doc/modules/f16/`.
`weapons.md` and `defence-rwr-cm.md` from SHALLOW to FULL; `controls-commands.md` new, as the template
for the command blocks.

### Weapons, damage, tactics (27 Jul)

| Commit | Section |
|---|---|
| `b62c769` | weapons foundation: the weapon is a unit of its own with its own FDM |
| `5c68fc5` | AIM-120 with seeker, guidance and datalink support |
| `439f53a` | RWR and countermeasures — who notices being seen |
| `1ecd433` | intercept AI: BVR tactics — guide, shoot, support, defend |
| `6d84647` | damage model: hits become system failures, failures become invalidity |
| `82df2e2` | combat objectives and evolutionary tournaments |
| `a1a8fbf` | M61A1 cannon: derived ballistics, EEGS funnel, kinetic damage |
| `1eeff72` | air-to-ground: ground targets without an FDM, CCIP/CCRP from one integration |

### Refinement of the AI (27 Jul, ongoing)

| Commit | Section |
|---|---|
| `cac7b62` | pilot memory: the datum instead of the last measurement point; gun tracking with a rate term; roll-rate controller |
| `9673e00` | guidance holds a track where a track is declared — cross-track error and waypoint capture |
| *(this round)* | the close-combat law survives a raw airframe — the MiG-29 flies BFM without departing (`duels.md` D1) |

### The MiG's close-combat law (D1) — surviving a deckless airframe

**What it built.** The BFM control law is written for the F-16, whose JSBSim deck holds α and roll rate
under any stick. The MiG-29's deck has no FLCS, so the law departed it in 22.8 s from a merge. Four
measured, airframe-scoped screws close it, F-16 byte-identical: (1) the Manual-path pitch-deflection cap
`PitchStickMax` and (2) an α limiter allowed to push to recover (both in `systems/FBFlightControl`), plus
the pilot hooks (3) `BfmSearchRollCap` (0.20 — the search is a scan, not a combat roll) and (4)
`BfmRollRateMaxDegS` (60 vs the F-16's 90 — the twitchy K=201 roll overshoots the 10 Hz cap into a PIO).
Each was diagnosed by telemetry before it was turned, each exposed the next (α tumble → mush → search
limit cycle → pursuit PIO). **What it measured.** `mig29-bfm.fbm` (new): full BFM run, no KO, α ≤ 27°,
acquires and locks a trail defender (lock_s 203 / ctrl_s 5.3), deterministic over threads 1/2/4 × 3.
`duel-merge`: exit 2 → 3, the MiG survives (aoaMax 24.6°); the F-16 dominates the angles (lock_s 298 /
ctrl_s 78) but neither converts — a draw, the remaining blocker being gap 4h's ACQUISITION half (RAD
cannot hold the target in a turning merge), not the flying. 13 F-16 BFM/gun/BVR/attack missions
byte-identical; every other MiG mission's exit code unchanged (`mig29-full` touchdown 143.4 → 143.7 kt).
`doc/pilot.md` §5.10, `doc/duels.md` D1, `doc/modules/mig29/module.md` gap 4h.

### One model root and the delta rule (27 Jul)

**What it built.** All flown JSBSim models now live under `sim/assets/aircraft` — `f16` (incl.
`Systems/` and the two referenced engine XMLs, which moved into the model directory as `f16/engine/`:
JSBSim's own per-aircraft layout, which its loaders search first), `mk82`, and the `aim120` that was
already there. `FBModelRoots` has ONE root, `FBModule::FdmModelVendored()` and `FBStoreSpec::Vendored`
have been dropped without replacement, `FBFdm`'s engine/Systems probing (`stat` + parent truncation)
has become two unconditional paths, and the WASM build embeds one root instead of five individual
paths.

**Why.** Principle 1 has moved from "never patched" to the **delta rule**: the pinned submodule is the
base, the copy flies, and every deviation is a named, evidenced entry in `sim/assets/MODEL-DELTAS.md` —
a better mission result is explicitly not evidence. The gate is `make -C sim verify-models`
(`sim/tools/verify_models.py`): canonical unified diff per file, character by character against the
diff block of the entry. Deliberately no `patch`/`git apply` — an application with fuzz could swallow a
deviation.

**Measured.**

| Check | Result |
|---|---|
| Regression, 50 missions | **121/121 telemetry files byte-identical**, all 50 exit codes equal, `events.log` identical except for output path and wall clock |
| `verify-models` | green (4 upstream-covered paths, 0 deltas, 1 FlightBox-own model) |
| Negative test | one changed byte in `f16.xml` → rc=1 with the missing block; likewise a declared but absent delta, a non-matching diff, and an undeclared model directory |
| Harnesses | all seven rc=0; corner speed unchanged at 380 KCAS / 16.2214 °/s |
| Determinism | 5 missions × `--threads 1/2/4` × 2 repetitions = one signature each |
| WASM | builds; JSBSim loads `f16` from the embedded `/fb/aircraft` and trims (`trimConverged=1`) |
| Frame | `gpu_native --mission payerne-takeoff --interval 20` → 28 PNGs, terrain + HUD |

## Defect classes found

What the control loop brought to light that an inspection would not have found. The list is both a
warning and a test pattern.

| Class | Concrete case |
|---|---|
| **Silent wrong values** | `ApplySetup` returned 0.0 for unparsable text and reported success. An HTML error page from the `/elev` endpoint was cached as a sea-level elevation — a whole 216 s mission flew over sea level and reported SUCCESS. |
| **Missing divergence check** | 16 injected NaN cases all ran through with `tripped=0`. |
| **Unguarded calls** | unchecked JSBSim calls → `std::terminate`, exit 134. |
| **Missing header dependencies** | The Makefile had no `-MMD -MP`: stale objects, phantom measurements. Proven by a deliberate header change that altered a telemetry hash and was then reverted. |
| **Architecture leak** | `FBFlightMonitor` knew about runways. Physics K.O. and mission verdict were separated. |
| **Module specifics in generic code** | F-16 references in `FBFlightMonitor`; limits are now derived entirely from the model. |
| **Non-determinism through ordering** | Log line position depended on the scheduler. Solved via merge order instead of locks. |
| **Two copies of the same data** | `sim/web/missions/*.fbm` was a hand-kept copy in the old format — the WASM app stayed black. Now a build copy. |
| **Aliasing through tick rates** | The seeker looked at 20 Hz at poses published at 10 Hz: 446 m/s measured instead of 654 m/s. Solved via a dwell window instead of two single measurements. |
| **Zombie state** | A detonated missile kept radiating for 74 s after its detonation. `Retire()` now clears the signature. |
| **Wrong controlled variable** | A pure P controller against a ramp (the gun solution against a turning opponent) parks at ramp rate × time constant. A point controller against a track has a steady-state cross-track offset. Both are a matter of controller type, not tuning. |
| **Stale documentation in a data file** | Two mission headers still documented "ends in a timeout" after both runs had become kills. The header carries the reading rule and must be maintained with it. |

### Documentation: the spec-driven restructuring (27.07.)

**What it built.** `doc/` moved from "one file per subsystem plus a central TODO" to a
spec-driven shape: every topic file now carries `## Spec` / `## State` / `## Gaps` / `## Knowledge`,
grouped into `sim/`, `aircraft/`, `render/`, `clients/`. New: `vision.md` (the direction),
`roadmap.md` (R1–R10, thin, pointing at the Spec each stage must satisfy), `aircraft/mig29.md` and
`render/units-visual.md` (both spec-only, nothing built), `aircraft/stores.md`,
`clients/clients.md`. `PROGRESS.md` became this file; `TODO.md` dissolved into the Gaps sections of
the files it belonged to, plus `roadmap.md`/`vision.md`. `render/rendering.md` split into
`renderer.md` + `hud.md` + `clouds.md` (whose Spec is the owner-approved rebuild, including the
cirrus layer) + `units-visual.md`.

**Rule change.** The maintenance obligation is now spec-first: change the Spec, build until State
meets it, then update State/Gaps and add a line here (`conventions.md`). There is no second list of
open work any more.

**Not done at the time.** Existing bodies stayed German (each file said so); the translation wave plus
the schema alignment of `doc/modules/f16/` and `doc/modules/mig29/` was roadmap R10. `world-and-terrain.md` stayed at its
old path until the `/wx` round lands, then splits into `world/terrain.md` + `world/weather.md`.

### 2026-07-27 — /wx: worldwide weather on the tile server (`24ac1fc`)

New `/wx` endpoint on fb-tiles: NOAA GFS 0.25°, decoded by an own 330-line GRIB2 reader (wgrib2 is
not packaged in Debian trixie; ecCodes serves as the test oracle — max error 0.5 quantisation steps
over all 20 fields × 259,920 points), delivered as ONE packed 8.3 MB blob per run ("one run is one
atmosphere" — split blobs could straddle a cycle boundary). Byte-identical deterministic builds
across two compilers; the fixture in `tiles/testdata/` doubles as the gym dataset. Poisoned-cache
lesson applied: NOMADS failure writes nothing, ever.

### 2026-07-27 — weather in the simulation (`43b82b5`)

`core/FBWeatherProvider` (calm / constant-wind instrument / FBWX blob from file or memory),
`FBFdm::SetWindNedMs` → `FGWinds` (derivation in the header; only the owner writes, only on change),
`wx` mission declaration (mission always wins; defaults gym/native calm, **browser live**).
Measured: crosswind drift 3.3078° vs 3.2765° derived (0.95 %); uncorrected CCRP in 25 kt crosswind
shifts 12.8 m — far below wind×TOF (127 m) because a 227 kg bomb barely couples laterally in a 10 s
fall; GFS fixture wind recovered from the flown trajectory to 0.12 m/s. All 50 pre-existing missions
byte-identical. Found and open: guidance cannot close a steerpoint inside its drift-widened turning
circle at 18 m/s crosswind (permanent 59° orbit); the 10 m wind anchors at 10 m ASL, not AGL.

### 2026-07-27 — R10: English throughout, schema everywhere (this commit)

The four-part wave: (a) the seven big `sim/` bodies translated (~8,300 lines, zero content loss,
anchors fixed); (b) the rest of `doc/` plus legacy markers on the twelve old cloud
studies; (c) `doc/modules/f16/` on the Spec/State/Gaps/Knowledge schema — producing the first **coverage
map** FlightBox-vs-real-jet (near-full: command bus, HUD symbology, RWR/CMDS; nothing: startup,
displays, HOTAS, refueling; and the surfaced fact that the model flies an F100-PW-229 while the
doc describes the F110); (d) `doc/modules/mig29/` on the schema plus the citation reconciliation — where
the task premise ("uniformly PDF pages") proved wrong: the files were internally mixed, so all 131
DCS-FM citations were scored individually against the extracted PDF text (88 converted, 43 already
printed, re-grep proof 125 printed / 0 PDF). Provenance tags (`[MESS]`/`[ABL]`/`[MODELL]`) stay
German deliberately — they appear identically in code and three doc trees; renaming is only sane as
a coordinated sweep. Remaining German: `world-and-terrain.md` (splits into `world/` in phase 3 of
the mirror refactor) and the pre-refactor `sim/src` paths inside the seven translated files (also
phase 3).

### 2026-07-28 — the first model delta, and the landing that follows from it (this round)

**D1 — the flaperon mixer** (`sim/assets/MODEL-DELTAS.md`, the delta rule's first live entry, and its
first practical test: the emitted block collided with the verifier's own HTML-comment stripping, so
`tools/verify_models.py` now protects the inside of a ```diff fence — otherwise a delta that touches an
XML comment would be undeclarable). `f16.xml`'s flaperon summer carried the flap command
DIFFERENTIALLY and the roll command SYMMETRICALLY, so `fcs/tef-control` cancelled out of
`fcs/flaperon-mix-rad` and twice the aileron command took its place. The correct mixing is derived from
the model's own consumer structure — the mixer's only two consumers, `CLDflaps` and `CDDflaps`, are
symmetric per-radian force coefficients, while the rolling moment travels through `fcs/aileron-pos-rad`
— and the evidence is a physical impossibility the model produced: **+6,420 lbf of forward "drag"** on
a right roll at 350 KCAS. Measured before → after: `flaperon-mix-rad` under a pure roll step
−1.28 → **0.0000**; Nz peak in the roll-in −1.54 g (right) / +3.46 g (left) → **+0.97 / +0.97**; flaps
fully out 0.0002 → **0.349 rad** = the 20° the Flaps channel commands, ΔCL **0.122**, ΔCD **0.028**;
roll rate at 400 KCAS +187.8/−132.3 → **+156.4/−156.6 °/s**, direction asymmetry across 250–600 KCAS
from **55.5 → ≤ 0.2 °/s**.

**Hook cascade, each one re-measured rather than assumed:** corner SPEED unchanged at 380 KCAS, the g
at it 5.6 → **5.4** (`BfmCornerG`), best rate 16.22 → 16.37 °/s (peak moves to 400); 11°-AoA trim speed
165 → **154 KCAS** (`ApproachSpeedKt`). Unmoved and reported as such: `BfmBrakeMs2` (2.531 → 2.527 m/s²
— the flaps only deploy below 250 KCAS, that hook is measured at 325–400) and the ~0.2° cruise
asymmetry (median |φ| on settled route legs 0.186° → 0.185° over 60,900 samples — it is the roll PID's
steady-state residue, not the mixer's; the hypothesis that D1 would fix it is **falsified**).

**The long landing roll.** The deceleration budget named the cause and it was not the model's µ:
JSBSim brakes on `static_friction` (0.8, upper end of dry-runway values), and the measured brake
deceleration is 3.3–3.8 m/s², working correctly. The loss sat between the two-point attitude and the
brake gate. In the aerobrake the wings carry the whole aircraft (wheel normal load **0 lbf** at 12°),
so no brake can bite and the 5,295 lbf of aero drag is the entire budget; the moment the nose falls,
drag collapses to 1,477 lbf. The pilot gated the brakes on `AerobrakeSpeedKt` (100 kt) while the
elevator actually loses the attitude at ~106 KCAS — a **361 m / 6.7 s coast at 0.45 m/s²** in between.
The gate now hangs on the fact instead of the speed: `FBAirframeControls::GetNoseWheelOnGround()`
(the forwardmost bogey's WOW, selected by geometry, `FBFdm::GetNoseGearOnGround`), latched for the
roll-out, exactly as `procedures-landing.md` sequences it. Landing roll at Payerne RWY23:
**1,597 → 785 m** (`payerne-landing`, −51 %) and **1,341 → 928 m** (`payerne-full`, −31 %). Attributed:
D1 plus the new approach speed does 1,597 → 1,039 m and 1,341 → 909 m (the flaps finally give the
two-point attitude real drag), the gate does 1,039 → 785 m on `payerne-landing` and is NEUTRAL on
`payerne-full` (909 → 928 m) — there the nose happens to fall at 99.6 KCAS, so old gate and new gate
fire at the same instant. That neutrality is the point: the gate does not brake EARLIER, it brakes when
the aerobrake is over, whenever that is.

**The approach speed is the honest one, and it costs distance.** With the pre-D1 165 kt the same build
rolls 642 m / 578 m and greases the touchdown (126.7 kt, 0.29 m/s sink) — but it flies final at 9.2° AoA
and floats 38 kt before touching. At the measured 154 kt it flies final at 11.0° AoA and touches at
12.8° AoA, both exactly as `procedures-landing.md` prescribes, at 142.9 kt and 2.96 m/s of sink (peak
gear load 2.05 W against the monitor's 3.0 knockout). The extra 143 m is the price of a procedurally
correct approach instead of a float. What this exposed and did NOT fix: the flare law targets a pitch
ATTITUDE 1.7° above the approach attitude and therefore barely arrests the sink — it had been masked by
11 kt of excess approach speed for as long as the flaps did not work.

**Re-baseline:** 53 missions, 48 verdicts unchanged, five changed and all five explained rather than
papered over — `attack-ccip`/`attack-ccrp`/`wx-ccrp-wind` (the release vertical velocity flips sign
because a roll-in no longer produces a lift step, and the FCC's own table-vs-aero prediction error of
53–64 m stopped cancelling the aim error instead of adding to it: aim error 28 → 80 m), `gun-bfm` and
`bvr-duel-decided` (the BFM/launch geometry rides on the roll behaviour that changed). No mission file
was edited to make any of them green. Determinism 1/2/4 threads identical on five multi-unit missions;
eight harnesses, `verify-models` (green WITH exactly one declared delta, and its negative directions
re-checked), `verify-layers`, WASM + smoke (the corrected gain is in `gpu.wasm`, the old one is not)
all pass.

### 2026-07-28 — the re-tune against the corrected physics (this round)

D1 left five missions on TIMEOUT with a suspended reading rule. All five are back — and none of them by
a number chosen to make them green: each of the three faults it exposed was a real defect that the old,
broken roll authority had been paying for.

**`gun-bfm` — the closure schedule was capped on the wrong measurement.** Attribution first, by running
the CURRENT code against the PRE-D1 model in a scratch tree: over a 16-approach sweep (8 geometries ×
straight/turning defender) the pre-D1 model scores **4/8 straight + 8/8 turning**, post-D1 **0/8 + 8/8**
— the whole regression sits against the STRAIGHT defender, and it is one event: the first stern
conversion now tips the other way and becomes a fly-through that costs the ACM box its contact. Under it
sat the real fault. `BfmBrakeMs2` bounds the closure schedule's cap `a/k`, but it had been measured as
the airframe's LEVEL-FLIGHT deceleration (2.4 m/s², 238 samples) — a different quantity, because a
closure carries the pursuit geometry as well as the drag. Measured on the thing itself (one-second
windows in the conversion, idle + full speedbrake + valid track, N=4,595): **median 1.86, p20 1.16, p90
5.76 m/s²**. A braking LIMIT takes the pessimistic end of its own distribution, so the hook is 1.2 and
the cap 140 → 70 kt. `gun-bfm`: the pursuer used to arrive at 0.5 nm with 105–120 kt against a schedule
asking for 27 and fly through at 0.11 nm; it now tracks at t=59.5 and KILLS at t=66.7 on 70 rounds.
Sweep after: **3/8 + 8/8 = 11/16** against 12/16 pre-D1 and 8/16 post-D1, with mean tracking error
41.1° → 25.5° (straight) and 7.2° → 4.6° (turning). The last kill does not come back and it is named as
such, not papered over.

**`bvr-duel-decided` — the round, not the shot.** The launch geometry is unchanged (24.8 km, the same
beaming defender to within 2° of heading and 30 m of altitude); shooting closer was measured and does
nothing (`pilot_shot_rtr` 1.0 → 0.5 gives 6.25 / 5.35 / 8.11 / 7.52 / 7.20 / 4.03 m — noise, no trend).
What D1 exposed is an instability in the AIM-120's terminal acceleration loop: past ~10 g of demand the
fins ran onto their stops, the integrator wound into a reversal, and the round's own alpha rang (mean
tick-to-tick |Δα| in the terminal phase 0.70°). Two structural fixes, no damage-model change:
**conditional integration** (a fin on its stop cannot answer more integral) and **`kLoopI` 2.0 → 1.5**,
the largest gain on the stable side of the measured boundary (|Δα| 0.698 at 1.75 → 0.139 at 1.50 — an
edge, not a trend). Miss 6.25/7.09 → **2.36 m, one shot, exit 0**. Everything else the round flies got
better with it: `intercept-lostlock` 4.12 → 0.755 m, `damage-amraam` 1.90 → 1.49 m, `cm-beam-only` from
no detonation at all to a 7.83 m hit — which is what its own 2×2 table claims (beam alone leaves the
seeker nothing to be confused by; only chaff AND beam still defeat the shot, and that leg still does).
Two verdicts follow: `cm-beam-only` 0 → 1 and `intercept-lostlock` 0 → 1, both explained in their heads.

**The three attacks — two errors that used to cancel, now separated.** D1's report said the fire
control's own table-vs-aero error (53–64 m) had stopped cancelling the aim error. Measured, the aim error
had a cause and it was not the computer: the pilot set `AtkReleased_` when the pickle was POSTED, so the
escape turn began during the actuation latency and the store left the rail at **32° of bank** and
−0.6 m/s. He now flies the run-in until his own SMS counter says the store has LEFT (roll −0.16°,
vertical +0.01 m/s at separation), and he leads the cue by his own DECISION TICK as well as the bus
latency — between reading a number and pressing lies one slot, worth 21 m at 211 m/s. Result, per
`stores DELIVERY`: `predErrM` 63.8 → **43.6 m** (inside the ~45 m the target requires, and NOT corrected
— it is the declared property), `aimLongM` 78.8 → **40.9 m**, i.e. the release-moment error is now ~0 and
what remains IS the computer's error. `attack-ccip`/`attack-ccrp`/`wx-ccrp-wind` exit 0.

**Rejected, with their measurements** (now in `pilot-ai.md`'s Gaps): integral action on the BFM throttle
— the textbook fix for a P-only loop, and it turns the straight-defender sweep 0/8 → 8/8 while turning
the other one 8/8 → 0/8, because exact speed matching leaves the pursuer at the defender's own 248 KCAS
and his rounds miss by 7–8 m instead of 1.6–4 (the miss is ½·V·ω·TOF², so it is the shooter's speed);
and a turn-rate speed floor meant to replace that accidental energy bias, which does not bind (the
"max-rate" defender actually turns at 5.4 °/s) and binds everywhere the moment the aim error is added.

**Regression, all 53:** 7 verdicts changed — the five targets plus the two missile neighbours above; the
other 46 keep theirs. 27 missions differ byte-wise, in exactly three families: the BFM-phase ones (the
closure cap), the attack/store ones (the release timing) and every AIM-120 one (the terminal loop).
Determinism: 9 runs each (1/2/4 threads × 3) on the five targets → one fingerprint each. Eight harnesses,
`verify-models` (green, still exactly one declared delta), `verify-layers`, `nm` (0 GPU symbols in
`fb-gym`), native + WASM green, and the WASM A/B is decisive: `gpu.wasm` built from this tree carries one
more `1.2` double than one built with the old hook, and the two binaries differ. Proof frame:
`gpu_native --mission attack-ccip.fbm --interval 20` → SUCCESS, bunker DESTROYED, eight PNGs.

**Left stale on purpose:** `doc/weapons.md` §10.2's gain table still prints
`kLoopI = 2.0` and does not mention conditional integration — that file was outside this round's write
permission.

### 2026-07-28 — R5 clouds merged (`9ca2c0e`), MiG-29 stage 1 merged (`b411b2b`)

**Clouds:** the rebuild per the approved Spec — ONE bounded-volumetric stage, one separable density
function evaluated in C++ AND WGSL (constants printed from the C++ side, max |Δ| 1.87e-5 over 12,288
samples), the six FBCloud* stages and the tonemap's second pipeline deleted. Cost measured worst-case:
8.8 ms full-res vs ~23 ms of the old quarter-res+temporal chain — 2.6× cheaper at 4× the marched
pixels. Weather-driven via FBWorld::Weather() (no weather ⇒ no cloud pass, 6/7 passes). Five proof
sets incl. seamless fly-through and cirrus fibres along the real 250 hPa wind (3.8° residual).
Merged on its own branch against `ab40bac` by a dedicated agent (deletions win over namespace edits;
`--wx` is the screenshot venue's weather, mission venue reads the .fbm — combining both is now an
argv error). Known gaps: stored proof PNGs are stale vs the committed source (predecessor tuning
drift — re-capture wanted), march grain 0.04–0.08 by design, one ceiling clamped into three decks.

**MiG-29 stage 1:** the model exists — `sim/assets/aircraft/mig29/` (FlightBox-own, GPL-2.0-or-later,
every table tagged INV/GEO/ANALOGY/SET) plus `make test-mig29` measuring 23 anchors. 10 hit or in
band (Vmax SL +0.2 %, rotation/liftoff/ROC in band), 4 missed with diagnosis instead of anchor-fitting:
Ps SL −24.8 % is the borrowed thrust analogy (needs aug factor 1.16, F100 surface gives 1.02 — NOT
drag-closable without destroying Vmax SL), ceiling +8.7 % same family, takeoff run +29.8 % is the
spec's own §12.3 doubt. Roll rate 241 °/s declared a model property — no anchor exists at any tier.
Two JSBSim findings for the house: FGTrim drives `pitch-trim-cmd-norm` (a pitch channel without that
summer cannot trim at all), and linear table interpolation overstates a quadratic drag rise ~4.5× at
the first breakpoint. F-16 untouched (corner numbers byte-identical).

### 2026-07-28 — Phase 3 of the mirror rebuild: `doc/` becomes `sim/src/`

The documentation is now a **1:1 mirror of the source tree**. `doc/flightbox/` is gone; the seven meta
files sit at the root beside `core.md` / `fdm.md` / `systems.md` / `sensors.md` / `weapons.md` /
`pilot.md`, and the four subdirectories `missions/`, `modules/`, `render/`, `world/`, `clients/` carry
the same names as their source directories. Every move was a `git mv`, so the history follows.

**The two splits, both with translation** (the last German prose in `doc/`):

- `mission-format.md` → `missions/` — nine files (`INDEX`, `syntax`, `verdict`, `sensors`, `avionics`,
  `weapons`, `combat`, `weather`, `output`) plus `units-and-missions.md` → `missions/runtime.md`. Each
  new file carries the Spec/State/Gaps/Knowledge frame; the leading rules (exit codes, "a mission
  file's header comment is a binding reading rule") live in `missions/INDEX.md`.
- `world-and-terrain.md` → `world/terrain.md` (§1–§8) + `world/weather.md` (§9). The two points the
  roadmap had parked for exactly this split — DEM cache per worker instance, imagery mode not
  declarable in `.fbm` / TLS not wired — got their home in `world/terrain.md`'s Gaps. **The Parked
  table is now empty.**

The two reference bases moved under their module (`doc/f16/` → `modules/f16/`, `doc/mig29/` →
`modules/mig29/`), each now sitting beside the `module.md` that implements it; the cloud studies became
`render/clouds-legacy/`. **One skill instead of three:** `f16-systems` and `mig29-systems` are deleted,
their routing tables absorbed into `.claude/skills/flightbox/SKILL.md` as "The module reference bases".

**Path sweep:** 209 relative links inside `doc/` re-resolved against their new locations, plus 475
plain-text mentions across `doc/`, `CLAUDE.md`, the `.fbm` headers, `sim/tools/`, `sim/assets/`,
`tiles/` and ~150 comment banners in `sim/src/**`. Comment banners only — the three CLI usage strings
that name the format were deliberately left, because touching a string literal would move the
`strip_comments` hash. That hash is unchanged (`8d85837e…`, 233 files), the link check over every `.md`
is clean, `core-lib`/`gym` build, and three mission samples run byte-identically.

### 2026-07-28 — two value gaps: the wind orbit and the roll-limiter fixed point (this round)

**A — a steerpoint the guidance cannot close** (`doc/systems.md` §7.5.1). A capture circle is a GROUND
test of fixed radius; the circle the aircraft can fly lives in the AIR MASS, and a fix WITHOUT a leg is
flown by the bearing law, which controls the nose and not the ground track. New instrument
`missions/wx-orbit.fbm` (the GFS fixture's 9,000 m wind as the closed form `wx wind 338 39`): closest
approach **614 m** against a 500 m circle, then a permanent limit cycle — 1,793…4,851 m, −59.1° bank,
99.2 s per lap; the same file in calm captures the same fix with **4 m** to spare. Answer: a THIRD
sequencing ground, `orbited` — two failed approaches (closest approach, opened by more than the capture
radius, closed again, opened again) — bound to the SUCCESSOR as `passed` is bound to the predecessor, so
the deliberate terminal orbits of `bfm-basic`/`gun-turning`/`bvr-duel` are out of scope by construction
(re-measured: bandit `activeWp` 0 for the whole run, zero `WP_REACHED`). Threshold 2 is measured, not
chosen: at 1 the attack missions sequence their target fix out of the egress at t=87.9 s. Both
authorities state it independently, both fired at t=311.6 s; `wx-orbit` SUCCESS at t=485.4 s. All 53
pre-existing missions byte-identical.

**B — the roll limiter had no fixed point** (`doc/pilot.md` §5.7). `cmd_prev·cap/rate` is not a limiter:
linearised against the identified plant it is `z² − 2az + a = 0`, i.e. an oscillator with **|z| = √a**,
and it held **1.52 ×** its own declared cap over the 16-approach sweep (pooled autocorrelation of the
rate while active: first recurrence 0.70 s). Replaced by a memoryless ONE-STEP PLANT INVERSION off an
ARX(1) identification (15,325 samples below the cap, open loop: a = 0.734 / τ = 0.323 s, K = 78.7
°/s per stick) — 1.23 × at the same cap, and stretches ≥ 4 s above 0.8 × cap 11 → 0. The cap itself
became a closed form: the largest error this law can command is 180°, flown in the time constant the
roll serves → **90 °/s**, with `kBfmReverseS` falling out identically `kBfmTurnTimeS`. Re-measured over
six cap values × 16 approaches, 90 is also the measured optimum (12/16 against 8/16, and the only value
with no departure in the eight committed BFM missions); the control with the limiter removed scores
7/16 at 132 °/s peak. New instrument `missions/bfm-pointblank.fbm` (0.8 nm head-on, the swinging
stimulus): 1.37 × → **0.89 ×** the cap, 9.2 s → **0.0 s** above it. Costs, all declared in their
headers: `gun-dry` 3 → 1 (all twelve rounds now arrive), `gun-bfm` kill 66.7 → 84.2 s, `bfm-blind`'s
blind interval 41 → 199 s (chaotic across every cap tested), one departure in a non-committed sweep
geometry. Exactly five missions move, all BFM; nothing else in the tree changes by a byte.

### 2026-07-28 — MiG-29 stage 2a+3: the module flies end-to-end (merge of `b3da424`)

`sim/src/modules/mig29/` (module, pilot numbers, damage zones, registry name `mig29`) plus four
missions; `mig29-full` flies takeoff, route and landing autonomously to a stop on the Payerne
threshold (exit 0, 730.6 s; rotation 130.1 kt, touchdown 143.4 kt at 11.66° AoA and 3.59 m/s).
`mig29-pair` proves two DIFFERENT modules in one formation. The FBW preset is its own for a
structural reason: behind the g output the F-16 has an FLCS, here the output IS the deflection.
Three measured failures stand in the preset comment and determine it (saturating yaw → LOC t=28 s;
double-integrator limit cycle, 20 s period; no α limiter → α 90°, LOC t=122 s). The SOS limiter is
thereby built where `flight-model-spec.md` §7.3 placed it, behind one preset number. `test-mig29`
gained the two measurements the module cites: 136.8 kt at the documented 11° touchdown α, and corner
420 kt / 24.18 °/s / 7.83 g. F-16 byte-identical across all 53 stock missions.


### 2026-07-28 — MiG-29 stage 4: the asymmetric duel as a measurement campaign (this round)

**What the round was for.** Everything since stage 1 existed so that two DIFFERENT aircraft could meet.
[`pilot.md`](pilot.md) gap 2.3 had recorded for three rounds that the symmetric F-16 duel is a
stalemate by construction, and `modules/mig29/module.md` had said in as many words that the MiG exists
to turn the coin toss into a choice. This round is the measurement that says whether it did.

**What it built.** Eight missions (`sim/missions/duel-*.fbm`), an analysis tool
(`sim/tools/fb_duel_report.py`), a `module=` key on the tournament so a variant file can pit an F-16
doctrine against a MiG doctrine, and a new topic file [`duels.md`](duels.md) — which is a family of
MISSIONS rather than a directory of source, and the first entry in `INDEX.md` that is not a mirror of
`sim/src/`.

**The answer, and it was not the expected one.** Neither side structurally dominates; the launch
DOCTRINE does. With both pilots on the shipped rule (shoot at Rtr) five of five BVR geometries draw —
head-on, 50° offset, 6,000 m to either side, EMCON — because the two Rtrs sit within half a mile of
each other (AIM-120 9.78 nm, R-27R 10.25) and every round then arrives outside its warhead's lethal
radius. Change the rule on one side and the same geometry decides, and what each side needs is
different: **the MiG needs only the early launch** (`duel-doctrine-mig`, exit 0 — R-27R away at
14.41 nm, 25.8 s of unbroken illumination, 9.35 m detonation, the F-16 defensive 1.5 s before its own
trigger and never firing), **the F-16 needs the early launch AND 6,000 m** (`duel-doctrine-f16`,
exit 0 — its early launch alone is 10.7 s ahead and still draws at 4.79 m; from 6,000 m higher the
identical decision arrives 1.77 m out and kills).

**Three AI defects, all found by measuring, all fixed.** The GCI entry chain advanced on the POST
rather than on the acknowledgement, so the one entry that makes the N019 exist could be lost to a
single g-loaded tick (measured: 400 s of a duel flown blind). The intercept antenna was centred on a
COASTED look while the jet's own attitude moved, freezing a ±6° bar after one look through a 6,000 m
descent. And `FBMig29Pilot::InterceptSpeedKt` was a unit error — a CAS derivation fed to a TAS command
— that had the MiG cruising to every BVR merge at 217 KCAS / M 0.54, 40 % below its own departure
speed. **That last one is the round's second finding:** with it in place the F-16 won four of the five
BVR geometries outright. Correcting it turned all four into draws, i.e. most of the F-16's apparent
BVR dominance was a MiG tuning error rather than a weapon-system difference.

**Measured.** 66 of 69 stock missions byte-identical, all 69 exit codes unchanged; the three that
moved are `bvr-duel` and `bvr-duel-decided` (one to two extra antenna slews — `cmd_*` counters, plus
2.9 s in which one jet's RWR carries an extra SEARCH-class contact behind it that nothing acts on) and
`mig29-intercept` (same exit code and verdict, everything earlier and tighter: kill 87.7 → 78.1 s,
miss 1.13 → 0.34 m). No flight-state column and no verdict moved anywhere. All eight duels one fingerprint over
`--threads 1/2/4` × 3. The mixed tournament decides 12 of 30 runs where the symmetric one decides
**0 of 30**, and the early launch is worth an entire outcome band on the MiG (−393.7 → +585.0) against
nothing on the F-16 (601.8 → 603.3) — the same asymmetry the named missions found, reproduced by a
fitness written before the campaign existed. Open, and now with numbers: the MiG's close-combat law
DEPARTS the airframe in 22.8 s from a nose-on merge (`duel-merge`, kept as a reproducer), and an
AIM-120's terminal miss runs 1.37 → 7.66 m as closure runs 744 → 1053 m/s, which against a 1/r² damage
model is the difference between a kill and a jet that flies on.


### 2026-07-28 — MiG-29 stage 2c: the weapons and the signature

**What the round was for.** The MiG-29 had sensors and no weapons; the F-16 had no infrared round at
all; flares had been dispensed and counted since the countermeasure round with nothing to work on; and
`RADAR_DESIGNATE` was unreachable because the intercept pilot correctly disengages from a target it
cannot shoot. All four are the same missing piece, and it is the SEEKER.

**The one architectural idea.** A guided round is still ONE module and N catalogue entries; what makes
an AIM-120, an AIM-9 and an R-27R three different weapons is `FBSeekerKind`, and each kind names a
derivation of a SENSOR SLOT THAT ALREADY EXISTS. The infrared seeker is an `sensors/FBIrstSystem`, so
it inherits the aspect law, the afterburner term, the cloud deck and the anonymity, and the perception
boundary does not grow by a file (`verify_layers`'s `RESTRICTED` list is unchanged — the scan lives in
the base). The semi-active seeker is an `sensors/FBRadarSystem` that never transmits. Two seeker kinds,
no new architecture, and the tactical differences fall out of the sensors' own limits.

**The measurements that decided things.**

- **Flares now work, deterministically.** One inequality between two received irradiances in one unit
  (a clean airframe seen dead astern = 1.0), so the ASPECT does the whole job: head-on and dry an
  aircraft radiates 0.16 and a cartridge beats it six times over; astern in afterburner it radiates
  2.25 and cannot be deceived. Both branches measured on BOTH airframes at exactly `tgtIntensity=0.16`
  — the same number from the same code — and the decoyed rounds miss by 22.8 m (R-73, 3.5 m fuze) and
  25.96 m (AIM-9, 6.0 m fuze).
- **The semi-active penalty, as a number.** 28.56 s of unbroken illumination for one R-27R shot against
  the AIM-120's 5-15 s; break the lock in flight and the round misses by 27.04 m where an AIM-120 with
  the same loss still hits by 0.755 m.
- **The RCS calibration is the identity for the F-16.** `σ^¼` scaling with the F-16's own 1.2 m² as the
  reference, so all 55 F-16 missions came out byte-identical on every column and every event, and the
  asymmetry (1.351× / 0.740×) exists only across types.
- **30 mm is a different weapon in the same currency.** A kill on 67 of 150 rounds at 294 m of round
  path; the FULL drum at 571 m wipes the target's avionics without downing it. The documented
  200-790 m effective band emerging from the dispersion model rather than from a range limit.

**Three defects the measurements found**, each fixed where it belonged rather than where it showed:
the MiG's gun never learned its own unit id and therefore shot ITSELF down at the muzzle (the runner's
shooter exclusion compares `LauncherId`); `FBFlightControl` returned before its alpha limiter in
`Manual`, so every hand-stick phase on an airframe whose deck has no limiter was unbounded — invisible
until BFM became the first phase that really pulls; and the BFM roll-rate cap inverts a PLANT, so with
another aircraft's constants it is an oscillator rather than a cap (identified for this airframe:
a = 0.819, K = 201 °/s against the F-16's 0.734 / 78.7).

**One long-standing gap closed by measuring instead of arguing.** The MiG's corner formula read −16 %
against the harness. Neither hypothesis survived: the altitude loss inside the window is worth +1.7 %
and the convexity of `√(n²−1)` +0.4 %. The harness was measuring the rate of the body's EULER HEADING
while the formula predicts the turn rate of the VELOCITY VECTOR, and at 22.7° of incidence in an
85°-banked pull those differ by 18 %. Measured directly, the formula is right to **1.4 %** — better
than the F-16's own −2 %. The correction went to the harness's reporting, not to the formula.

**What is honestly not finished.** The MiG-29 has no dispensers at all (no source states the
BVP-30-26's programme parameters), so the flare asymmetry currently runs entirely one way. And its BFM
is unfinished: the N019's close-combat modes are pencils in azimuth and its wide mode does not
auto-lock, so a manoeuvring MiG cannot acquire — 0 contact ticks in 134 s, measured.
`FBPilot::BfmDesignate` gives the pilot the thumb he needs (a no-op on an auto-locking set), but the
cold-search law still rolls the jet before the first two looks land. `mig29-gun` therefore measures the
WEAPON from a stable position with a briefed burst, and says so in its header.

### 2026-07-28 — MiG-29 stage 2b: the sensors and the guidance

Three real sensor derivations, **one new generic slot**, and GCI as mission data.

**`sensors/FBIrstSystem` is the fourth sensor slot and the fifth file allowed to read
`units/FBUnitRegistry`.** The boundary was never a COUNT — it is "only simulated sensors, each paying a
stated price" — and the widening is recorded where it is enforced: `tools/verify_layers.py`'s
`RESTRICTED` table FAILED on the new include until the file was added to it by name. An IRST pays in
range (25 km at best against the radar's 50), in identity (no interrogator, and `core/FBIrstContact` has
no field one could be put in) and in weather, and gives back the one thing no other sensor here does:
it costs the observer nothing to look.

**Two generic constants became hooks, both defaulting to the previous behaviour exactly.**
`DopplerNotchMs(rangeM)` + `NotchRejectsDetection()` (until now the notch was ONLY chaff's channel — a
target in the filter stayed visible; a set whose source QUANTIFIES the threshold now rejects) and
`CoastS(volume)` (the N019's source names a duration, not a frame count). The RWR grew four:
`Blanked`, `ReportBearingDeg`, `ClassifyMode`, `PriorityRank`. `FBUnitSignature` gained
`Afterburner`, read off JSBSim's own `FGTurbine::GetAugmentation` rather than off a throttle position.

**Measured against the documented numbers** (four new rigs, all TIMEOUT by construction): detection
latency **6.0 s** (the derivation runs the other way — the documented "up to six seconds" over the
generic two-look firming IS the 3.0 s frame time); the Doppler envelope rejecting at **7.94 m/s vs
41.67** beyond 8 nm and **4.34 vs 16.668** inside 5.4 nm; **`coastS=6`** inertial tracking; the SPO-15's
forward hemisphere going dark in the SAME tick ILLUM is acknowledged, with the emitter's `fcr_on`
unchanged, and its bearings reported as channel centres (−10.0° where the F-16 reports 0.045°); the IRST
aspect law separating a tail-on detection at **19 562 m** from a 103°-aspect one at **15 222 m**; the
6 km laser stepping `irst_lock_nm` from **−1** to 3.199 nm; a target above a GFS deck never detected
(`irst_masked`, the first tactical weather effect on a sensor here); and the GCI chain taking **8.0 s**
from the controller's call to a radiating radar, with the opposing RWR lighting up 0.1 s later.

**Two defects found by building the rigs, both fixed and both measured.** (1) A set powered up mid-run
replayed its whole silent period through the catch-up guard and reported a firm track in the tick the
switch moved (t=27.9 instead of one frame later) — `ResyncScan()`, opt-in, so the F-16 is untouched.
(2) Timing the SPO-15's documented 125-250 ms illumination event classified EVERY search emitter as
tracking, because the emission model publishes a searching beam as continuous (`mig29-pair`, t=0.3:
the F-16's CRM sweep reported as TRACK). The event half of that rule now waits for a pulsed emission
model; the channel half — the actual device defect — is what the override contributes.

`set task intercept` is unlocked for this module, and the honest outcome is that the intercept
DISENGAGES on first contact: `pilot/FBPilot`'s own rule is "a target on the scope and nothing on the
rails → Abort", and this jet has no weapon yet. F-16 byte-identical across all **56** stock missions on
every column they ever had; the four MiG missions move exactly once, because the N019's power-up
emission position is OFF and this aircraft now starts silent by doctrine.
