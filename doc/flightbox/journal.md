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
| FDM adapter | **finished** — instanceable, IC-sealed, damage and stores channels | [fdm.md](sim/fdm.md) |
| Core / avionics bus | **finished** — typed blocks with three-state validity, command bus with acknowledgement | [core.md](sim/core.md) |
| Mission orchestrator | **finished** — four steps, no mission knowledge in the code | [units-and-missions.md](sim/units-and-missions.md) |
| Multi-unit | **finished** — formation as mission data, thread per unit in the gym, deterministic | [units-and-missions.md](sim/units-and-missions.md) |
| Sensors | **built** — datalink, radar, RWR, countermeasures. Without terrain masking. | [sensors.md](sim/sensors.md) |
| Weapons | **built** — AIM-120, Mk-82, M61A1, ground targets, damage model | [weapons-and-damage.md](sim/weapons-and-damage.md) |
| Pilot AI | **in progress** — takeoff/route/landing, BFM, BVR intercept, air-to-ground all fly; refinement ongoing | [pilot-ai.md](sim/pilot-ai.md) |
| Renderer | **built** — stage split complete. Units and weapons still invisible. | [rendering.md](render/renderer.md) |
| HUD | **built** — generic default HUD + full F-16 symbology, coverage AA | [modules-f16.md](aircraft/f16.md) |
| Cockpit displays | **not started** — the values are on the bus, the presentation is missing | [clients/clients.md](clients/clients.md) |
| HOTAS | **not started** — deliberately last, it is only a mapping | [clients/clients.md](clients/clients.md) |

## Chronology

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

`2dd1142`, `e22f228`, `c4e96e7` — the official ED documentation distilled into `doc/f16/`.
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

**What it built.** `doc/flightbox/` moved from "one file per subsystem plus a central TODO" to a
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

**Not done.** Existing bodies stay German for now (each file says so); the translation wave plus the
schema alignment of `doc/f16/` and `doc/mig29/` is roadmap R10. `world-and-terrain.md` stays at its
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
anchors fixed); (b) the rest of `doc/flightbox/` plus legacy markers on the twelve old cloud
studies; (c) `doc/f16/` on the Spec/State/Gaps/Knowledge schema — producing the first **coverage
map** FlightBox-vs-real-jet (near-full: command bus, HUD symbology, RWR/CMDS; nothing: startup,
displays, HOTAS, refueling; and the surfaced fact that the model flies an F100-PW-229 while the
doc describes the F110); (d) `doc/mig29/` on the schema plus the citation reconciliation — where
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

**Left stale on purpose:** `doc/flightbox/sim/weapons-and-damage.md` §10.2's gain table still prints
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
