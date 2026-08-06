# Pilot AI — FBPilot, FBBfmTrack, FBEngagement, FBPilotTuning

**Source state:** commit `9673e00` ("guidance holds a path where a path is declared").
The primary sources are the comment banners of the source code:

| File | Role |
|---|---|
| `sim/src/pilot/FBPilot.h` / `.cpp` | the decision layer: phase machine, brief, BFM control law, Attack, Intercept |
| `sim/src/pilot/FBBfmTrack.h` / `.cpp` | the target picture from radar contacts + the BFM scoreboard + `FBTrackDatum` |
| `sim/src/pilot/FBEngagement.h` / `.cpp` | the BVR state machine as data + the debriefing |
| `sim/src/pilot/FBPilotTuning.h` / `.cpp` | the pilot VARIANT as mission data |
| `sim/src/modules/f16/FBF16Pilot.h` | the F-16 numbers (all virtual hooks) |
| `sim/src/modules/f16/FBF16Module.cpp` | rate wiring, `set` keys → brief/task/variant |
| `sim/tools/fb_tournament.py` | the tournament runner (not a build target) |
| `sim/tools/fb_bfm_sweep.py` | the **16-approach BFM/gun sweep** the roll law is measured against (§5.7.2/§5.7.3) — not a build target. It generates its 16 `.fbm` cells and prints kills, departures and the pooled roll statistics; cell `trail2-str` reproduces `gun-bfm.fbm` to the digit |
| `doc/missions/combat.md` §§ combat/intercept/attack missions, pilot variants | the mission data side |

Convention in this document: **[MESS]** = measured on the pinned model or in the mission control loop,
**[HERL]** = derived from a named equation, **[SETZ]** = set (a design decision without a source number),
**[DOK]** = from `doc/modules/f16/`.

---

## Spec

The mission layer above guidance and flight control: the pilot decides WHERE, the autopilot flies the
manoeuvre, the FBW moves the hands.

| Contract | Acceptance / measurement anchor |
|---|---|
| The pilot decides at ~10 Hz over 100 Hz FDM substeps | the module throttles the slot like any other |
| The pilot sees other units only through `FBState` | he holds no registry, no `FBWorld`, no system pointers |
| **What the pilot LOOKS AT is an operating action like any other** | the page on the cockpit's attention display changes because `FBPilot::SelectCockpitPage` posted `FBCommandTarget::MfdPageSelect`, with the ordinal read out of the published catalogue (`FBMfdBlock`) — he knows page ROLES, never a jet's page numbers, and a role his cockpit does not offer he cannot choose. Rank: somebody's round in the air > a warning on the aeroplane > the phase's own job. Acceptance: a `CMD_ISSUE`/`CMD_ACK` pair per switch and no other write path |
| The pilot operates avionics ONLY through the command bus | with human reaction time on top and the risk of rejection; what he enters is his BRIEF (`brief_*` mission lines) |
| Without a brief he operates nothing | a mission that does not brief a switch gets the boot state |
| Phases are the procedure skeleton | Idle/Preflight/Takeoff/Climb/Route/Approach/Flare/Rollout/Shutdown + Attack/Bfm/Intercept/Formation, per `doc/modules/f16/procedures-*.md` |
| A pilot in a declared FLIGHT sorts, stations and covers | phase `Formation`, the assignment inside `Intercept` and the cover deferral; all three are no-ops without a `flight` line in the mission. Contract, derivations and measurements: [`formation.md`](formation.md) |
| Airframe numbers are hooks, pilot numbers are not | corner speed, brake authority etc. are virtual airframe hooks; reaction time and operating cadence describe the PILOT and stay fixed |
| The close-combat law (phase `Bfm`, `Guidance = Manual`) must be survivable on a RAW airframe | an airframe whose JSBSim deck carries no FLCS (the MiG-29) is protected only by what `systems/FBFlightControl` adds. The law commands raw stick, so EVERY device that class models for the airframe — the α limiter, the pitch-deflection cap `PitchStickMax` AND the rate damper `KqDamp`/`KpDampRoll` (the SAU-451 DAMPER) — must bind on the `Manual` path exactly as it binds on the FLCS path, because each is a property of the aeroplane and none of them knows which mode the autopilot is flying, and the commanded g may never exceed the α-limit-sustainable g at the current speed. Acceptance: `missions/mig29-bfm.fbm` flies a full BFM run with no `FBFlightMonitor` KO AND holds a control position (`bfm_ctrl_s` > 0), and a MiG-29 alone in a cold BFM search does not sink into the ground; the F-16 (`PitchStickMax` 1.0, its own deck FLCS) is byte-identical |
| The close-combat law may command a roll rate, never a sustained one | the judge's rule is a SUSTAINED body rate (|ω| > 60 °/s for 3 s), so a peak cap alone is not a bound. The commanded roll carries an EXTENT bound over the same window the peak is derived in — no more than one reversal (180°) per `kBfmTurnTimeS` — and the sustained fixed point that follows is half the peak. Acceptance: no departure over the 16-approach sweep, and `duel-merge` flown to its end instead of to t = 18 s (§5.7.3) |
| **A trigger squeeze is predicted to the moment its rounds LEAVE, never to the moment they arrive** | `FBGunSolveLead` answers "where must the bore point for a round fired NOW to meet the target LATER" — the target's motion over the time of flight is already inside the solution, so a round that has left cannot benefit from the aim improving behind it. The horizon of §5.8's prediction is therefore the bus latency plus half the squeeze, and `fc.GunTofS` has no place in it. Acceptance: `R·tan(GunAimErrorDeg)` read at the tick a bundle left predicts that bundle's own closest approach, and no squeeze is commanded from outside the funnel gate the fire control publishes |
| **The close-combat phase can employ the round on the rail, and the round decides how long it binds** | `Phase::Bfm` offers an infrared shot on five instrument readings and no new arithmetic (§5.11): the selected store is an IR round, a lock exists, the fire control's own launch zone says in-zone, the target is inside the CUEING limit of this aircraft, and the previous shot has had its time of flight. Aspect is not a gate (both rounds are documented all-aspect) and own g is not a gate (nothing in the release path models a rail load). An infrared round's `FBSeekerHandoverS` is 0, so the phase is unchanged after the release: no Support state, no illumination, nothing held. Acceptance: a merge decided by an IR round from EITHER seat, and every mission without a WVR round on a BFM task byte-identical |
| **A pilot at minimum fuel breaks off, and the question is asked where it can be reached** | the BINGO bit is the only fuel signal this pilot has, and it says a number he committed to himself. At it he goes `Abort` — turn cold, 180° from the last thing that pointed at him — and logs `intercept BINGO_ABORT` once. Acceptance: a mission in which the break-off happens, against the identical mission with the `brief_bingo_lbs` line deleted (§7.4a) |
| **The rank of the fuel question is: survival, then the shot in the air, then fuel, then the picture** | `defendDue` (somebody's round is in the air at ME) outranks it — a jet does not stop defending because of fuel. An unfinished SUPPORT (my round still needs my uplink) outranks it — leaving would throw away the shot that was already paid for. Everything else ranks BELOW it: "no target on the scope", "target in lock range", "too close to still be BVR" are statements about the picture, and fuel is a statement about the aeroplane |
| **The defence hold OWNS the state while it runs** | `Defend` is left by exactly one branch — its own timer — and the general chain may not take the state away underneath it. Without that, `now − IntThreatLastS_` is one tick at the moment the threat symbol goes out, the hold never elapses, the beam is broken off at exactly the instant it is working, and `CanPressOn` (behind that branch) is unreachable code. This is a **branch ORDER**, and reordering it changes every mission with a combat in it: the list, the diff and the per-mission reason are the acceptance, split into *reports differently* and *flies differently* |
| Only ONE of the three resumption instruments is lifted out of `CanPressOn` | fuel is a fact about the airframe and is asked everywhere. "Radar not radiating" is a briefed TACTIC (EMCON) and asking it outside a resumption would send every EMCON jet home; "racks empty" already has its own branch with its own stated exception ("an abort for empty racks before anything was ever seen is simply a jet that leaves"). Acceptance: no EMCON mission and no unarmed-CAP mission moves |
| A pilot variant is mission text, not a class | `set pilot_*` → `FBPilotTuning`; an unset entry means "this pilot's own number", and a mission without `pilot_*` flies byte-identically |
| Every fitness channel is computable from OWN perspective | `bfm_*` / `eng_*` telemetry — the basis of the evolutionary tournament |
| A measured failure stays documented | rejected approaches remain under Gaps with their measurements |

## State

In progress — takeoff, route, landing, BFM, BVR intercept and air-to-ground all fly autonomously;
refinement is the running work.

| Piece | Status | Anchor |
|---|---|---|
| Framework, phase machine | built | `681c5f8` |
| **The commander's inbox** (`ReceiveOrder` / `ConsumeOrders`) | built | ONE order per decision tick, four outcomes, every one a line in `events.log`. An order whose act is a HEAD-DOWN ENTRY (`waypoint`/`steer`/`attack`) is not finished when it is posted — it is posted to the steerpoint page in the DED class and finished when `FBCommandBus::AckOf` answers, which brings the manoeuvre gate with it: above 1.5 g the bus turns it away and the commander sees a refusal. **`attack` is refused `nothing_held` unless one of THIS pilot's OWN radar contacts is within 4 000 m of the ordered point** — the commander points, the pilot must see. `emcon` is refused `no_capability` on an airframe with no silent radar mode. [`player-layer.md`](player-layer.md) §12 |
| **Fire authority** (`SetWeaponsControl` / `MayFire`) | built | the doctrine word the air-defence net already transmits, on an aircraft: `Free` is the built behaviour exactly (so a unit nobody put under fire control is byte-identical), `Hold` refuses at the ONE gate every weapon-employment post passes, and `Tight` is refused because target addressing does not exist in this tree. `SetAutonomy` + `SetControlNodeHeard` give the declared fallback when the node goes quiet — the ground net's own mechanism, reused unchanged |
| Takeoff | flies | `e49d335` |
| Landing — `payerne-full` flies fully autonomously | flies | `8cd3a74` |
| BFM manoeuvre AI on radar contacts alone | flies | `b375bef` |
| Intercept: BVR tactics — lead, shoot, support, defend | flies | `1ecd433` |
| Objectives + evolutionary tournaments | built | `82df2e2` |
| Pilot memory: the datum instead of the last measurement; gun tracking with rate term; roll-rate limiter | built | `cac7b62` |
| Guidance holds a path where a path is declared | built | `9673e00` |
| Brake-authority-derived closure cap (`BfmBrakeMs2`), conversion rule, absolute-value trigger | built, **not yet distilled below** | `658014d` |
| Re-tune against `MODEL-DELTAS.md` D1: `BfmBrakeMs2` re-measured as a CLOSURE decay (2.4 → 1.2 m/s², cap 140 → 70 kt); the attack release holds the run-in attitude until the store has LEFT and leads the cue by the decision tick as well | built | `658014d`+ |
| **The antenna is held against one's OWN attitude change while a contact is coasting** (§7.5) — the reported elevation is body-referenced and was measured at the attitude of the LOOK. Found by the asymmetric duel campaign; a fresh look is unchanged to the bit | built | this round, [`duels.md`](duels.md) M2 |
| **The close-combat law survives a RAW airframe (MiG-29).** Four screws, each measured, F-16 byte-identical: (1) the airframe pitch-deflection cap `PitchStickMax` and (2) an α-limiter that may PUSH to recover both now bind on the `Manual` path of `systems/FBFlightControl`, not only its FLCS path; (3) a search-only roll cap hook `BfmSearchRollCap` and (4) a commanded roll-rate cap hook `BfmRollRateMaxDegS` keep the twitchy MiG (K=201) from a search/pursuit roll limit cycle the monitor reads as a departure | built | `71cb99f`, [`duels.md`](duels.md) D1 |
| **The FLIGHT** — roles as mission data, the wingman's station on a moving point, target sorting from the shared picture, and the cover rule that keeps one member free. `pilot/FBFlightPicture` is `FBBfmTrack`'s sibling: built from `FBState` blocks only. Measured: 45.2 m straight-leg station error, 93 % target split in a cooperative pair, 0.962 vs 0.750 distinct targets per shooter cooperative against contract, 7.8 s of measured shot deferral, all 79 stock missions byte-identical | built | this round, [`formation.md`](formation.md) |
| **The roll-rate cap became a bound instead of a peak** (§5.7.3). A high-closure merge (898 kt, LOS rate 543 °/s) rotates the commanded lift direction with the aircraft, so the roll error never closes and the law rolls 290° in 3.1 s on a 10–20° steering error. The same 180° that gives the peak cap now also bounds the roll flown per `kBfmTurnTimeS`; the sustained rate follows as `cap/2`. `duel-merge` no longer departs the F-16, sweep departures 6 → 0, 73 of 79 missions byte-identical | built | this round, [`duels.md`](duels.md) D1 |
| **The airframe's own rate DAMPER binds at the hand stick too** (§5.10a) — the fifth screw, and the one that was killing the MiG in the merge. `KqDamp`/`KpDampRoll` model the SAU-451 DAMPER and were FLCS-path-only, while BFM commands `Manual`: the jet fought every close engagement undamped. [MESS, one MiG alone in a cold BFM search, 300 s] mean `bfm_gcmd` 4.57 → 1.11, mean bank 76° → 24°, p95 \|VS\| 183 → 4 m/s, CFIT from all three start altitudes → none (F-16 on the identical search: 1.22 / 40° / 9 m/s). `duel-merge` exit 2 → 3, `mig29-bfm` `bfm_ctrl_s` 0.0 → 287.6 s. 134 of 139 missions byte-identical; the five that move are all MiG-29 | this round, [`duels.md`](duels.md) D1 |
| **The trigger's prediction horizon was the ROUND's and is now the SQUEEZE's** (§5.8). `pred = err + rate·(latency + GunTofS)` extrapolated a ONE-TICK derivative a full second, and the time of flight had no business in it: the lead solution at the muzzle already carries the target's motion, so a round that has left cannot benefit from the aim improving behind it. [MESS, `xmergesplit`] **11 of 13** squeezes were commanded with the aim OUTSIDE the funnel gate, the first **14.6×** outside it. With `latency + burstS/2`: rounds landed per drum **6.37 → 9.53** on the same 150 fired, and over the whole 120-run merge arena **139.8 → 449.1 rounds on target off 6,318 → 5,850 fired (2.21 % → 7.68 %)**. `gun-turning` keeps its kill at t = 64.4 on **279 → 209** rounds; `gun-bfm` keeps its kill (t = 106.4 → 106.5) | built | this round |
| **`Phase::Bfm` can employ the round on the rail** (§5.11) — five instrument gates, one module hook (`BfmWvrCueDeg`, the MiG's Shchel-3UM 60° against the F-16's default "the round's own gimbal"), no new arithmetic and nothing held after the launch. `duel-merge` **exit 3 → 0**: the viper's AIM-9 arrives 1.93 m out, 218,781 J/m² → flight controls fail → `damage KILL` at t = 10.5 s. `missions/duel-merge-stern.fbm` is the mirror proof — the MiG's R-73 arrives 1.86 m out at 590 m/s of closure and kills the F-16 at t = 21.3 s | built | this round, [`duels.md`](duels.md) D6 |
| **A fuel state became a decision, and the branch that was blocking it was blocking the defence hold too** (§7.4a). Two changes in one chain: `Defend` now owns the state while its 12 s hold runs (before, the general branch took it away one tick after the threat symbol went out, so the hold never elapsed and `CanPressOn` behind it was unreachable code), and BINGO is asked at the head of the general chain — below survival and below an unfinished missile support, above everything that is a statement about the picture. **[MESS, `bingo-abort` vs `bingo-press`, one declaration apart]** `eng_state` search → **abort at t = 4.1 s** against search → closing → attack → support → kill at t = 172.8 s; heading 90° → 263.5°, the jet ends 65 km west of its spawn. **[MESS, `w3-06-bingo`, the file the finding was made on]** both escorts abort at t = 4.1 s and the four missile launches of the old run are gone. Blast radius over 238 stock missions: **42 move, 2 change their verdict**, and the whole list with per-mission attribution is §7.4b | 2026-07-30 |
| **What he is looking at is now visible, and it is a command** (§7.6a). `SelectCockpitPage` runs once per decision tick under the same "cockpit work only in flight" gate as the briefed entries, on its OWN spacing timer — a page button at the screen's edge may not take the hand away from a chaff throw, a designation or a shot, and that separation is what the regression measures. **[MESS, `mig29-intercept`]** the MiG spawns `n019_emission off`, so there is no FCR page and he takes RWR at t = 0.0; the emission acks at **t = 27.9**, the FCR page comes into existence and he posts `mfd_page 0` in that same tick (ack t = 28.4). **[MESS, `payerne-full`]** three selects in 734 s: SYS at 0.0, HSD at 16.0 (the Nav block came up), SYS at 663.2 (ALOW went active on the approach) | this round, [`modules/f16/cockpit-displays.md`](modules/f16/cockpit-displays.md) |
| **EMCON became reversible: the FCR knob is asked of the SET, not of the PICTURE** (§7.6b). `InterceptCockpit` guarded both `RadarMode` posts with `Radar.H.Readable()`, which `FBRadarSystem` clears the moment nothing radiates — so the one branch that could end a silent spell was disabled by the silence. `FBRadarBlock` gains the `Powered` readback its siblings already carry (+ `SetAbsent()` for the module's no-set branch), and the two posts plus `BfmSelectRadarMode` read it. **[MESS, `sat-02-picture-split`]** `EmconSilent_` was true for **10 of 5,200 ticks** while the radar was off for **4,626** — the gate was never the latch. Before → after: `fcr_on` **11.0 % → 87.3 %**, `fcr_lock` **0 → 18**, blue `sms LAUNCH_SOLUTION` **0 of 18 → 4 of 12**, `eng_shots` **0 → 1** per sweep member, named `kill unit` bits **0 of 8 → 2 of 8**, and the jet now flies real EMCON spells (1.0 s and 30.6 s) instead of one-way silence | 2026-08-03, [`doctrine-evolution.md`](doctrine-evolution.md) `X-6` |
| **Every angle this pilot hands to an antenna carries its FRAME in its TYPE** (`core/FBBodyAngle`). The elevation the intercept law commands is built by `FromWorldElevation(worldEl, st.pitch)` where the search band is a world altitude and by `Measured(...)` where it is a contact's own return angle, and `FBCommandBus::PostAntennaAz/El` accept nothing else. It closed §Gaps 2.15 and its two siblings at once, and `verify-layers` now counts the posters (**1**) the way it counts the registry readers | 2026-07-29 |


## Gaps

### Open work (from the retired `TODO.md` §2)

| # | Thing | Known from |
|---|---|---|
| 2.1 | **Arrival closure still ~85 kt at the band edge.** The throttle controls a speed *difference*, the schedule is written in range *rate*; the two diverge as soon as the pursuer trades altitude (74 kt TAS difference against 157 kt closure). Two candidates measured and rejected (below). Next: lag angle only inside the band, where the estimate has converged. | `658014d` |
| 2.2 | ~~Roll-rate limiter does not converge when the raw command oscillates in amplitude~~ — **CLOSED.** The recursion was an oscillator (|z| = √a), not a limiter; replaced by a memoryless one-step plant inversion off an identified plant, and the cap re-derived as a closed form and re-measured. §5.7. Remaining: the inversion still overshoots its cap by ~18 % at the 10 Hz decision rate (intersample build-up, not a loop mode), and its stability bound assumes a roll lag above ~0.13 s — a crisper airframe than the F-16's 0.32 s would need the two plant numbers to become airframe hooks (**done**, `71cb99f`: `BfmRollPlantA`/`BfmRollPlantKDegS`). The over-hold itself is **still open and now quantified against the regime that exposed it**: at full deflection the F-16 holds 103–109 °/s against a declared 90 (1.15 ×) because the ARX(1) identification was restricted to samples BELOW the cap, i.e. small stick, while the limiter only ever operates AT full stick. An open-loop step inside `duel-merge` (six ticks of a constant −1.0 command) fits K ≈ 110–113 °/s against the declared 78.7. Re-identifying at full deflection would move every BFM mission and is a round of its own; §5.7.3's extent bound removes the CONSEQUENCE (a sustained over-cap rate) without touching the identification. | `71cb99f`, this round |
| 2.3 | **The SYMMETRIC duel stays a stalemate** — every long shot is defeated in the notch, nothing ever reached fuze radius; re-measured this round, the F-16-only tournament produces **0 kills and 0 losses in 30 runs**. **ASYMMETRIC duels do decide**, and what decides them is the launch DOCTRINE rather than the geometry: [`duels.md`](duels.md) | `cac7b62`, [`duels.md`](duels.md) |
| 2.6 | **An AIM-120's terminal miss is a strong function of closure, and nothing says whether that is the round or the physics.** [MESS, `duel-headon` with the target's cruise swept] target 169/206/237/268/288 m/s ⇒ closure 744/842/919/1000/1053 m/s ⇒ miss **1.37/2.13/4.74/3.15/7.66 m**. Against a 1/r² damage model those six metres are the difference between a kill and a jet that flies on with wrecked avionics | [`duels.md`](duels.md) D2 |
| 2.7 | **The pilot does not use the IRST.** `sensors/FBIrstSystem` publishes an `Irst` block; the intercept picture is built from the Radar block alone, so a passive sensor cannot cue anything and "IRST + EMCON" can only be flown as "silent and blind" | [`duels.md`](duels.md) D3 |
| 2.8 | **The lift-vector law is singular for a DOWNWARD demand above 1 g** — the mechanism that TRIGGERS the merge roll (§5.7.3 fixes the consequence, not this). `L = a_turn·dir + g_body`: if `dir` points below the nose and `a_turn > g`, `liftUp` goes negative and the commanded bank jumps past ±90°; for a purely downward error it is ±180° decided by the sign of an arbitrarily small lateral component. [MESS, `duel-merge` t = 14.7 → 14.8] the gun-track handover replaced the Lag aim (el +5.4°) with the EEGS lead solution (el −8.0°) — a 13.4° step on a 9° total error — and the commanded bank went **−4.2° → −131.3°** in one tick, full deflection. A 10° aim correction at 250 m/s legitimately costs 2.2 g, so the g is not the error; the error is that the law reaches "down" only by rolling, when unloading to 0 g already buys 1 g of it. Any fix touches §5.1's core expression and every BFM mission. | this round |
| 2.9 | **HALVED (this round): the sink was the MiG's undamped short period; the re-acquisition is still open.** The MiG half is CLOSED by §5.10a — the damper was FLCS-only, the fight was flown without it, and the CFIT is gone: [MESS] `duel-merge` min AGL fulcrum **−4 → 4,548 m**, viper **449 → 4,487 m**, no monitor KO, exit 2 → 3. Nobody settles below the 2,000 ft floor any more, so the floor bias (a 30° elErr term, not a limit) has not been tested against a real sink since and stays unproven. **Still open, and now the whole of it:** the fight never re-acquires. [MESS, `duel-merge` after §5.10a] the F-16 spends **231.6 of 300.1 s** with `bfm_rng = −1`, the MiG 223.7 s, **220.1 s both at once** — worse as a FRACTION than before (77 % against 82 %, 75 % against 62 %), because the two jets now climb apart instead of spiralling down together. An ACM box of ±15°/±10° does not find a co-speed opponent in a turning fight. | this round |
| 2.12 | **The merge is DECIDED, and what still has no path is the INTERCEPT into it.** Unchanged: `FBPilot` has exactly ONE transition into `Phase::Bfm` and it is the briefed `set task bfm` at spawn — `InterceptCommands` aborts at `InterceptAbortRangeNm` (5 nm) and never transitions, and the BFM phase writes no `eng_*` column, so the tournament reads level C as `GATE` on both sides. **Closed this round:** the phase now employs the IR round (§5.11) and `duel-merge` goes exit 3 → 0. [MESS, the 120-run merge arena] outcome classes **60/60 (2,1) on all three cells → `xmerge` 30 (3,2) + 30 (1,0), every run decided**; `merge` and `xmergesplit` stay 60/60 (2,1). **Still open beside the transition:** the symmetric F-16 merge is a mutual WVR exchange that kills nobody (both AIM-9s arrive 2.7–3.4 m out against a 2.32 m threshold — §2.3's stalemate, now WVR instead of BVR), and `xmergesplit` never decides because the F-16 entering 2,000 m high never acquires at all (§2.9). | [`doctrine-evolution.md`](doctrine-evolution.md) E-14, this round |
| 2.10 | **A separated wingman rejoins badly.** The station law has no rejoin behaviour — a member pulled out by a defensive turn tail-chases up the lead's course line and cannot close: [MESS, `four-4v4-asym`] 20.4 km median station error for 1,894 ticks. Full list of formation gaps: [`formation.md`](formation.md) | this round |
| 2.11 | **The attack phase cannot release on a BEARING**, so no suppression shot is possible. Specified as `C27` in [`air-to-ground.md`](air-to-ground.md) §7 and deliberately bounded to **one gate**: `set attack_mode arm` beside `ccip`/`ccrp`, the RWR block read like any other instrument (readable ∧ a threat of the declared class ∧ its bearing inside the seeker's cone at the predicted moment of release), the release lead unchanged, and the shot MOMENT still briefed by `set brief_release_s` — so the weapon is measurable before this phase changes at all. What is expressly **not** in that round, and is therefore this file's: turning to put a threat inside the cone, any defensive reaction to a launch (`modules/ground/module.md` G11), weapon selection between an ARM and a bomb, and re-attack | [`air-to-ground.md`](air-to-ground.md) §7 |
| 2.4 | **Gun: the fire discipline is fixed, the AIM at range is not — and the split is now measured.** §5.8's horizon defect is closed (State above). What remains is arithmetic and it is stated once: a `damage KILL` by 30 mm needs **17.0 landed rounds** in one zone [HERL] — `kFlcsFail` 1.5·10⁵ J/m² against 0.5·0.39·795²/14.0 m² = 8,803 J/m² per round — and the merge delivers **9.53 of 150**. The two halves, both measured on `xmergesplit`: (a) **effect is not the limiter** — at the σ the fight is actually fought at (3.78 m, 630 m of round path) a PERFECTLY aimed drum lands 20.2 rounds and kills, so the ceiling is above the threshold; (b) **aim is** — the drum kills at a mean miss ≤ **2.38 m** and the measured median over all 150 rounds is **6.41 m** (was 8.72). And `missM ≈ RangeM·tan(GunAimErrorDeg)` to within 10 % on the far bundles, so the aim error at the muzzle IS the miss: the remaining work is §5.6's tracking loop at 400–770 m, not the damage ladder. The old reading ("~1 in 8 approaches misses") stands unchanged for the F-16 sweep. | `658014d`, this round |
| 2.13 | **A WVR launch has no load-factor gate, and that is a missing MECHANISM rather than a missing number** (§5.11). A round leaves with the launcher's attitude and velocity (`FBStoreRelease`, `HaveRail` false for every air launch); the release path models no rail load, no separation transient and no seeker-cage disturbance, so a g limit would be a constant with nothing behind it. What would make one derivable is a separation model — until then the phase shoots at any g the airframe is holding. | this round |
| 2.14 | **A GROUND START PLUS A COMBAT TASK DESTROYS THE AIRCRAFT — so the alert scramble is not expressible.** `set task <bfm\|intercept\|attack>` sets `CurPhase` AT SPAWN (both modules' `ApplySetup`), and the phase machine has **no transition from `Route` into `Intercept`** — the only way in is the briefed task, exactly as §2.12 says of `Bfm`. A `spawn ground` unit with a combat task therefore never runs `Preflight`/`Takeoff`/`Climb` at all. [MESS, campaign O5's arena] with a `runway` line the MiG-29 steers off the strip toward its first waypoint and the monitor calls *"touchdown off the assigned runway"* at **t = 11.1 s** (59.3 m of lateral error 357 m down the roll); without one it rolls, lifts and cartwheels — `monitor KO … reason=ATTITUDE_CONTACT` at **t = 35.4 s**. The cost is named where it lands: O5's spec calls the scramble delay *"the campaign's most important single parameter"* and it had to be flown as a spawn DISPLACEMENT instead | [`campaigns/o5-airfield-defence.md`](../mods/f16/mods/f16/doc/campaigns/o5-airfield-defence.md) §State |
| 2.15 | ~~**THE GCI SCAN-ELEVATION ENTRY IS A WORLD-FRAME ANGLE POSTED INTO A BODY-FRAME ANTENNA COMMAND.**~~ — **CLOSED 2026-07-29, and it was the third of three of exactly this kind, so it was closed STRUCTURALLY rather than one call site at a time.** The defect was `st.pitch` exactly, as O2 measured: `FBMig29Pilot` posted `atan2(g.AltM − ownAlt, g.RangeM)` — a WORLD angle — into `FBCommandTarget::RadarSlewEl`, which is BODY-referenced, while this file's own uncued search law had always posted the identical geometry **minus `st.pitch`**. It now goes through `core/FBBodyAngle::FromWorldElevation(worldEl, st.pitch)`, and the antenna entries are reachable only through `FBCommandBus::PostAntennaAz/El`, which take that type and nothing else; `make -C sim verify-layers` prints **1 antenna-cue poster(s)** and fails on a second. **[MESS, `o5-02-scramble`, the case the entry was written from]** the pair climbs at pitch **+5.646…+6.013°**, the raid sits at **−3.41…−3.74° in the body frame**, the N019's RAD bar is ±6.0°: the antenna was commanded to **+2.891°** (bar edge −3.109°, the raid 0.3–0.6° outside it) and is now commanded to **−2.754…−4.339°**. **Zero radar contacts in the whole run before, first contact at t = 48.0 s at 25.70 nm after**, both R-27R shots away by t = 106.3, and the mission's exit code moves **3 (TIMEOUT) → 0 (SUCCESS)** on an unchanged mission file. O2's runs, which the same defect did NOT delete (0.11–0.64° of margin), keep their first track at t = 30.9 and change only in the entered number (**−0.025° → −1.868°** at `ownPitchDeg` 1.843). The two SIBLINGS found with it: the same world elevation went into `modules/air`'s live net cue (**+0.025° → −3.358°** on `air-awacs-cue`, a climbing MiG-25) and into its briefed one; and the SPAWN TICK published a zero attitude to every body transform in the tree ([`sensors.md`](sensors.md) §Frames). | [`campaigns/o5-airfield-defence.md`](../mods/f16/mods/f16/doc/campaigns/o5-airfield-defence.md) §State |
| 2.16 | **THE THREE EMPLOYMENT GATES ARE OPEN FOR THE CATALOGUE AIRCRAFT SINCE 2026-07-29, AND WHAT REMAINS SHUT IS NAMED.** All three of this file's weapon gates read `FBState::FireControl` — `InterceptCommands`' `inParams` (`fc.DlzValid && fc.InZone`), `BfmMissileShot` (the same plus a radar lock) and `BfmGunfire` (`fc.GunTolDeg`, `fc.GunInFunnel`) — and until that date NOTHING in `modules/air/` wrote that block, so every one of the eighteen catalogue rows was a flying statue ([`modules/air/module.md`](modules/air/module.md) A13). `modules/air/FBAirFireControl` now writes it for the ten armed rows and **the gates in this file did not move by one line**. Two consequences land HERE and are this file's own: (a) `BfmMissileShot`'s `state.Radar.LockIndex >= 0` requirement means a row with **no radar at all** can never launch even a fire-and-forget IR round, which is four catalogue rows (A14); (b) with a measured roll plant a `mig21` fires its cannon through §5.8's discipline and hits (four `gun HIT`, `damage SYSTEM structure degraded`) and then **cannot re-attack**: it takes 76 s to close 3.0 → 0.9 nm on a 240 kt target, overshoots, and descends into the ground from 5 000 m with `BfmFloorFt` failing to hold it (`CRASH` at t = 147.9 s, impact 424 m). That is §5's law on a GENERATED deck rather than on a measured one, it is booked as A15, and it is the reason no campaign may score a catalogue gun engagement | [`modules/air/module.md`](modules/air/module.md) §Spec 12 |
| 2.17 | **A fuel-driven RTB does not exist — the minimum-fuel decision is a break-off inside the engagement machine and nothing more** (§7.4a). `Abort` turns the jet cold; it does not send it home. A jet in `Phase::Route` at BINGO flies its briefed route to the end regardless, so W2's eight missions without air opposition — the campaign whose whole subject is fuel — have no fuel decision at all. The mechanism is small and the reason it is not built is not: the pilot receives the flight plan `const`, so an RTB is a `SteerpointNum` command that would have to move `FBFlightPlan`'s active index, and `core/FBMissionMonitor` reads that same plan for `objective waypoints`. A pilot who skips to the landing fix therefore FAILS a mission that never declared he might, which makes this a change to the VERDICT and not only to the pilot — and it needs an `.fbm` way to say "RTB is an acceptable ending" before it may be built. | this round |
| 2.18 | **Nobody drops an empty tank on their own** — the jettison moment is briefed, not decided ([`modules/stores/module.md`](modules/stores/module.md) §Gaps). Measured cost of not deciding it: **184.7 km of range, 4.8 %** (`tank-jettison.fbm` against `tank-radius-tanks.fbm`), and 7.1 % of the fuel per kilometre for as long as the empty tanks hang there. The instrument exists (the SMS knows its plumbed tank's contents); the decision does not. | this round |
| 2.19 | **THE SAME CONFLATION IS STILL OPEN ON THE DISPLAY SIDE, and it is left open deliberately.** §7.6b removed "picture head = box present" from the one place it was a LATCH; two display readers still make the substitution. `systems/FBMfdSystem::PageAvailable` returns `s.Radar.H.Readable()` for `FBMfdPage::Fcr`, so a jet in EMCON loses the FCR page from its bank entirely and `SelectCockpitPage` falls through to RWR — a real FCR page exists with the set in OFF and shows OFF. `systems/FBDisplaySystem` writes `NoData("FCR")` on the same test. Both are now expressible against `Radar.Powered` ([`core.md`](core.md) §1.1a) and neither is fixed here, because "what does a scope show for a set that is on but quiet" is a DISPLAY decision whose acceptance is a rendered frame, not a telemetry column. Cost of leaving it: an EMCON pilot's attention rank silently skips its own rank-3 first choice. | 2026-08-03, §7.6b |
| 2.5 | AoA band 11–13° instead of a flat 11° on approach (ED-documented, `doc/modules/f16/procedures-landing.md`); porpoise after touchdown; `ApproachSpeed` should be weight-scheduled instead of fixed. | measurement |

### Rejected approaches (do not retry without a new argument)

| Approach | Why rejected |
|---|---|
| Geometric lag **angle** from the range-rate equation, acting globally | mode selection degenerates into a relay, the lift vector flutters; with lag in the vertical the jet zooms 940 m and comes back as a split-S. Best variant: 0 of 8 kills. |
| Throttle controls measured range rate instead of speed difference | band 21.4 → 23.2 %, but funnel time against the straight defender 21.2 → 12.7 s. |
| Conversion mirroring the long way round | unbounded roll, departure at t=39 in `bfm-blind`, 2 of 16 approaches. |
| Wingline commit without a LOS-rate gate | costs 2 of 11 kills. |
| Gun integrator with ζ = 0.5 | better against the turner; against the straight flyer the loop rings and funnel time collapses. |
| **Integral action on the BFM throttle** (the textbook fix for the P-only loop's standing error around a fixed trim of 0.6) | it works, and it costs more than it buys. [MESS, 16-approach sweep, Ki = 2·10⁻⁴…2·10⁻³ δ/(kt·s)] against the STRAIGHT defender 0/8 → **8/8** kills, because the pursuer stops arriving with 100+ kt and stops creeping through the funnel's minimum range; against the TURNING defender 8/8 → **0/8**, because exact speed matching leaves him at the defender's own 248 KCAS with nothing left to take the last 14° of angle with, and he empties all 510 rounds at 7–8 m of miss. The miss is time-of-flight driven and measurable: target manoeuvre during flight ≈ ½·V·ω·TOF² = 1.6 m at TOF 0.5 s against 4.8 m at 0.9 s (measured misses 1.6–4 m vs 7–8 m). The standing bias the loop leaves is therefore doing an ENERGY job by accident; before this can be corrected, the energy rule it stands in for has to exist. |
| Speed floor from the turn rate the pursuer has to match (`ω_max(V) ≥ ω_target`, closed form off `BfmCornerG`/`BfmCornerSpeedKt`) — the intended replacement for that bias | too permissive AND too aggressive at once. The defender in `bfm-basic`'s break turns at only **5.4 °/s** (measured off its own velocity vector), so the floor solves to 191 KCAS and never binds; adding the aim error over the pursuit time constant (`ω_target + err/T`) makes it bind and then it binds everywhere — sweep 0/8 straight, 0/8 turning, 0 rounds fired. |
| Lowering the shot's Rtr factor to buy a closer AIM-120 pass (`bvr-duel-decided`) | no trend: `pilot_shot_rtr` swept 1.0 → 0.5 gives miss distances 6.25 / 5.35 / 8.11 / 7.52 / 7.20 / 4.03 m. The endgame miss was not the shot's timing but the round's own terminal loop. |
| Roll limiter as a recursion on its own previous command (`cmd_prev·cap/rate`) | it is an oscillator, not a limiter: |z| = √a, and it held 1.37–1.52 × its own declared cap. Replaced by the plant inversion, §5.7.1. |
| Keeping the 60 °/s cap once the limiter actually holds it | the number was the nominal of a loop that rang a third past it; enforcing it exactly starves the roll and `gun-bfm` departs at t = 214.6 s. §5.7.2. |
| Reading `bfm-blind`'s reacquisition time as a regression metric | it is chaotic — 50.9 / 120.0 / 209.0 s across a six-value cap sweep with nothing monotone between. The reproducible claim of that mission is the SEQUENCE, not the interval. |
| Enforcing the roll-extent bound EXACTLY (`|win| ≥ 180 ⇒ cap 0`) instead of tapering the cap with the remaining budget | it is the literal constraint and it moves fewer missions (3 instead of 6), but it spends the whole budget at the peak, stops for ~0.3 s while the window drains and re-arms — the duty cycle keeps the judge's timer running. [MESS] sweep departures **3** against **0** for the taper, kills 5/16 against 7/16, and `gun-bfm` loses its kill (1 → 3). §5.7.3. |
| Reading a ±1–2 kill change in the 16-approach sweep as a result | measured chaos: perturbing `gun-bfm`'s spawn longitude in 0.8 m steps over ±3 m flips the outcome between KILL (t = 77.9…197.1 s) and no kill in 2 of 8 samples, one of them a LOC at t = 372.5 s in the committed geometry. Departure counts and pooled rate statistics are the parts of the sweep that carry information. (The same instrument, applied to `bfm-blind` and `mig29-bfm`, says the OPPOSITE about their `bfm_ctrl_s` loss under §5.7.3: 0.0 in all seven perturbations on the new law against 48.4/88.2 on the old — that one is causal, and it is declared as a cost rather than explained away.) |

### Documentation lag (from the retired `TODO.md` §7)

This file describes state `9673e00`. Commit `658014d` has already changed three of its points
(sign-correct trigger, derived brake cap `a/k` with the new hook `BfmBrakeMs2`, wingline conversion) —
affected: §5.2, §5.7, §5.8, §11, §12 of the body below. §5.2, §4.2 and §4.3 have since been brought up to
the D1 re-tune round; §5.6–5.8 have not.

### Inventory (from the previous `Offene Punkte` section)

**Known weaknesses (state `9673e00`, each one measured, none of them hidden):**

1. **Gun against the turning defender: ~1 in 8 approaches still misses.** The pursuer settles inside the
   MINIMUM RANGE of the funnel — there the EEGS solution no longer delivers a usable firing cue, while
   the pursuit geometry keeps the jet there. Missing: a rule that turns "too close" back into distance
   (the control position describes a band centre, not a lower bound with an avoidance behaviour).
2. **The BVR duel stays a stalemate, because every long shot is NOTCHED.** Both sides defend successfully
   with beam + chaff (`bvr-duel.fbm`: first shot defeated, the second at t=527 s with a 43.6 m miss
   distance likewise). As long as neither side brings a shot from a geometry that excludes the notch (or
   a weapon that survives it), the pairing is symmetric and the result structurally a draw.
   `bvr-duel-decided.fbm` shows the control: an energy difference decides, craft alone does not.
3. **The tight head-on pass is not CONVERTED into a turn.** The law controls a BEARING and always takes
   the short way; exactly at the tail passage both ways are equally long, the commanded lift vector sign
   flips, and the control loop sees not a worsening error but a NEW error on the other side — it answers
   with a reversal. [MESS] at the 385 m merge of `bfm-blind`: the line of sight moved from +102° to +177°
   and wrapped to −177°, the roll command clattered across the vertical (94/109/97/100/107/93/105° of
   bank in successive half seconds) instead of holding a turn. A pass is therefore not converted into a
   position.
4. **The closure schedule has a FLAT cap at `9673e00`** (`BfmMaxClosureKt` = 200 kt) that does not hang
   on the brake authority of the airframe. The schedule `c = k·(R − Rctrl)` demands a deceleration `k·c`;
   beyond `a/k` the pilot writes a cheque the airframe cannot cover. [MESS, `gun-bfm`] acceleration to
   190 kt of closure at 2 nm, then 35 s of idle + speedbrake without getting rid of it, arrival in the
   control band at 98 kt instead of the scheduled 5, fly-through at 61 m.
5. **The throttle controls a speed DIFFERENCE, not the closure rate.** The two are the same number only
   in a co-altitude tail chase. [MESS, `gun-bfm` third approach] 74 kt of TAS difference against 157 kt of
   actual closure, because the pursuer was 700 m higher and converted altitude into closure. The
   alternative (controlling on the radar measurement of closure) has been measured and rejected: control
   band marginally better (21.4 % → 23.2 % of the tracked time), funnel time against the straight-flying
   defender collapsed (21.2 → 12.7 s) — the closure rate carries the whole pursuit geometry, controlling
   it with the throttle lets the throttle fight the turn.
6. **No terrain masking in the sensor chain** (deliberate, `sensors/FBRadarSystem`): the pilot therefore
   cannot learn any terrain tactics, neither offensive nor defensive.
7. **Flares have no effect** (no IR seeker in the tree) — the pilot's defence today is purely chaff +
   beam.

**Update due — concurrent change.**
At the time of this file another agent is changing `sim/src/pilot/FBPilot.{h,cpp}` and
`sim/src/modules/f16/FBF16Pilot.h` (close-combat round). Already visible in the working copy and NOT YET
documented here:

- a new airframe hook `BfmBrakeMs2()` (F-16 2.4 m/s², [MESS] 238 samples at 4,000 m between 325 and
  400 KCAS, median 2.39 / p10 1.64 / p90 3.80; re-measured across `MODEL-DELTAS.md` D1 as 2.531 →
  2.527 m/s² — unmoved, because the trailing edge flaps only deploy below 250 KCAS and this band never
  sees them) and a closure cap **derived** from it, `a/k`, instead of
  the flat 200 kt cap → affects point 4 above;
- a **conversion rule** with a committed turn direction (`BfmTurnSense_`, `kBfmConvertErrDeg` = 90°, zone
  width from `180° − LOS rate · kBfmReverseS`) → affects point 3 above;
- the trigger reads the predicted target solution as an **absolute value** instead of clamping it at 0.

After this round is finished, § 5.2 (schedule), § 5.7/§ 5.8 (roll/trigger), § 11 (hook table) and § 12
(points 3–5) have to be brought up to the then-current commit. `doc/missions/combat.md` already carries
parts of this round — where the two files disagree, the SOURCE CODE of the respective quoted commit
applies.


## Knowledge

Derivations, formulas and measured constants — the distilled body of this file.

### 1. The layering

Three levels, three questions, three rates. No level reaches past the next.

| Level | Class | Question | Rate | Output |
|---|---|---|---|---|
| Mission | `pilot/FBPilot` | WHERE should the aircraft go | 10 Hz | `FBPilotCommands` |
| Guidance | `systems/FBAutopilot` | WHICH manoeuvre gets it there | 100 Hz (in the FDM substep) | `FBGuidance` (bank, VS, speed) |
| Hands | `systems/FBFlightControl` | WHICH control deflection | 100 Hz | `FBControls` → `FBFdm::SetControls` |

The rate arises in the module, not in the pilot: `FBF16Module::Run` calls `Due(PilotAccS, dt, 10.0)` and
inside it `PilotSys->Run(...)`; the 100 Hz substeps run below it in the same `Run()`
(`AP->Run(st)` → `FC->Run(...)` → `fdm.Step(st)`, spiral protection ≤ 12 substeps/frame).
A pilot decision therefore stands for ~10 FDM steps. Immediately after the pilot tick the module
publishes the fused BFM view onto the bus (`SharedState.Bfm = PilotSys->BfmTrack().Block()`) and calls
`NavSys->AdvanceWaypoint(...)` — waypoint sequencing is actor behaviour, not runner bookkeeping.

#### `FBPilotCommands` — the output of one decision tick

```
FBPilotGuidance Guidance;                        // None | Manual | Direct | Course
double TargetAltM, TargetSpeedKt;
double TargetLatDeg, TargetLonDeg;               // Direct target point / Course reference point
bool   HaveLeg; double LegLatDeg, LegLonDeg;     // the path origin, when a path EXISTS
double CourseDeg, GlidepathDeg;                  // Course only (TargetAltM = threshold elevation)
double ManualRoll, ManualPitch, ManualYaw, ManualThr;
std::optional<bool>   GearDown;
std::optional<double> Speedbrake, WheelBrakeLeft, WheelBrakeRight, NosewheelSteer;
std::optional<bool>   EngineStart;
```

Two contracts sit in the types:

- **`FBPilotGuidance::None` = "leave the AP untouched".** The module calls the matching `FBAutopilot`
  setter (`SetManual`/`SetDirect`/`SetDirectLeg`/`SetCourse`) ONLY on a concrete mode; a `None` tick
  changes nothing about the running guidance. A neutral `FBPilotCommands` (phase `Idle`) calls nothing at
  all in `FBF16Module::ApplyPilotCommands`.
- **Every airframe request is `std::optional`.** Not set = "the pilot is not touching this lever right
  now" — the image of a real pilot's hands, and the reason why most ticks carry no airframe request at
  all.
- **`HaveLeg` is NOT an optional**, although it could be one: it is not a control the pilot may or may
  not operate but what he KNOWS. No leg = no line = the bearing law. `SetLegFromPlan` sets it only from
  the SECOND waypoint of a plan on (`idx > 0`) — two declared fixes are a path, a first waypoint is a
  bearing. That is load-bearing: if one invented a leg from the spawn position, a defender that is
  supposed to orbit a point INSIDE its own turn circle would fly to it once and leave it
  (`missions/bfm-basic.fbm`).

---

### 2. The rule that carries everything

#### 2.1 The pilot holds NO system pointers

The whole signature is the contract:

```
virtual FBPilotCommands Run(const FBState &state, FBCommandBus &avionics,
                            const FBAirframeControls &airframe, const fb_fdm_state &st,
                            const FBFlightPlan &plan, const FBRunway *runway, double dt);
```

- `state` — the avionics bus. What sensors have WRITTEN into it, with range, scan volume, net cycle, age
  and validity header. Never world truth.
- `avionics` — the command bus. The ONLY way from this class to an avionics box.
- `airframe` — the airframe, borrowed CONST: instrument readings (WOW, gear position, take-off weight,
  engine running) through the same interface through which the commands go back out. The pilot reaches no
  FDM and does not know which one he is flying — that keeps this layer airframe- AND instance-agnostic.
- A `const FBWorld *` used to STAND in this signature (unused, `(void)world`). It has been removed: a
  pilot to whom world truth cannot even be handed cannot accidentally fly on it.

#### 2.2 Avionics only through the command bus

Every operation is an `avionics.Post(target, value, nowS)` with an acknowledgement. That costs time and
can be rejected (`core/FBCommandBus.h`):

| Class | Latency | Examples |
|---|---|---|
| HOTAS | `kHotasLatencyS` = 0.5 s | `MasterArm`, `Designate`, `WeaponRelease`, `CmDispense`, `RadarMode`, `RadarSlewEl` |
| DED (head down) | `kDedLatencyS` = 4.0 s | `AlowFt`, `BingoLbs`, `CmdsMode` |
| Trigger (special case) | `kTriggerLatencyS` = 0.1 s | `GunTrigger` — the value IS the duration of the squeeze; the SPACING of two squeezes stays 0.5 s |

In addition the bus locks head-down inputs while the jet is manoeuvring (load factor from the AirData
block; without a valid block it reads 1 g).

#### 2.3 The brief — and without a brief he operates nothing

A `set` line configures the aircraft in the spawn window, BEFORE the first pilot tick. A `brief_*` line
configures the PILOT: it is a value that he ENTERS in flight over the bus.

| Brief | Channel | Mission line |
|---|---|---|
| `BriefAlowFt` | DED | `set brief_alow_ft <ft>` |
| `BriefBingoLbs` | DED | `set brief_bingo_lbs <lb>` |
| `BriefMasterArm` | HOTAS | `set brief_master_arm arm\|sim` |
| `BriefWeapon` | HOTAS | `set brief_weapon gun\|aim9\|aim120` |
| `BriefRelease(atS)` | HOTAS | `set brief_release_s <t>` (repeatable, max. `kMaxBriefedReleases` = 8) |
| `BriefChaff(atS)` | HOTAS | `set brief_chaff_s <t>` (repeatable, max. `kMaxBriefedDispenses` = 8) |
| `BriefAttack(mode)` | — (no box) | `set attack_mode ccip\|ccrp` |

Sequence (`EnterBriefedItems`, `ReleaseBriefedStores`, `DispenseBriefedCm`):

- **One input per decision tick**, in the fixed order ALOW → BINGO → master arm → weapon selection. The
  stream is thereby deterministic, and the pilot works one lever after the other.
- **Retry only on a bus rejection** (channel busy, manoeuvre lock), and then at the earliest after
  `kBriefRetryS` = 2.0 s [SETZ]. Justification in the header: a rejected DED input is a hand off the
  stick and a head down; repeating it at 10 Hz would be a keyboard macro and would clog the command
  stream. What REACHED the responsible box is final — a pilot to whom the jet has said "no" does not
  retype the same number forever.
- **Releases and countermeasures are NEVER repeated.** A rejected release is a decision of the jet.
- **Cockpit work only in flight**: `CurPhase != Idle && !GetWeightOnWheels()`. On the ground these inputs
  are on the checklist before engine start, hence outside the phases of this class.

#### 2.4 He sees only through sensors

Everything the pilot knows about other units comes from `FBState` blocks: radar (ANONYMOUS contacts +
lock index), RWR (bearings and emission class, NO range), FireControl (launch envelope/release solution),
stores, warnings, cmds, datalink. The header of every block (`Invalid`/`Valid`/`Held`) is asked FIRST —
`Readable()` resp. `IsValid()` stand in every decision path before the first access to a number.

---

### 3. The phase state machine

`FBPilot::Phase` (the order in the enum is telemetry-visible, only APPEND):
`Idle, Preflight, Takeoff, Climb, Route, Approach, Flare, Rollout, Shutdown, Bfm, Intercept, Attack`.

| Phase | Guidance | What it does | End |
|---|---|---|---|
| `Idle` | `None` | nothing — a neutral command, the AP stays untouched | only through `SetPhase` from outside |
| `Preflight` | `Manual` | gear down, both wheel brakes 1.0, idle, wings level | engine running AND `PhaseElapsedS ≥ kPreflightHoldS` (2.0 s) → `Takeoff`; without ground contact it stays neutral |
| `Takeoff` | `Manual` | brakes off, thrust `TakeoffThrottleNorm()`, nosewheel steering onto the runway axis, stick neutral until `Vr − RotationLeadKt`, then pitch PD onto `RotationPitchDeg` | WOW == 0 → `Climb` |
| `Climb` | `Direct` (+ leg from WP 2) | to the active waypoint at ITS altitude, speed = `ClimbSpeedKt`; gear up on a positive rate + AGL margin + below `GearUpLimitKt` | `GetGearPosition() ≤ 0.02` → `Route`; no waypoint → `Shutdown` |
| `Route` | `Direct` (+ leg from WP 2) | to the active waypoint at ITS altitude AND speed | waypoint of type `Land` → `Approach`; no waypoint left → `Shutdown` |
| `Approach` | `Course` | extended runway centreline + `GlidepathAngleDeg` descent to the threshold, speed = `ApproachSpeedKt`, speedbrake `ApproachSpeedbrakeNorm`, gear down | radar altitude ≤ `FlareStartAglFt` → `Flare`; WOW before that → `Rollout`; no runway → `Shutdown` |
| `Flare` | `Manual` | thrust idle, pitch PD onto `FlareTargetPitchDeg` with damped authority (`kFlareStickMax` 0.6) | WOW → `Rollout` |
| `Rollout` | `Manual` | speedbrake full, two-point aerobrake onto `AerobrakePitchDeg` until `AerobrakeSpeedKt`, then proportional lowering of the nose; wheel brakes `RolloutBrakeNorm` from the moment `FBAirframeControls::GetNoseWheelOnGround()` is true (latched); nosewheel steering as in take-off | none — `core/FBMissionMonitor` judges "standing on the runway" |

**Why the brakes hang on the NOSEWHEEL and not on `AerobrakeSpeedKt`.** In the two-point attitude the
wings still carry the aircraft — measured on the roll-out, wheel normal load 0 lbf while 12° is held —
so a wheel brake has no normal force to work against and the aerobrake is the whole budget (2.4 m/s² of
drag). The moment the nose falls, drag collapses (1,477 lbf instead of 5,295) and the mains take load;
that is the instant braking becomes possible, and `doc/modules/f16/procedures-landing.md` sequences it exactly
so ("reduce back stick, lower nosewheel … apply moderate-heavy wheel braking"). The ~100 kt in the
procedure is the EXPECTATION of when the nose comes down, not an independent gate — the elevator
actually loses the attitude at ~106 KCAS, and the old speed gate therefore left a **361 m / 6.7 s
coasting segment at 0.45 m/s²** between the two. `GetNoseWheelOnGround` is the forwardmost bogey's WOW,
selected by geometry rather than by a gear name, so it stays airframe-agnostic.
| `Shutdown` | `None` | nothing | — |
| `Bfm` | `Manual` (own law) | § 5 | none: a combat phase, entered by mission declaration |
| `Intercept` | `Direct` | § 7 | none (except internally `Abort`) |
| `Attack` | `Direct` | § 4 | back to `Route` after the egress |

Shared building blocks:

- **`PitchHoldStick(targetDeg, pitchDeg, qDegS, stickMax)`** — a PD (`kRotateKp` 0.15, `kRotateKd` 0.02)
  onto a target pitch attitude. Rotation, flare and aerobrake/derotate are THE SAME law with a different
  target and a different authority limit (`kRotateStickMax` 1.0 resp. `kFlareStickMax` 0.6).
- **`NosewheelSteerCmd`** — `−(0.01·cross-track offset[m] + 0.02·heading error[°])`, capped at ±0.6. Kept
  small, because the model-owned `steer-cmd-norm`→degrees characteristic is itself very steep at taxi
  speed (~80 °/unit at ~6 kt, `f16.xml`). Axis convention = that of `FBMissionMonitor::OnRunway` and
  `FBAutopilot::SetCourse` (along = 0 at the threshold, + in the runway direction; + across = right) —
  take-off, rollout and the mission verdict thereby agree about what "on the line" means.
- **Gear-up clearance:** `st.vy > kPositiveRateMs` (0.5 m/s), radar altitude **readable** and
  > `kGearUpAglFt` (10 ft), CAS < `GearUpLimitKt`. The small AGL margin is a JSBSim property: `FGLGear`
  freezes WOW as soon as `gear-pos-norm` first falls ≤ 0.99 — a retraction in the middle of a touchdown
  would freeze a stale `WOW=true`. It must not be large, because in full afterburner the model would
  otherwise reach the 300 kt gear limit [DOK `procedures-takeoff-taxi.md`].
- **Every AGL gate asks the header of the radar altitude block first.** Without a valid altitude the
  pilot does NOT act: the gear stays down, the flare does not trigger, the BFM floor no longer pulls
  [DOK `controls-commands.md` §6.4 — the sensor blocks the effect, not the command].

---

### 4. Phase `Attack` — the only phase whose decision is a MOMENT

Three parts; the middle one lasts one tick.

#### 4.1 Run-in

`FBAutopilot::Direct` onto the active waypoint, at ITS declared altitude and speed — hence a **level
laydown**. Two reasons, both in the banner:

1. `Direct` HOLDS an altitude. This guidance flies a level, stable run-in exactly; a 20–30° dive would be
   the pilot in a permanent fight against his own altitude hold.
2. That way the RELEASE MOMENT stays the only variable of the experiment: the run-in is repeatable to the
   metre, every metre of miss distance belongs to the computation or to the moment, not to the flying.

The run-in is a **PATH, not a bearing**: the origin is anchored ONCE at the moment the run-in begins
(`AtkHaveRunIn_`, `AtkRunInLatDeg_/LonDeg_`) and handed to `SetDirectLeg` via `HaveLeg`. The mission
declares no initial point, so the point at which the run-in began IS the initial point. [MESS,
`doc/missions/combat.md`] cross-track error 31.6 m → 10.6 m on the 19 km CCRP run-in, the along-track
component unchanged.

#### 4.2 The ONE pickle

The gate conditions in the order in which a pilot checks them:

| # | Condition | Source |
|---|---|---|
| 1 | `fc.H.Readable() && fc.AgValid` | no computer, no release |
| 2 | `fc.AgArmMarginS > 0` | a release from here would arrive as a dud (PUAC) |
| 3 | `fc.AgInRange` seen at least ONCE (`AtkInRangeSeen_` latched) | a countdown counts THROUGH zero; without the latch one also fires on a solution that was never positive (no ground speed entered, target long behind the jet) |
| 4a | **CCRP**: `fc.AgTimeToReleaseS ≤ leadS − bias` | the cue |
| 4b | **CCIP**: the same AND `\|fc.AgCrossErrM\| ≤ AttackCcipTolM` | the judgement that a countdown cannot make |

Only the CROSS-track component goes into the CCIP condition. The along-track component is exactly what
the cue is about — it is deliberately non-zero at the moment of the press (the bomb still has to be
thrown). Checking the combined miss distance would reject every correct release and then accept a late
one.

**The lead by one's own actuation latency — by one's own decision tick — and by the rail's own queue.**
`leadS = LatencyS(WeaponRelease) + DecisionDtS_ + FBStoresSystem::kSeparationDelayS` = 0.5 + 0.1 + 0.1 s.
Pressing exactly on the cue would let the store leave the rail half a second too late: at 230 m/s that is
115 m — more than the whole computation is worth. **The real jet solves the same problem the other way
round:** in CCRP the pilot HOLDS the button and the AIRCRAFT releases when the cue runs through
[DOK `weapons.md` §2.5]. The intention is therefore expressed early and the moment belongs to the
computer. On a bus where a command is a discrete event, "lead by exactly the channel latency" is the very
same statement — and it is the pilot's knowledge of his own hands, not a look into something he may not
see.

The DECISION TICK is the second half of the same statement and was missing until this round: the pilot
reads the cue in one slot and the press reaches the bus in the next, so the delay between the NUMBER and
its EFFECT is latency + his own cadence. Alongside it the cue's own AGE is subtracted — the validity head
carries it, so it is read like any other instrument value. [MESS, `attack-ccrp`] without the tick term the
delivery was 63.7 m long against a fire-control prediction error of 42.8 m, i.e. **21 m came from the tick
alone** at 211 m/s; with it, `aimLongM` 40.9 m against `predErrM` 43.6 m.

**That round then read its own result against the wrong yardstick, and this one corrects it.** It
concluded "the release-moment error is ~0 and what is left IS the computer's own error" — from a figure
measured against `predLat`/`predLon`, i.e. against the COMPUTER. Measured instead against the store's own
back-solved ground crossing (`stores IMPACT`, `crossLat`/`crossLon`, which the round already logs), the
delivery was **+38.8 m LONG**, not ~0. The two errors had simply gone back to cancelling inside one
figure — the very trap the paragraph above names, one level up.

**The THIRD term, and its derivation.** The acknowledgement is not the separation: `FBStoresSystem::
Release` puts an `FBStoreRelease` in a queue and `missions/FBOrdnance` drains it in the phase AFTER the
actor step, so the store becomes a unit one sim tick later. [MESS] `attack-ccrp`: read t = 71.4,
`CMD_ACK` t = 72.0, `stores SEPARATION` t = **72.1** — the chain read → separation costs **0.7 s** while
the pilot held 0.6 s back. The gap is constant 0.1 s over `attack-ccrp` / `attack-ccip` /
`cbu87-footprint` / `w2-01-dome`, and it is one tick BY CONSTRUCTION, pinned by a `static_assert` in
`FBOrdnance.cpp` against `kSimTickS`. It is dead time exactly like the bus latency, one layer down; the
number lives on the queue's owner (`weapons/FBStoresSystem.h`) because `pilot/` may not see `missions/`.

**Predicted before the run, then measured.** One tick earlier at 231.5 m/s = **23.2 m** expected.
Measured on the ground crossing: **+38.8 m → +16.0 m long, a gain of 22.8 m** (1.5 % off the derivation,
the remainder being the speed change across the tick). The residual 16.0 m is 0.069 s — INSIDE one
decision tick, i.e. exactly [`doctrine-evolution.md`](doctrine-evolution.md) X-3's staircase floor of
10–22 m, which no lead can remove and only a finer release clock can.

**And it is a BIAS removed, not an accuracy gained — which is a different and weaker claim.** On
`w1-06-strike-escort` the same fix moves two deliveries from **+10.9 and +17.4 m long to −12.2 and
−5.7 m short**: both shifted by 23.1 m, one tick, and the first of them ended up marginally WORSE
(13.1 → 14.2 m total). That is X-3's lattice, not a regression: the release can only ever land on a 23 m
grid, and correcting a systematic offset moves every delivery one grid step regardless of where on the
step it was sitting. **The falsifiable claim is therefore about the DISTRIBUTION, stated before it was
measured: over every F-16 `stores DELIVERY` in the tree the mean `aimLongM` must fall by ≈ one tick ×
ground speed, the SPREAD must not change, and every MiG-29 delivery must not move at all** — the MiG
releases through a director, not a lead, and the attempt to carry this fix over to it was refuted and
reverted ([`air-to-ground.md`](air-to-ground.md) `C29`). A cell-by-cell "is it better" reading would be
cherry-picking either way.

**MEASURED, over the whole tree.** 284 missions flown on both builds, 259 `stores DELIVERY` lines paired
by (mission, ordinal); 9 excluded because their store came down kilometres from any aim point (a
jettison is not a delivery), leaving **250**.

| | old | new | delta |
|---|---:|---:|---:|
| mean `aimLongM` | +43.15 m | +23.50 m | **−19.66** |
| spread (sd) | 56.78 | 52.76 | −4.02 |
| mean `aimAcrossM` | +26.76 m | +26.66 m | −0.10 |
| mean `aimErrM` | +64.95 m | +50.61 m | −14.34 |

**The per-delivery shift has a median of −23.10 m** (range −17.83 … −25.51, which is the ground-speed
spread across the tree) against a derivation of 23.2 m. **The along-track bias moves, the spread does
not, and the cross-track does not move at all** — which is exactly the claim, and the cross-track number
is the one that could have falsified it: a lead term that touched anything but the release moment would
have shown there. 212 of 250 moved, 199 nearer the aim point and 13 further from it — the lattice, as
predicted.

**Three missions changed exit code, all 3 → 0, each explained individually** and none of them by luck:
`w2-01-dome` and `w2-08-flak` 36.38 → 13.29 m with objectives met 1 → 2; `w3-03-weasel-close` 23.59/21.48
→ 15.06/16.50 m with 7 → 8. **This is stated as a CONSEQUENCE and never as evidence** — the tree's rule
is that a better mission result is not a source for a model number, and it is not one here either. The
number came from the phase order before any mission was flown.

**One thing this round does NOT explain: the residual.** The mean is still **+23.50 m long** after the
fix — one further tick. It is not booked as a defect because it is a mean over a mixed population (the
MiG-29's deliveries are systematically SHORT, `C29`) and separating the two is its own measurement. It is
recorded here so the next round starts from the number rather than from the impression that this one
finished the job.

**Why it is compensated and not made structurally impossible — the alternative, and its price.** The
displacement is geometric, not temporal: `FBOrdnance::Launch` spawns via `FBMissionSpawnStore(…,
carrier.State(), …)`, i.e. the carrier's LIVE pose one tick after the release was serviced. **The tree
already contains the immune pattern one file over**: `FBGunBurst` carries its own `LatDeg`/`LonDeg`/
`AltM`/`VelE…`/`SimTimeS`, captured at the trigger, so the gun's identical queue costs it nothing and its
lead needs no such term. Giving `FBStoreRelease` the same fields would make this defect impossible rather
than compensated — and it was REJECTED, because the two things it would have to break are both deliberate:
`FBStoresSystem` holds no FDM at all (`SetOwnPose` is handed lat/lon/yaw by the module precisely "damit
diese Klasse keinen FDM anfasst") and would have to be handed a full state snapshot; and `Launch` sits
after the pose barrier on purpose, so that "a round is never resolved in the tick it left the rail". Two
deliberate choices produce the tick between them; the lead is the repair that keeps both. **And the scope
of the defect is bounded by the same reading: every store leaves through this queue, missiles included,
but a GUIDED round flies its own error out — the displacement is a pure, uncorrectable bias only for a
round that cannot steer.** That is why it shows up as a bomb-delivery bias and nowhere else. **The price is
stated rather than hidden: this is a compensation, so a future change to either phase order or tick rate
silently invalidates it** — which is why `kSeparationDelayS` is pinned to `kSimTickS` by a `static_assert`
in `FBOrdnance.cpp` and cannot drift without failing the build.

**This is what E14 measured, and why E14 was right not to publish it.** The campaign breadth found
`pilot_attack_bias_s` = −0.2 s winning 25 : 9 over 154 cells. Half of that (−0.1 s) is this defect and is
now gone from every aircraft by derivation. The other half was a bias averaging out half a tick of X-3's
quantisation — a number with no source, which would have been baked into every airframe in every
geometry. A doctrine would have hidden a defect and a partition artefact inside one tuning key.

**The pilot computes no ballistics.** He holds no target position and solves no trajectory: he reads the
FireControl block like the radar altimeter before a flare. Who computes: `core/FBBallistics` (shared
forward integration) via `modules/f16/FBF16FireControl`.

The pickle is posted ONCE and never repeated on a rejection; the line `pilot ATTACK_RELEASE` records
mode, acceptance, `ttrS`, `leadS`, `biasS`, along/cross error, miss distance, throw range, time of
flight, arming margin, altitude and ground speed.

#### 4.3 Egress

**It starts when the store has LEFT, not when the thumb went down.** Between the two lies the actuation
latency, and a pilot who is already rolling in it releases out of a turn: [MESS, before] 32° of bank and
−0.6 m/s of vertical velocity at separation, 11.5 m of cross-track error and a total 79.7 m. The pilot
watches his own SMS counter (`state.Stores.ReleasedCount`) like any other instrument and keeps flying the
run-in leg until it moves; only then is the egress anchored. If the SMS block goes unreadable the pass
counts as over anyway — a jet that cannot say what it is carrying will not be flown around for another
countdown. [MESS, after] roll −0.16°, vertical velocity +0.01 m/s at separation, cross-track 9.6 m.

After the release: compute the target point ONCE (`AttackEgressTurnDeg` against the current GROUND TRACK
from the AirData block, `AttackEgressRangeM` ahead, `AttackEgressClimbM` higher), then `Direct` to it at
the speed the run-in left behind, for `AttackEgressS` seconds — then back to `Route`. Always to the
RIGHT: an escape turn has to decide on a side, and choosing the side from the geometry would be a
decision for which there is no source here [SETZ]. No ornament: a level release flies the aircraft over
its own detonation.

#### 4.4 The bias as a measuring instrument

`AttackReleaseBiasS` (variant `pilot_attack_bias_s`, band −10…+10 s) shifts EXCLUSIVELY condition 4a. A
release delayed by `bias` seconds lands one ground speed further per second — with that the mission
answers the question "does the computation do anything at all".
[MESS] `attack-late.fbm` with `set pilot_attack_bias_s 2.0`: 482 m miss distance instead of 22 m.

---

### 5. Phase `Bfm` — the only phase with its OWN control law

No autopilot mode: `Guidance = Manual`, like takeoff/flare/rollout. Reason: `Direct`/`Course` are
NAVIGATION modes whose 60° bank cap and deliberately gentle roll input (`FBFlightControl::F16`'s
`RollStickMax` = 0.15, a cruise number) are structurally wrong for a fight — and re-tuning them would
shift the numbers of every existing mission.

One tick is four steps: read the picture → choose the pursuit type → form the aim point → fly there with
the energy that is left.

#### 5.1 The law: ONE lift vector, ONE load factor

An aircraft can only accelerate along its lift axis (belly → canopy). A turn is therefore first a ROLL
that puts this axis where acceleration is needed, and then a PULL. Two things are wanted, both vectors in
the plane perpendicular to the velocity — so they are added:

```
L = a_turn·(sin φ, cos φ) + (−g·sin(roll)·cos(pitch), +g·cos(roll)·cos(pitch))      [body: right, up]
a_turn = min( V · err_rad / kBfmTurnTimeS , g_avail · g0 )
roll_cmd = clamp( atan2(L_right, L_up) / kBfmRollFullDeg , −1, +1 )
n_cmd    = clamp( |L| / g0 , 0 , g_avail )
```

φ is the direction of the steering error in the same body axes; `kBfmTurnTimeS` = 2.0 s [SETZ] is the
time in which the pilot wants the steering error gone; `kBfmRollFullDeg` = 60° [SETZ] is the roll error
that deserves full lateral deflection.

**Three behaviours fall out of this one expression instead of being coded as special cases:**

1. Zero error at any bank angle → **L** points vertically up in the WORLD → the jet rolls wings-level and
   holds 1 g. That is the tracking solution, without a tracking mode of its own.
2. Pure azimuth error in level flight → `roll = atan(a_turn/g)` and `n = 1/cos(roll)` — that IS the
   turning-flight relation, reached here instead of assumed.
3. A hard turn at 90° of bank → gravity has no component ALONG the lift axis at all, so the turn costs
   only its own g and the nose falls.

**Why `cos(roll)·cos(pitch)` and NOT `1/cos(roll)`.** The term is the projection of gravity onto the lift
axis, not the demand to hold altitude. `1/cos(roll)` would be the level-turn load factor — a law that
goes to infinity at 90° of bank and already spends 5.7 g at 80° silently on holding an altitude nobody
asked for. The chosen form lets the nose fall in the bank, and **exactly that is what makes the energy
fight possible in the first place**: altitude is traded for speed without an "energy mode" standing
anywhere.

**The axes individually:**

- **Roll** needs no damping term: the lateral stick of an FLCS airframe IS a RATE command (`f16.xml`
  differentiates `fcs/aileron-cmd-norm` against the measured roll rate), so a proportional law on the
  ANGLE error is already one on the rate.
- **Pitch** is a PI control on the load factor error (`kBfmGKp` 0.25, `kBfmGKi` 0.5, I limit ±0.6),
  because the stick-to-g authority of an FLCS is speed-dependent. Deliberately capped ASYMMETRICALLY:
  full pull, but only `kBfmPushMax` = 0.3 push — a fighter pulls and unloads, it does not push.
- **Available g**: `g_avail = clamp(CornerG·(V/Vcorner)², 1, MaxG)` — lift grows with dynamic pressure.
  With that the loop never demands a turn that does not exist, and the integrator does not wind up.

#### 5.2 Pursuit type from the geometry

The excess is a **SCHEDULE, not a threshold**: the desired closure is proportional to the remaining
range, so that the closure rate has already been worked off by the time the control range is reached.

```
ctrlMid = ½·(ctrlMin + ctrlMax)                          // control position, readable through the variant
capKt   = min( BfmMaxClosureKt , BfmBrakeMs2 / (BfmClosureGainKtPerNm·kt/nm) )    // = a/k, see below
schedKt = clamp( (R_nm − ctrlMid) · BfmClosureGainKtPerNm , ±capKt )
overtaking = validTrack && closKt > schedKt + kBfmClosureDeadKt      // dead band 40 kt
```

**What `BfmBrakeMs2` has to be a measurement OF** [MESS, this round]. The cap is `a/k` because the
schedule `c = k·(R − Rctrl)` demands a deceleration `k·c`. The quantity being decelerated is the
CLOSURE, and a closure carries the geometry as well as the drag — so the number may not be the
airframe's level-flight deceleration, which is what it was (2.4 m/s², 238 samples at 325–400 KCAS).
Measured on the thing itself — one-second windows in the stern conversion with the throttle at idle,
the speedbrake fully out and a valid track, N = 4,595 over the 16-approach gun sweep — the distribution
is **median 1.86, p20 1.16, p40 1.63, p90 5.76 m/s²**, i.e. far wider and centred lower. A braking
LIMIT takes the pessimistic end of its own distribution (a cap the pursuer meets half the time is one
he breaks half the time), so the F-16 hook is now **1.2 m/s² → cap 70.0 kt** instead of 140.

[MESS] what the old cap did on `gun-bfm`: the pursuer accelerated to 427 KCAS in the first 13 s, held
177–182 kt of closure against a schedule that never asked for more than 140, arrived at 0.5 nm with
105–120 kt against a schedule asking for 27, and flew through at 0.11 nm. On the pre-D1 model that
overshoot was recoverable because the roll asymmetry tipped it the other way; on symmetric roll it costs
the ACM box its contact and the whole attack has to be flown again. With the corrected cap the same
mission tracks at t=59.5 and kills at t=66.7 on 70 rounds.

| Type | Condition | Aim point |
|---|---|---|
| `Search` | no valid track | direction + altitude (§ 5.4), never a point |
| `Lag` | `R < ctrlMin` OR `overtaking` | BEHIND him along his path (`BfmLagTimeS`) **and** ABOVE him (`BfmYoYoHeightM · excess`) |
| `Lead` | `aspect > BfmLeadAspectDeg` OR `R > BfmLeadRangeNm` | collision lead: `t_lead = clamp(R/V, 0, BfmLeadMaxS)`, target + `v_tgt·t_lead` |
| `Pure` | otherwise | onto him |

- **Lag is TWO displacements, not one.** Behind him stops the nose from shooting out in front; ABOVE him
  actually works the excess off — pulling up out of the turn plane trades the excess speed for altitude
  instead of burning it with drag the jet does not have, and gives it back on the way down. That IS the
  **high yo-yo**. The height scales with `excess = clamp((closKt − schedKt)/BfmMaxClosureKt, 0, 1)`, so it
  unwinds by itself.
- **Lead is a TIME, not a fixed angle** — the lead shrinks with range and never demands a turn into empty
  sky.

#### 5.3 The throttle as the second half of the closure problem

Aiming behind him keeps the nose in; it does not stop a jet that is simply 100 kt faster. So the pilot
flies the SPEED that the geometry wants:

```
with a track:  speedErrKt = (v_tgt − v_own)·kt + schedKt      // schedKt is negative INSIDE the zone
without one:   speedErrKt = BfmCornerSpeedKt − casKt          // the corner, where every fight starts best
thr = clamp( kBfmThrTrim(0.6) + kBfmThrKpPerKt(0.006)·speedErrKt , 0, 1 )    // ±67 kt = idle…full
lowEnergy → thr = 1.0
cas > BfmCornerSpeedKt·kBfmOverspeedFrac(1.15) → thr = min(thr, 0.6)
Speedbrake = speedErrKt < −kBfmSpeedbrakeKt(40) ? 1 : 0
```

**"Out of energy" is relative, not absolute:**
`lowEnergy = casKt < BfmMinSpeedKt && (!validTrack || v_tgt > v_own)`. A pursuer in the control position
behind a hard, decelerating defender IS below his own corner band, and "correcting" that with full
afterburner throws him out in front.
[MESS] the absolute rule cost 250 out of 268 seconds of control position.

`lowEnergy` also acts in the aim point: `if (lowEnergy && aimU > 0) aimU = 0` — a **cap**, not a
nose-down bias. A negative altitude wish would INVERT the lift vector (the law points the lift axis at
the aim point, "below me" means 180° of roll), hence fly a split-S.
[MESS] exactly that threw the jet 2,900 m down while the pilot only wanted to recover 50 kt.

#### 5.4 The search: the datum + the uncertainty width

The search flies a **DIRECTION and an ALTITUDE**, never a point, and it flies the **DATUM** (§ 6.2), not
the last measured position.

```
if (datum.Valid && datum.RangeM > datum.RadiusM) { brg = datum.BearingDeg; aimU = datum.UpM; }
else                                             { brg = anchor(BfmSearchHdgDeg_); aimU = anchor(BfmSearchAltM_) − st.elev; }
aim = kBfmSearchRangeM(3 nm) · (sin brg, cos brg)
aimU = clamp(aimU, −tan(kBfmSearchDownMaxDeg 5°)·R, +tan(kBfmSearchUpMaxDeg 20°)·R)
```

Four individual decisions, each with a measured justification:

1. **Datum instead of a frozen measurement point.** The block falls back beyond the extrapolation window
   onto the last MEASURED position; a defender in a break has moved a quarter of his circle away from it
   by the time the search starts. [MESS] aiming at the old point produced a 1,500 m dive onto a datum
   whose owner was long gone.
2. **Inside the area the bearing is no longer information.** If the jet is inside `datum.RadiusM`, the
   opponent is just as likely to be behind as in front; steering onto the centre point one is sitting on
   makes the bearing swing through 180°, the law answers with a maximum-rate reversal, and the search
   becomes an orbit. Then the honest search is the cold one: hold heading and altitude and let the scan
   pattern work.
3. **The cold search is ANCHORED** (`BfmSearchAnchored_`): aiming at "wherever my nose is pointing right
   now" is a control loop without a reference — [MESS] the weave starts a roll, the roll turns the jet,
   the aim point follows the turn, and the search settles into a stable 80° bank orbit that searches
   nothing. As soon as a track exists again, the anchoring is released.
4. **The search is flown UPRIGHT**, with asymmetric limits (20° up, 5° down). A steep downward wish makes
   the lift vector law roll inverted (see above) — [MESS] 2,000 m of zoom and roll while the target flew
   unobserved at datum altitude in all peace. Altitude lost in a bank is recovered by CLIMBING, not by
   diving after it.

**The weave pattern** (`SearchWeaveDeg`) — the aim point is rotated about the vertical IN THE WORLD (not
degrees added to the body azimuth), so that the steering error stays a coherent direction at any bank
angle. Two patterns:

| Case | Amplitude | Period | Phase reference |
|---|---|---|---|
| no datum (never seen anything) | `BfmScanAmplitudeDeg` | `BfmScanPeriodS` | mission clock |
| datum available | `clamp(datum.HalfWidthDeg, base, kBfmScanMaxAmpDeg 45°)` | `BfmScanPeriodS · amp/base` | **start of the search** (`ScanSinceS_`) |

- **The width is what the pilot does NOT know.** A fixed amplitude searches the same few degrees whether
  he was lost a second ago or a minute ago.
- **The period grows with it**, so that the weave's own heading rate (`2π·A/T`) stays the gentle number
  the airframe hooks name: a wider search takes longer, it does not turn harder.
  [MESS] a 20°/10 s weave put the jet into a permanent 77° bank orbit and acquired nothing.
- **The phase is anchored at the START OF THE SEARCH**, so that the sweep begins ON the datum bearing —
  the most probable bearing there is — and moves outwards symmetrically. Phased on the mission clock, the
  pilot enters every search at a random point of the sine.
  [MESS, 16 merges of one geometry] before: 6 of 11 contact losses reacquired (34/80/81/86/170/240 s,
  five times never), afterwards 11 of 11 (10…141 s, median 39 s).

The weave runs as soon as `!validTrack || trackAgeS > BfmScanAfterS` — until then the nose follows the
extrapolation alone.

#### 5.5 The floor

`if (BfmFloorFt() > 0 && ra.H.Readable() && ra.AglFt < BfmFloorFt())`
→ `elErr += kBfmFloorPullDeg(30°) · clamp(1 − AglFt/FloorFt, 0, 1)`.
It stands ABOVE everything above it: a fight flown into the ground is not a fight won. Without a readable
radar altitude block it does not pull (§ 3).

#### 5.6 Gun tracking (section 3b/3c) — a LAW, not a number

**Why the phase needs a second law at all.** Pursuit steering (§ 5.2) aims the nose at a POSITION — where
he is, where he will be in two seconds, or a point behind him. None of these is where a gun has to point:
a 20 mm round takes a third of a second for 300 m, and the target moves a wingspan of angle in that time.
Pure pursuit inside gun range is therefore a guaranteed miss — [MESS] with the pursuit law alone the aim
error IN the control position runs to ~9° against a funnel tolerance of ~1°, and the trigger never comes
down. That is why in the funnel the steering error becomes the LEAD error published by the fire control
computer: the same solution that the trigger gate reads too, so aiming and shooting cannot diverge. The
pilot computes nothing in the process — he reads a number off the bus like any other instrument.

**Entry gate.** Three conditions; the last two prevent this from becoming a worse pursuit law than the
one above:

```
gunTrack = Gun.H.Readable() && Gun.Ready && fc.H.Readable() && fc.GunValid
        && fc.GunSpanMr >= fc.GunFunnelBottomMr        // the guide's own range check
        && |g.AzDeg| <= BfmControlAtaDeg               // he is IN FRONT
        && fc.GunAimErrorDeg <= BfmGunTrackMaxErrDeg   // it is a TRACKING and not a turning problem
```

A gun solution exists for a target ANYWHERE, including one that has just pulled past the wing — the
demanded barrel direction then lies 170° off the nose. Feeding that as a steering error into the lift
vector law produces a violent, energy-destroying reversal: [MESS] flew the jet into the ground in 158 s.

**Why there has to be a rate term at all.** The law controls an ERROR and commands a turn rate `err/T`.
Against a turning target, however, the demanded barrel direction is not a constant but a **RAMP** — it
travels through the funnel — and a control loop that answers a ramp with a proportional term alone stays
constantly behind by (ramp rate × its own time constant).
[MESS] against a defender in a maximum-rate break: the solution travels ~1 °/s, T = 2 s, aim error never
below 4.6° against a ~1° funnel tolerance — two bursts, 70 rounds, no hit. That is not a shooting
problem, it is a control loop TYPE problem.

**Which rate — and here the obvious implementation is the wrong one.** The published lead is
BODY-referenced. Differentiating it therefore measures one's own pulling motion just as much as the
target's motion, and in the steady state (exactly the one whose lag one wants to remove) this derivative
is almost zero, while the solution travels through the sky as fast as ever. Removing one's own gyro rates
again would be arithmetic on a quantity that has already mixed them in. So:

```
(be,bn,bu) = FBBodyLosToEnu(roll,pitch,yaw, fc.GunLeadAzDeg, fc.GunLeadElDeg)   // into the WORLD
rate  = α filter( d(be,bn,bu)/dt ),  α = kBfmLeadRateAlpha = 0.4
k     = kBfmTurnTimeS · min(1, rateMax/|rate|),  rateMax = CornerTurnRateDegS()
(azErr,elErr) = FBEnuToBodyLos( (be,bn,bu) + k·rate )                            // back into the body
```

Nothing of one's own roll, pitch or yaw motion survives this round trip — that is the point. Handed to
the existing law, this yields a turn rate command `err/T + ω`: the P term takes the error out, the rate
term keeps the nose on a solution that does not stand still. The limitation to `CornerTurnRateDegS` is
not caution: a faster travelling solution is no longer a tracking problem, and demanding it would throw
the lift vector at a point the airframe cannot reach. The filter exists because the estimate is the
difference of ONE published float over two 10 Hz ticks — fast enough for a reversal, slow enough that a
noisy look does not swing the nose.

**The integrator and its derivation.** The feedforward term leaves standing exactly what the RATE
ESTIMATE itself has wrong: a filter has lag, and the motion of the demanded barrel direction depends
weakly on one's own speed, so the estimate is a little too small and the loop settles a little behind.
[MESS] steady state 1.45° against a solution tolerance near 0.4°. A constant offset in a loop that
already has a P and a rate term is exactly the case for an integrator — and here it converges on a
definite number: in equilibrium the error is zero and the integrator holds exactly the shortfall of the
feedforward term.

**Its gain is not a matter of taste** [HERL]: the law commands a turn rate `(e + I)/T` against an angle,
so the loop closes as

```
s² + s/T + Ki/T = 0        →    ζ = 1 / (2·√(Ki·T))
Ki = 1/(2T)                →    ζ = 1/(2·√(0.5)) = 0.7071
```

hence `kBfmTrackKi = 0.5 / kBfmTurnTimeS` = 0.25 s⁻¹ at T = 2 s — the textbook root "settles without
overshoot", and nothing further has to be chosen. [MESS] twice that (ζ = 0.5) was tried first: against a
defender in a break a clear gain, against a straight-flying one the loop rang and the funnel time
collapsed. Wind-up limit `kBfmTrackIMaxDeg` = 10°; a funnel break resets both integrators hard.

**The limitation to the entry gate.** The three terms (P, rate, I — all three ANGLES here) can add up to
a demand far larger than the aim error itself. This law is however only responsible for the TRACKING
problem, so exactly the threshold that let it in also limits what it may demand:

```
mag = √(azErr² + elErr²);  lim = BfmGunTrackMaxErrDeg;  if (mag > lim) scale both to lim
```

[MESS] without it the combined demand reached sixty degrees at the shortest range, the jet answered as it
answers any sixty-degree demand — full deflection in both axes — and departed (LOC knockout at 150 °/s of
roll rate).

The state change is logged: `pilot GUN_TRACK` / `pilot GUN_BREAK` with range, time of flight, aim error,
lead angles, funnel geometry and drum contents. In the funnel the scoreboard reports `Lead` — it IS lead
pursuit.

**Overall effect** [MESS, eight approaches per defender, before/after]: funnel time 3.2 → 20.7 s
(straight) resp. 0.0 → 21.6 s (turning); rounds on target 11.9 → 111.2 resp. 0.0 → 120.4; kills 0 → 5
resp. 0 → 7 out of eight runs each; mean tracking error 10.5° → 6.9° resp. 11.9° → 4.1°.

#### 5.6b The lock in a turning fight — `BfmDesignate` (MiG-29 stage 2c)

The ACM modes of a set that has them lock the nearest firm track by themselves
(`FBRadarScanVolume::AutoAcquire`) — in a knife fight nobody operates a radar — and on such an aircraft
this branch never runs, because a lock already exists. A set whose close-combat patterns do NOT
auto-lock still has the contact on the scope and a thumb on the TMS: the designation is then a piece of
COCKPIT WORK like any other — over the command bus, with its latency, refusable, and at the pilot's own
action rate (`kInterceptActionS`) rather than at 10 Hz. Friendly IFF replies are skipped, the same rule
the intercept uses; silence proves nothing.

Without it the BFM phase on such an aircraft is blind: no lock, no `FBBfmTrack`, no aim point, and the
law flies its cold-search pattern with the opponent in front of it (measured on `mig29-gun`'s geometry:
**0 lock ticks in 134 s**). With it the lock is available — though on the MiG-29 the remaining problem
is ACQUISITION rather than designation, and that is recorded as a gap in
[`modules/mig29/module.md`](modules/mig29/module.md) rather than papered over here.

#### 5.7 The roll-rate limiter — the third law

The lift vector command is an ANGLE error, the F-16 lateral stick a RATE command: a large error means
full deflection for as long as it lasts — and the largest error this law can produce is 180° (it always
takes the short way). [MESS] a reacquisition at 3.7 nm demanded 230° of roll, the jet answered with
150 °/s for three seconds, and the flight monitor called it what it looks like from outside: a departure.

```
kBfmRollRateMaxDegS = 180 / kBfmTurnTimeS = 90 °/s
if (p·rollCmd > 0)
    lim = clamp((cap − a·|p|) / (K·(1−a)), 0, 1);   if (|rollCmd| > lim) rollCmd = ±lim
```

**It is a plant INVERSION, not a regulator and not a recursion:** `p[n+1] = a·p[n] + K(1−a)·u[n]` solved
for the `u` that puts the NEXT rate exactly on the cap. Its output depends only on the MEASURED rate, so
it is memoryless and has a fixed point by construction. It still only REDUCES — never counter-stick,
never more than the raw command.

**The plant is identified, not assumed** [MESS, ARX(1) over 15,325 ten-Hz samples from eight BFM runs,
restricted to samples below the cap so the fit is open-loop]: `a` = 0.734 (roll lag 0.323 s), `K` = 78.7
°/s per unit of stick. The same fit over ALL samples gives 0.772 / 90.5, i.e. a limiter gain 7 % away —
the identification carries.

The two plant constants are **module hooks** (`BfmRollPlantA`/`BfmRollPlantKDegS`) since a second
airframe flies this law, and they must be: the cap INVERTS the plant, so with another aircraft's
numbers it is not a cap but an oscillator. The MiG-29 rolls 2.6× harder for the same stick (a = 0.819,
K = 201 °/s against the F-16's 0.734 / 78.7, identified from its own BFM samples by the same ARX(1)
fit), and with the F-16's constants its cap never bit: ±130° of bank in a limit cycle and a departure
after 8.9 s.

**The generic default is 0/0 — no plant identified, hence NO cap** (it was the F-16's two numbers until
this round, which made every airframe that had not been identified fly this jet's inversion silently).
The F-16 declares 0.734 / 78.7 in `FBF16Pilot`, the MiG-29 its own, and a catalogue row its measured
pair from step 7 of the flight-model recipe. Two guards, not one: `modules/air` refuses `set task bfm`
without a plant, and `BfmCommands` skips the cap when the plant is undeclared (with 0/0 the inversion
would divide by zero). Byte-identical across all 296 missions — the phase is unreachable without a
plant.

The COMMANDED rate cap (`BfmRollRateMaxDegS`) went the same way: its generic default is the closed form
of the pilot's own reversal window (`180 / kBfmTurnTimeS`, a number of this class), and the F-16
declares 90 °/s as the value its 16-approach sweep measured — the two coincide, and the override is
there because the number was measured on the airframe rather than inherited from the pilot.

#### 5.7.1 Why the previous form had to go (rejected: `cmd_prev · cap/rate`)

The old law scaled the command that had PRODUCED the measured rate: `lim = |cmd_prev|·cap/|p|`. The
argument was that on a rate stick command and rate are proportional, so the fixed point of the recursion
is the cap. It holds only if `p` is the previous command times a constant — i.e. if the airframe answered
within one sample.

It does not. With the identified lag spread over the samples (`p[n] = K·Σ(1−a)a^j·u[n−1−j]`), linearising
the recursion in `x = ln(u·K/cap)` gives

```
x[n] = x[n−1] − Σ (1−a) a^j x[n−1−j]      ⟹      z² − 2a·z + a = 0      ⟹      |z| = √a
```

**so it is not a limiter with a fixed point, it is a lightly damped OSCILLATOR** — |z| = 0.86 at the
identified `a`, i.e. it decays only 5:1 per cycle, and any extra phase (the g loop, the FLCS, the fact
that the limiter switches in and out) pushes it to the unit circle. Under a raw command that swings in
AMPLITUDE the two sides of the ratio no longer belong to the same instant at all and the map is
unbounded for a step at a time.

Measured consequences, all against the number the source declared:

| Instrument | Recursion | Inversion |
|---|---|---|
| `missions/bfm-pointblank.fbm` (0.8 nm head-on — the swinging stimulus) | peak **1.37 ×** its own cap, 9.2 s above it | peak **0.89 ×**, **0.0 s** above it |
| 16-approach gun sweep, peak / cap | **1.52 ×** | 1.23 × at the same cap, 1.18 × at the committed one |
| 16-approach gun sweep, stretches ≥ 4 s spent above 0.8 × cap | **11** | **0** |
| ringing while active (pooled autocorrelation of \|p\| over those 11 stretches) | first minimum 0.3 s, first recurrence **0.70 s** | no stretch long enough to measure |

The measured period is shorter than the linear prediction (1.16 s), and that is expected: the limiter is
not continuously active, and every engage/disengage is a switching mode on top of the loop mode. The
substantive measurement is the ratio, not the period.

#### 5.7.2 The cap: now a closed form, and re-measured against the corrected law

`120 / kBfmTurnTimeS` was a knob. The **largest** steering error this law can produce is 180° (it always
takes the short way), and the sentence the old value only gestured at is that even the worst case must be
flown in the time constant the roll serves — so `cap = 180° / kBfmTurnTimeS = 90 °/s`. With it,
`kBfmReverseS` (§5.2) becomes identically `kBfmTurnTimeS`, which is what it always meant: **a reversal IS
a 180° roll.**

The old measurement ("60 °/s → no departures, 90 °/s → six") was taken on a law whose nominal was not its
cap — 60 nominal was 91 achieved. Re-measured on the corrected limiter [MESS, 16-approach sweep,
8 geometries × straight/turning defender, plus the eight committed BFM missions]:

| cap °/s | kills straight | turning | total | `gun-bfm` | departures in the 8 committed missions |
|---:|---:|---:|---:|---|---:|
| 60 | 0/8 | 8/8 | 8/16 | **LOC** | 1 |
| 75 | 1/8 | 8/8 | 9/16 | LOC | 1 |
| 82.5 | 3/8 | 8/8 | 11/16 | LOC | 1 |
| **90** | **4/8** | **8/8** | **12/16** | **KILL** | **0** |
| 97.5 | 1/8 | 8/8 | 9/16 | TIMEOUT | 0 |
| 105 | 2/8 | 8/8 | 10/16 | LOC | 1 |
| 200 (limiter effectively off) | 1/8 | 6/8 | 7/16 | TIMEOUT | 0 |

Derivation and measurement land on the same number, which is the only reason to trust either. Note what
the first row says: keeping the OLD cap with the NEW law is a **regression** — the exact limiter tightens
the real bound by a third without anyone having decided to, and `gun-bfm` departs at t = 214.6 s.

The last row is the control that the limiter still earns its place: with it effectively removed the
sweep drops to 7/16 and the peak rate reaches 132 °/s.

**Costs, declared** (all five affected missions are BFM, no other mission in the tree moves by a byte):
`gun-dry` 3 → 1 (all twelve rounds now arrive; the drum still empties first and the header carries the
new reading rule), `gun-bfm` kill t = 66.7 → 84.2 s, `bfm-blind`'s blind interval 41 s → 199 s (that
number is chaotic across every cap tested — 50.9 / 120.0 / 209.0 with nothing monotone between — and the
mission's four claims all still hold), and one departure in a non-committed sweep geometry at t = 232.6 s
where the old law had none.

#### 5.7.3 The same 180° as a SUSTAINED bound — the roll extent

§5.7.2's cap is a **peak**: it says how fast a reversal may be flown, not how long. Nothing in the law
said a roll had to END. That is not an oversight one can leave standing next to a judge whose rule is a
SUSTAINED quantity — `core/FBFlightMonitor` trips on |ω| > 60 °/s **held for 3 s**, so a cap of 90 °/s is
1.5 × the judge's threshold and is survivable only as long as no geometry holds it. One does.

**The regime, measured** [MESS, `duel-merge` at `71cb99f`, the F-16 pilot]: a head-on merge at **898 kt of
closure**, range collapsing 0.79 → 0.03 nm in 3.2 s, LOS rate peaking at **543 °/s** against a corner turn
rate of 15.8 °/s — 34 × more than the airframe can follow. The jet rolled **290° in 3.1 s** at 95–109 °/s
with a steering error of only 10–20°, and departed at t = 18.0. Against that, the same instrument on the
committed missions: `gun-bfm` peak LOS rate **17.7 °/s**, `bfm-basic` 6.2 °/s. The discriminator is not
range and not the error — it is the LOS rate, and behind it the closure.

Neither existing guard sees it. The **conversion guard** (§5.2) has exactly this premise ("the LOS turns
faster than the airframe can") but tests the target's angular OFFSET: its zone floors at
`kBfmConvertErrDeg` = 90° while the merge's error never exceeded 76.6°, so it never armed — and its
action freezes the turn SENSE, which permits an unbounded roll in that sense forever. The **rate cap**
did arm, and held 103–109 °/s against its declared 90 (1.15 ×, consistent with §5.7.1's measured 1.23 ×
over the sweep: the plant identification is a small-signal fit and the limiter operates at full
deflection).

**The missing half of the same closed form.** The law always takes the short way, so no correction can
ever require more than 180° of roll in one direction: after 180° the lift axis has pointed everywhere
once. A demand that still asks for more is not being caught — it is rotating with the aircraft. So the
sentence that gives the peak cap gives the sustained one over the same window:

```
kBfmRollExtentDeg = 180                                  // a reversal, the same 180° as the cap
win  = ∫ p dt over the last kBfmTurnTimeS                // SIGNED, from the MEASURED rate (ring buffer)
if (win · rollCmd > 0)  cap ← cap · clamp(1 − |win|/180, 0, 1)
```

and the existing plant inversion then flies that cap. Three properties, all of them consequences rather
than choices:

- **Empty window ⇒ unchanged.** `cap · 1.0` is exact, so a mission whose BFM roll never fills the window
  is byte-identical. Measured: of 79 missions, **6 move and 73 do not** — and the 6 are exactly the BFM
  runs whose 2 s roll window exceeded ~110°.
- **A reversal keeps its full authority**, because the window is SIGNED: rolling the other way unwinds it.
  A scissors does not accumulate; only a monotone roll does.
- **The sustained fixed point is half the peak, exactly.** `p = cap·(1 − p·T/180)` with `cap·T = 180`
  gives `p = cap − p`, i.e. `p = cap/2` = 45 °/s for the F-16 — and with the limiter's measured 1.15 ×
  over-hold, ≈ 52 °/s, which with a hard pull's 20 °/s of q leaves |ω| ≈ 55 against the judge's 60. That
  margin is thin and it is the whole point of the number; it is why the softer variants below fail.

**Measured, `duel-merge`** (F-16 pilot): longest continuous |ω| > 60 stretch **2.9 s → 0.8 s**, roll per
2 s window **195.8° → 110.8°**, and the departure is gone — the F-16 flies the full fight instead of
18.0 s of it (§ the merge row in [`duels.md`](duels.md)).

**Measured, the 16-approach sweep** (`sim/tools/fb_bfm_sweep.py`, 8 geometries × straight/turning
defender — the sweep was a scratch script until this round and is now a committed tool, so the numbers
below are reproducible): departures **6 → 0**, kills **5/16 → 7/16**, peak roll rate **182.2 → 103.4 °/s**
(2.02 × → 1.15 × the cap), longest |ω| > 60 stretch **4.9 s → 1.5 s**, seconds spent above the 90 °/s cap
**63.2 → 5.4**.

**The sweep's kill count is a coarse instrument, and this is the measurement that says so.** Perturbing
`gun-bfm`'s spawn longitude in 0.8 m steps over ±3 m gives KILL at t = 77.9 / 78.7 / 78.8 / **84.3** /
81.6 / 197.1 s in six of eight cases and no kill in the other two (one of them a LOC at t = 372.5 s in
the committed geometry itself). A ±2-kill band is therefore chaos, not signal; the DEPARTURE count and
the pooled rate statistics are the parts of the sweep that carry information.

**Costs, declared — the complete ledger.** 79 missions run, **6 move, 73 byte-identical**, and exactly
one verdict changes.

| Mission | verdict | what moved, and why |
|---|---|---|
| `gun-bfm` | 1 → 1 | its 2 s roll window peaked at 186.8°, just past the one reversal the bound allows, so the taper bites late in the pursuit and the conversion tips differently. Kill t = 84.3 → **106.5 s** on 71 rounds instead of 35; four tracking entries instead of three. Flying strictly better: peak 104.8 → 97.6 °/s, roll/2 s 186.8 → 116.6°, longest |ω| > 60 stretch 1.9 → 0.7 s, `bfm_ctrl_s` 12.9 → **23.7** |
| `gun-dry` | **1 → 3** | the ONE squeeze twelve rounds buy falls 19 s earlier (t = 83.6 → 64.4). All twelve still arrive — 4.0 of 6 at 0.62 m — but into the FORWARD zone at 25.7 kJ/m² instead of the CENTER zone at 153 kJ/m², below what disables. `gun DRY` at t = 64.7 with `fired=12` and no refusal line: everything the file exists to show is intact, and twelve rounds are one squeeze, i.e. one geometry (see the chaos measurement above) |
| `bfm-blind` | 3 → 3 | its two named times are unchanged (`RADAR_LOST` t = 9.9, `RADAR_LOCK` t = 209.0) and the sequence it claims is complete. `bfm_lock_s` 99.1 → 50.4 and `bfm_ctrl_s` 48.4 → **0.0**, causally: the long one-direction conversion of its SECOND engagement is now flown at the sustained half-cap and does not arrive |
| `mig29-bfm` | 3 → 3 | no KO, `bfm_lock_s` 296.3 unchanged, pursuit (not search) for 296.3 s — but `bfm_ctrl_s` 88.2 → **0.0** and closest approach 1.19 → 1.70 nm. The MiG pays most because its peak hook is already 60 °/s (§5.10 screw 4), so its sustained roll is 30. Its reading rule was rewritten around what the run does support |
| `bfm-pointblank` | 3 → 3 | the claim it exists for is **unchanged to the digit**: peak 80.5 °/s = 0.89 × its cap, 0.0 s above it. Only the trajectory after t = 5.6 differs |
| `duel-merge` | 2 → 2 | the F-16's departure at t = 18.0 is gone and the run goes to 232.3 s; the exit code stays 2 because the MiG now ends it by flying into the ground. [`duels.md`](duels.md) row 8 |

**Rejected: putting the MiG's `BfmRollRateMaxDegS` back on the derived 90 °/s** now that a law covers what
§5.10 screw 4 covered with a number. It restores `mig29-bfm` (`bfm_ctrl_s` 0.0 → 86.2, closest approach
1.34 nm) and costs a MiG **departure** in `duel-merge` at t = 103.6 (stall/mush) — i.e. it reopens the
exact D1 claim screw 4 closed. The number is therefore not redundant: a lost control position is a fight
that does not convert, a departure is a K.O.

**Rejected: enforcing the constraint exactly** (`if |win| ≥ 180 → cap = 0`, bang-bang instead of the
taper). It is the literal constraint and it moves only 3 missions instead of 6, but it does not solve the
problem: the roll spends its whole budget at the full 90 °/s, stops for ~0.3 s while the window drains,
and re-arms — a stutter whose duty cycle keeps |ω| above the judge's threshold. [MESS] sweep departures
**3** (against 0 for the taper) and kills 5/16; `gun-bfm` loses its kill outright (1 → 3). The taper is
kept because the sustained fixed point, not the budget, is what the judge measures.

#### 5.8 The trigger (`BfmGunfire`)

No second aiming — a finger. It does three things:

1. Checks whether the gun is armed and loaded (`Gun.H.Readable() && Gun.Ready`) and a valid solution
   exists.
2. **Predicts the aim error to the moment the rounds LEAVE**:
   `pred = |err + (dErr/dt)·(LatencyS(GunTrigger) + burstS/2)|`.
   Every HOTAS action arrives one latency later, and at a fighter's tracking rates the aim error moves
   ~2 °/s — at 300 m that is ten metres of miss distance per second of delay.
   [MESS] bursts commanded on a 0.35° solution arrived on a 1.7° solution and missed by 8 m.

   **The horizon is the squeeze's, not the round's — and it used to be the round's.** `FBGunSolveLead`
   answers *"where must the bore point for a round fired NOW to meet the target LATER"*: the target's
   motion over the time of flight is **already inside the solution**, so a round leaving the muzzle at
   τ misses by about `R·tan(err(τ))` and nothing that happens to `err` afterwards reaches it. The
   squeeze commanded at t produces rounds over `[t+L, t+L+burstS]`, whose mean leaves at
   `t + L + burstS/2` — **0.35 s**, against the `L + GunTofS` ≈ **1.0 s** the form carried before.
   [MESS, `xmergesplit`, the five bundles of the first squeeze] `R·tan(GunAimErrorDeg)` read at the tick
   each bundle left predicts that bundle's own closest approach: **54.9 / 47.5 / 39.6 / 37.3 / 36.6 m**
   predicted against **48.7 / 42.2 / 37.2 / 34.6 / 34.9 m** measured.
   [MESS, same run, 13 squeezes] with the round's horizon, **11 of 13** were commanded with the measured
   aim OUTSIDE the trigger's own gate — the first one **14.6×** outside it (4.808° against 0.329° at
   766 m) — because a ONE-TICK derivative was extrapolated a full second. The drum was 88 rounds down by
   the time the range came inside 630 m.
3. **Holds the pipper tighter than the funnel.** The funnel walls are the target's WINGSPAN; a solution
   just inside puts the pattern half a wingspan beside its centre.
   `pred > Tuned(GunFireTolFrac, BfmGunFireTolFrac()) · fc.GunTolDeg` → do not fire.
   0.35 [SETZ] puts the pattern into the fuselage instead of somewhere across the wingspan.
4. Squeeze over the bus (`GunTrigger`, value = `BfmGunBurstS`), and not again before that duration has
   elapsed (`GunNextS_`).

**It NEVER checks whom it is shooting at** — and cannot: the pilot sees a radar contact, not a cast list.
The mission declares the cast, the trigger answers the funnel.

#### 5.9 The control position and the scoreboard

```
inControl = validTrack && Locked && ctrlMin ≤ R ≤ ctrlMax
         && AspectDeg ≤ BfmControlAspectDeg && |AzDeg| ≤ BfmControlAtaDeg
```

Only `ctrlMin`/`ctrlMax` go through the variant table (`pilot_bfm_ctrl_min_nm`/`_max_nm`) — it is the one
BFM number that a mission really has to change: a missile holding position lies OUTSIDE the gun funnel
[DOK `weapons.md` §2.5: 600–3,000 ft], so a gun brief IS a different control position and nothing else.

#### 5.10 Surviving the law on a RAW airframe — the four MiG-29 screws (D1)

The whole of §5 is written for the F-16, whose JSBSim deck carries its own FLCS: the pilot commands raw
`Manual` stick and the deck holds α and roll rate whatever the stick asks. The MiG-29's deck has NO FLCS
— `systems/FBFlightControl` is the only thing between the BFM law and 35° of stabilator — so the law
departed it in 22.8 s from a nose-on merge ([`duels.md`](duels.md) D1). The fix is **four measured
screws, each airframe-scoped so the F-16 is byte-identical** (verified: 13 F-16 BFM/gun/BVR/attack
missions unchanged to the bit). Diagnosed in order, each exposing the next:

1. **The pitch-deflection cap `PitchStickMax` must bind at the handstick.** It bound only on the FLCS
   path; the `Manual` path passed the pilot's up-to-1.0 pitch through, and on the MiG (`PitchStickMax`
   0.6) that is 35° of stabilator = a tumble. Now clamped on both paths. F-16 `PitchStickMax` = 1.0,
   BFM pitch ∈ [−0.3, 1.0] → no-op. [MESS] merge KO 24.7 s stall/mush → 7.7 s roll (the α tumble gone,
   the next screw exposed).
2. **The α limiter must be allowed to PUSH to recover.** The FLCS path let its `byAlpha` go negative
   (active push); the `Manual` path clamped it to ≥ 0 (never push), leaving the raw airframe no
   recovery authority when α overshot the 26° SOS at low speed — it ran to 150° and mushed. Now the
   `Manual` limiter may push, bounded to −`PitchStickMax` (the airframe's own deflection, not the
   −44.6 the first-tick `alphaDot` glitch once produced — that is caught by `AlphaPrimed`). [MESS,
   `mig29-bfm`] pursuit α held to ≤ 27 instead of 150; the mush is gone, the next screw exposed.
3. **`BfmSearchRollCap` — the search is a scan, not a combat roll.** The cold-search aim is a GUESS;
   flown with full roll authority on a jet that rolls 2.6× harder (K=201), the platform's own attitude
   drift drives the body-frame steering error into a roll limit cycle — the nose never settles on the
   antenna, no contact is ever built, and the monitor reads the sustained roll rate as a departure.
   Only the ROLL is capped (the g/pitch loop keeps holding altitude, else the scan drifts in pitch and
   climbs/dives away — both measured). Hook, generic default 1.0 = full deflection, i.e. the cap never
   bites and claims nothing; the F-16 DECLARES 1.0 (its search legitimately uses full roll —
   `bfm-blind` byte-identical), MiG 0.20 (≈40 °/s steady). [MESS] with it the MiG acquires and
   locks a trail defender (lock_s 0 → 58+).
4. **`BfmRollRateMaxDegS` — the commanded roll-rate cap is a hook.** The default `180/kBfmTurnTimeS`
   = 90 °/s is the F-16's, and it stays the closed form (a reversal is a 180° roll in the time
   constant). On the MiG the plant inversion's known intersample overshoot (§5.7) carries 90 °/s to
   ~118 °/s between two 10 Hz ticks, and a pursuit PIO then sustains a rate past the monitor's
   60 °/s-for-3 s threshold. The MiG commands 60 °/s so the loop can hold it. Only the LIMITER reads the
   hook; the reversal-zone timing (`kBfmReverseS`) stays tied to `kBfmTurnTimeS` → byte-identical.
   [MESS, `mig29-bfm`] cap 90 = departure at t=154; cap 60 = full run, lock_s 203, ctrl_s 5.3, no KO.
   Since §5.7.3 the hook is the PEAK of a cap that also carries an extent bound; the MiG's 60 is
   unchanged as that peak, and its sustained fixed point follows the same `cap/2` (30 °/s). It was
   RE-TESTED against the law rather than assumed: back on the derived 90 the MiG departs `duel-merge` at
   t = 103.6 (stall/mush), so the number still carries something the law does not.

Screws 1–2 live in `systems/FBFlightControl`'s `Manual` branch (the airframe layer); 3–4 are pilot
hooks. The result: `mig29-bfm` flies a full BFM run with no flight-monitor KO, acquires, locks and works
into the control position; `duel-merge` no longer departs the MiG (exit 2 → 3). Both the F-16 and the
MiG's BVR/takeoff/landing missions keep their outcome (`mig29-full` touchdown 143.4 → 143.7 kt — the
correct `PitchStickMax` now binding in the flare, within the documented band).

**What screws 3–4 did NOT cover, and why the F-16 needed §5.7.3 anyway.** Both are per-airframe NUMBERS,
and the F-16 kept its own — which was correct as far as it went: the MiG's failure was a limit cycle its
own plant gain drove, and a number fixed it. The F-16's failure is a different thing in the same place: no
number is wrong, the law simply has no notion that a roll must END. That is why the answer is a law and
not a third hook, and why it applies to both airframes through their own peak.

#### 5.10a The FIFTH screw — the airframe's own RATE DAMPER also binds at the hand stick

Screws 1–2 moved the airframe's deflection cap and its α limiter from the FLCS path of
`systems/FBFlightControl` onto its `Manual` path, on the argument that *"a stick force does not know
which mode the autopilot is flying"*. **The same sentence is true of a damper, and the damper was still
FLCS-only.** `KqDamp` (pitch rate) and `KpDampRoll` (roll rate) exist in that class *because* the
MiG-29 has no FLCS in its deck — they stand for the SAU-451 **DAMPER**, which
[`modules/mig29/flight-model-spec.md`](modules/mig29/flight-model-spec.md) §7.4 maps in as many words
(*"three-axis rate damper — this **is** `FBFlightControl`'s inner rate loop"*), which `DCS-FM p.34`
describes as improving stability *"with **manual** control over the entire operational … range"*, and
which `DCS-EA p.64` says *"in most cases of operations it must be enabled"*. BFM commands `Manual`.
**So this airframe fought every close engagement with its damper switched off.**

The reproducer is one aircraft and no opponent — a MiG-29 on `set task bfm` with the nearest hostile
100 km away, i.e. a pure cold anchored search (§5.4), 300 s, three start altitudes. Same mission text
one module over for the F-16:

| cold BFM search, 300 s | mean `bfm_gcmd` | mean \|bank\| | p95 \|VS\| | result at 1,000 / 1,500 / 3,000 m |
|---|---|---|---|---|
| **MiG-29, damper FLCS-only** | **4.57 g** | **76°** | **183 m/s** | CFIT **12.7 / 15.1 / 188.6 s** |
| MiG-29, damper at the hand stick | 1.11 g | 24° | 4 m/s | survives all three |
| F-16 (its deck damps itself) | 1.22 g | 40° | 9 m/s | survives all three |

The mechanism is legible in the first eight seconds: the pilot's g loop is `0.25·gErr + I`, an F-16
loop, and behind it the F-16's `elevator-cmd-norm` enters a deck FLCS that closes g itself, while the
MiG's enters an ARU gearing and a 2.0/s stabilator actuator and nothing else. [MESS] at t = 0.6 s a
0.32 pitch stick had already produced **3.05 g against a 0.37 g command**; the loop then bang-bangs
between `+PitchStickMax` and `−kBfmPushMax` at a ~1.5 s period, `nz` runs −2.5 … +10.6 and α −4 … +19°,
the steering error never closes, the g demand saturates at `gAvail`, and the fight becomes a ±1,000 m
roller coaster that eventually meets the ground. The F-16 on the same tick holds `gcmd ≈ nz ≈ 1.05`
with 0.02 of stick.

**It is a damper, not a limiter, and it is gated on its own gain** — the same idiom as `AlphaLimitDeg`
and `GLimitG` beside it, so a cell whose deck damps itself never enters the branch and is byte-identical
as a STRUCTURE. (Without the gate `-0.0 - 0.0·p` flipped exactly one `rollCmd` field in `gun-bfm` and
`gun-dry` from `-0.000000` to `0.000000` — physically nothing, in a byte gate everything.)
**The yaw axis stays out:** the real damper has three, but FlightBox's rudder branch is measured OFF on
this airframe (`KNy = KNyi = 0`, departure at t = 28 s with the generic gains), so a yaw gain here would
be invented.

What it bought, beyond the CFIT: `mig29-bfm`'s control position, which §5.7.3 had booked as the price of
the roll-extent bound. [MESS] `bfm_ctrl_s` **0.0 → 287.6 s** of 300 and closest approach **1.70 → 0.65
nm**, with `BfmRollRateMaxDegS` unchanged at 60 and `bfm_lock_s` unchanged to the digit (296.3 s) —
i.e. past the 88.2 s the bound was blamed for costing. Both earlier readings measured correctly and
attributed wrongly: no pursuit on this airframe could converge while the short period was undamped.

#### 5.11 The WVR missile shot (`BfmMissileShot`) — the phase's second weapon

**Written as a specification before it was built**, because the question this round had to answer is not
"can the fight fire a missile" but "what may it fire at, and what does the shot then cost the shooter".

**Five gates, each an instrument reading, no new arithmetic.** Everything below already exists: the
launch zone is `weapons/FBLaunchZone`'s (the same three numbers the intercept phase gets), the lock is
the one the SMS's `RequiresLock` interlock reads, and the round's own limits are its catalogue row.

| # | Gate | Where it comes from |
|---|---|---|
| 1 | the SELECTED store is an INFRARED round | `state.Stores.Station[SelectedStation−1]` → `FBStoreSpecOf` → `Seeker == FBSeekerKind::Infrared`. He checks what is on the rail rather than asking for a rail: [`duels.md`](duels.md) D4 says `WeaponSelect` is `NotImplemented` on both modules, so weapon choice is not a decision this pilot has. A radar round in a knife fight is a different weapon with a different rule and this gate declines it |
| 2 | a LOCK | `state.Radar.LockIndex >= 0` — the same bit the interlock reads, so the pilot cannot ask for something the jet will refuse |
| 3 | inside the LAUNCH ZONE | `fc.DlzValid && fc.InZone`, i.e. Rmin ≤ R ≤ Raero. In a merge this reduces to **Rmin**, which is `closure · ArmingS + kLaunchZoneMinTurnM` — at 700 m/s of head-on closure, 1,000 m. **The intercept's Rtr discipline is expressly NOT applied**: Rtr means "the round arrives even if he turns and runs", which buys a first shot from far out against somebody who might leave. A target already in a turning fight is not going to run, and demanding Rtr inside two miles would decline every shot the phase exists to take |
| 4 | inside this aircraft's CUEING limit | `\|ATA\| ≤ BfmWvrCueDeg()`, a module hook, because the documented limit is the AIRCRAFT's and not the round's: [`modules/mig29/weapons.md`](modules/mig29/weapons.md) §3.3 states it in as many words — *"the 9-12's high-off-boresight capability is bounded by the HMS (±60° az) rather than by the missile (75° gimbal) … the CUEING limit is the module hook that decides whether a shot is offered"*. The generic default is **< 0 = the round's own `GimbalHalf`** (AIM-9M ±30° [T4], R-73 ±75°), the MiG-29 overrides with the Shchel-3UM's **60**, and the F-16 DECLARES < 0 because no HMCS is modelled on it |
| 5 | one shot at a time | `TimeS_ ≥ BfmShotNextS_`, re-armed on the OBSERVED release (`Stores.ReleasedCount`, an instrument, not a memory of one's own thumb) to `max(InterceptShotSpacingS(), fc.TimeToImpactS)`. At a 1–4 s WVR time of flight it is the time of flight that binds, which is the rule the intercept phase already carries |

**Aspect is NOT a gate, and that is a finding rather than an omission.** Both rounds in the tree are
documented all-aspect (AIM-9M; R-73 *"30 km forward hemisphere at high altitude"*, `DCS-FM p.72`), so a
rear-quarter rule would be a restriction the sources do not carry. What genuinely makes the head-on shot
worse is modelled and is not the pilot's business: `sensors/FBIrstSystem`'s aspect law makes the target
dimmer from the front, and a flare seduces the head more easily there — measured, both rounds decoyed at
`tgtIntensity = 0.16` ([`weapons.md`](weapons.md) State, [`duels.md`](duels.md) D5).

**Own load factor is NOT a gate, and the reason is that there is no mechanism.** A round leaves with the
launcher's attitude and velocity (`FBStoreRelease`, `HaveRail` false for every air launch) and the
release path models no rail load, no separation transient and no seeker-cage disturbance. A g limit here
would be a number with nothing behind it — the class of thing this file's Gaps exist for. Booked as 2.13.

**When the shot breaks off: it does not have to, and that is the second finding.** An infrared round's
`FBSeekerHandoverS` is **0** — the shooter's obligation ends at the rail — so:

- the phase does not change. `BfmCommands` runs its next tick exactly as before the release: no
  `Support` state, no illumination, no uplink, nothing held. This is the *opposite* of the R-27R, whose
  `−1` makes `Support` and `Defend` mutually exclusive for the whole time of flight
  ([`weapons.md`](weapons.md) §Spec);
- the pilot does not look for his own round. He learns a shot happened the way he learns everything —
  off an instrument, `Stores.ReleasedCount`;
- the refusal path is the SMS's and is already complete: master arm, weight on wheels, no lock, no
  zone, empty rail, each a `RELEASE_REJECTED` with its reason. A refused shot is **not retried**, the
  same rule every other weapon action in this file has.

**One hand, one action.** The gun trigger and the missile release are both cockpit actions and both can
be satisfied in the same tick. **The gun has precedence**, for two reasons that are geometry and not
preference: its gate is the tighter one (the funnel is a fraction of the target's own span, §5.8) and
its window ends about where the missile's own Rmin begins (funnel max 914 m / 790 m against a merge Rmin
near 1,000 m), so "both in parameters" is a narrow band and the weapon already tracking owns it.

---

### 6. `pilot/FBBfmTrack` — the picture and the memory

#### 6.1 What it is built from — and what expressly NOT

**Only** from `FBState.Radar` (contacts + lock index) and its own `fb_fdm_state`. In the include tree of
this file there is **no `FBWorld`, no `FBUnitRegistry`, no datalink track** — that is the anti-cheat
property on which the whole BFM proof rests: the reported position is a computed estimate that can be
laid against a truth in the mission analysis which it has never seen.

**Header first**: an `Invalid` radar block is a device that is not looking, and its contact array means
nothing. `Update()` reads only the LOCKED contact — an unlocked search return is a detection, not a
target the pilot has committed to; mixing the two would let the pursuit jump between jets.

**One look, no re-read.** `lookS = nowS − c->LookAgeS`; folding happens only if
`lookS > LastLookS_ + kMinLookDtS` (0.05 s). A contact re-read between two looks carries THE SAME frozen
geometry — differentiating it would push a zero into the velocity filter.

**The echo location is WORLD-referenced** (bearing + elevation angle), not body az/el: the latter were
measured against the attitude AT THE TIME OF THE LOOK, and applied to the attitude NOW they would smear
the own motion of a rolling jet into the estimated target velocity.

**The alpha filter.** `Vel += kVelAlpha · (Δpos/Δt − Vel)` with `kVelAlpha` = 0.25. At a ~0.1 s STT frame
that is a time constant of ~0.4 s.
[MESS, against the truth reconstructed from both unit logs] successive looks with this filter hit the
DIRECTION of the estimated velocity to within 1.8° (median, p90 1.9°); a half-second baseline — five
times less differentiation error, half a second more lag — was 5.4° off.

**Extrapolation and freezing** (`Predict`, every tick, fresh look or not):

| Age | `Blk_.H.Status` | Position |
|---|---|---|
| ≤ `kMaxExtrapolateS` (8.0 s) | `Valid` | last look + `Vel·age` — young enough to LEAD on |
| > 8.0 s | `Held` | back to the last MEASURED position, frozen |

The stamp is always the LOOK, not `now` — age at the header is age since the sensor saw him. Justification
for the 8 s: a constant-velocity model against a turning fighter goes wrong quickly; at a ~5 °/s sustained
turn rate a straight-line prediction is already off by a large part of a turn diameter after eight
seconds, and then "where he WAS" is more honest than "where he would be if he had stopped manoeuvring".

**The closure rate** comes from the radar AS LONG AS it is looking; in coast from the estimate itself
(relative velocity onto the line of sight). The frozen last measurement would be worse than useless: a
merge ends with several hundred knots of closure in the record, and a pilot still reading that number
keeps flying an overshoot that ended long ago instead of turning back.

**Aspect** [HERL]: the angle AT THE TARGET between its tail and the line of sight to us. With **L** = the
unit vector own→target and **T** = its unit velocity, target→us points along −**L** and its tail along
−**T**, hence `cos(aspect) = (−T)·(−L) = T·L`. Undefined below `kMinTrackSpeedMs` (20 m/s) — then the last
value stays standing.

**Energy height** `Es = (h + v²/2g)` in ft: the only energy number a pilot can read off his OWN
instruments.

#### 6.2 `FBTrackDatum` — the memory, fully derived

The block answers "where is he" and honestly stops beyond the window. This freezing is right for the
PURSUIT — one does not pull lead on a guess — and useless for the SEARCH. Searching needs two numbers the
block does not carry: a POINT and the WIDTH of the area around it.

Both come from the same estimate, with ONE additional assumption: that the opponent turns about as hard
as oneself.

**The displacement** between a straight-line prediction and a turn at constant rate ω, after t seconds at
speed V:

```
d(t) = (V/ω) · √( (ωt − sin ωt)² + (1 − cos ωt)² )
```

Series expansion for the times that matter (ωt small): `ωt − sin ωt ≈ (ωt)³/6`, `1 − cos ωt ≈ (ωt)²/2` —
the second term dominates, hence

```
d(t) ≈ (V/ω)·(ωt)²/2 = 0.5 · V · ω · t²
```

**The hard bound:** he cannot be further than `V·t` from the position last actually seen, whatever he did.
Hence

```
RadiusM = min( 0.5·V·ω·t² , V·t )
```

**The crossing point** of the two halves [HERL]: `0.5·V·ω·t² = V·t ⟺ t = 2/ω`. BEFORE it the prediction
is worth more than the last look, AFTER it the turn could have gone anywhere and the honest centre point
stops moving — hence `tProp = min(AgeS, 2/ω)`. This limit is DERIVED, not chosen, and for the F-16 of
this simulator it lands at ~7.3 s — right where `kMaxExtrapolateS` had been set to 8 s INDEPENDENTLY. Two
different questions, one answer.

**ω = "he turns like me"** — `FBPilot::CornerTurnRateDegS()`:

```
ω = g·√(n² − 1) / V        (n, V = the airframe's OWN corner hooks)
F-16: n = 5.4, V = 380 kt = 195.5 m/s → 9.80665·√28.16/195.5 = 0.2663 rad/s = 15.3 °/s
```

[MESS] `make -C sim test-corner` measures 16.18 °/s directly on the same model — the derivation checks
out against the model to within 6 %. (Both numbers are post-`MODEL-DELTAS.md`-D1; before it they read
5.6 g → 15.8 °/s derived against 16.22 °/s measured.) The same method is used TWICE and is therefore a method and not two constants: it is
the fastest nose movement this jet can use to track a travelling gun solution, AND the assumption about
the other one.

**`HalfWidthDeg`** = `atan2(RadiusM, horizontal range)` — the radius seen as an angle from HERE, hence
exactly the half width a search has to cover. With that the scan pattern is a CONSEQUENCE of what the
pilot does not know, instead of a fixed weave.

`Datum()` is deliberately NOT derived from `Block()`: the block falls back to the measurement point
beyond the window and freezes (right for the pursuit, wrong for the search), so this method computes from
the same stored estimate with its OWN propagation rule. Const, allocation-free, once per decision tick.

#### 6.3 The `bfm_*` channels (source `bfm`, 15 columns, appended at the end)

| Column | Meaning |
|---|---|
| `bfm_pursuit` | `none`/`search`/`lead`/`pure`/`lag` |
| `bfm_valid` / `bfm_locked` | estimate young enough to lead on / radar is holding him NOW |
| `bfm_age` | s since the last real look (−1 if invalid) |
| `bfm_rng` / `bfm_ata` / `bfm_aspect` / `bfm_hca` / `bfm_clos` | nm / ° off nose (+ = right) / ° AT THE TARGET (0 = on his tail) / heading crossing angle / kt (+ = closing) |
| `bfm_es` | own energy height (ft) |
| `bfm_gcmd` / `bfm_ctrl` | commanded g / control position NOW |
| `bfm_engaged` / `bfm_lock_s` / `bfm_ctrl_s` | the three integrals (s) — lock retention ratio and time in the control position readable from the LAST line |

Every quantity is computable from OWN perspective. Everything that needs world truth (e.g. the TRUE
aspect) belongs in the analysis, not in the pilot.

---

### 7. Phase `Intercept` — flown with the SENSOR

The opposite pole to BFM: BFM is flown with the NOSE and the lock never goes away; an intercept is flown
with the SENSOR, and the whole art is when to point it at what. Guidance is `Direct` onto a point
`kInterceptAimM` (60 nm) along the desired course — far enough that the bearing to it is the desired
course to within fractions of a degree over a whole tick.

The order of a tick = the order of a pilot's attention: what do I see → who sees me → what state am I in
→ where do I point the jet → and only then which switch do I touch.

#### 7.1 The picture

- Locked → the contact at `LockIndex`. Otherwise the NEAREST return that has not identified itself as a
  friend: a valid Mode 4 reply PROVES friendly and takes it off the list, silence proves nothing and
  stays a candidate (`core/FBRadarContact.h` knows no value "hostile") — that IS the identification
  problem, not a shortcut around it.
- `haveTgt = tgt && LookAgeS < kInterceptLostS` (10.0 s) — two CRM frames plus margin: a missed sweep is
  a missed sweep, three are a target that is no longer there.
- `Bfm_.Update()` runs here TOO: the fusion supplies what a single echo cannot — the target velocity and
  from it the aspect on which the shot decision rests.

#### 7.2 Who sees me

From the RWR block: the strongest non-search warning, where a **missile** symbol ALWAYS beats a **track**
symbol (one is a radar that could shoot, the other a seeker that already has).

**The core rule — when a warning demands an answer:**

```
shotSelfSufficient = Eng_.HaveShot() && (Eng_.Pitbull() || !locked)
mustDefend = threatMissile || (threatTrack && (!weapons || shotSelfSufficient))
defendDue  = mustDefend && (now − IntDefendCueS_) ≥ Tuned(ReactionS, kInterceptReactionS)
```

A seeker on one's own aircraft is **never** negotiable. A merely TRACKING radar is: turning away from a
lock spike before one's own shot loses the engagement — the whole reason a fighter accepts being tracked
is that it is about to shoot back. A track spike therefore demands an answer only once one's own attack
has nothing left to gain: the shot is away and needs no more guidance, or there never was one.

`IntDefendCueS_` is the zero of the reaction time — the moment at which the warning DEMANDED an answer,
not the first symbol ever seen.

#### 7.3 The state machine (`FBEngageState`)

| State | What the pilot does | Left when |
|---|---|---|
| `Idle` | treated like `Search` | see below |
| `Search` | fly the briefed vector (active waypoint = bearing + altitude), select the search mode, antenna onto the expected altitude band, **do NOT lock** (a lock is a personal warning to the one it points at) | `haveTgt` → `Attack` if `R ≤ LockRangeNm`, otherwise `Closing` |
| `Closing` | pursuit course onto the contact, co-altitude, antenna centred on the return — still WITHOUT a lock | `R ≤ LockRangeNm` → `Attack`; contact gone → `Search`; no weapons → `Abort`; `R < AbortRangeNm` and never fired → `Abort` |
| `Attack` | designate (TMS forward) if not yet locked; read the launch envelope; shoot when `inParams` AND the lock is ≥ `kInterceptTrackSettleS` old AND the shot spacings are respected | shot registered (stores counter) → `Support`; otherwise as above |
| `Support` | **HOLD the lock** (the AIM-120 flies its midcourse on the uplink) and **CRANK** while doing so: turn away until the target sits at the edge of what the antenna can still carry | `now − ShotS ≥ holdS` OR (`!locked && !Pitbull`) → `weapons && haveTgt ? Attack : Abort` |
| `Defend` | **BEAM**: turn across the threat bearing (the shorter way), countermeasures every `ChaffIntervalS` | `now − IntThreatLastS_ ≥ DefendHoldS` → `CanPressOn(state) ? Search : Abort` |
| `Abort` | turn away cold: 180° away from the last thing that pointed at this aircraft | terminal |

`defendDue` beats everything else and sets `Defend` immediately.

**The shot gate:**

```
inParams = fc.DlzValid && fc.InZone
        && fc.TargetRangeM ≤ fc.RtrM · Tuned(ShotRtrFactor, InterceptShotRtrFactor())
        && |tgtAzDeg| ≤ Tuned(ShotAtaDeg, InterceptShotAtaDeg())
wantShot = locked && inParams
        && (now − IntLockSinceS_) ≥ kInterceptTrackSettleS
        && (now − IntLastShotS_) ≥ Tuned(ShotSpacingS, …) && now ≥ IntNextShotS_
```

The shot is taken at **Rtr**, not at Raero: Raero is the kinematic maximum range, hence a shot that the
opponent defeats by turning around and running away; Rtr is the range from which the round arrives even
when he does exactly that. `RtrFactor` is a FRACTION of a number that the fire control computes per shot
— not a range value of its own — so it stays correct when the geometry changes.

**`kInterceptTrackSettleS` = 2.0 s** [HERL/MESS]: a single target track is not a firing solution the
moment the antenna stops moving. The round is programmed with the estimated MOTION of the target
(`core/FBWeaponUplink`), and this estimate is differentiated from successive looks and filtered
(`kVelAlpha`, ~0.4 s time constant on a 0.1 s STT frame). Shooting within a second of designating means
launching a round with a velocity of nearly zero — [MESS] the aspect at which the shot was taken could
not even be computed. Two seconds are several filter time constants and at the same time what a real
firing sequence costs in switch work.

**After the shot:** `IntNextShotS_ = now + max(ShotSpacingS, fc.TimeToImpactS)` — a second round is only a
question at all once the first has had its chance. The pilot still learns nothing about the hit except
through his own sensors: a destroyed jet stops being a radar contact because it falls, not because
somebody tells him.

**The crank duration.** NOT "until the seeker takes over": that is the moment from which the round no
longer needs the UPLINK, not the end of the shot. Between seeker activation and impact this jet can do
nothing more for it and has every reason not to fly towards the target meanwhile:

```
holdS = min( kInterceptSupportMaxS(60 s) , max( Eng_.ShotTtiS() , max(Eng_.ShotTtaS(), 0) ) )
```

The cap catches a launch envelope that never produced a countdown (`TimeToImpactS < 0`: the round dies
before it arrives). The other end is failure: lock gone AND the seeker has NOT taken over — the round is
flying an estimate that nobody refreshes any more; going back is worth it for that, because re-designating
resumes the uplink.

**The crank side is fixed once per shot** (`IntCrankSign_`): a crank that chooses its direction anew every
tick is a jet flying an S while its round stays unsupported. The command is
`aimHdg = st.yaw + wrap180(tgtAz − sign·CrankAtaDeg)`.

**The beam.** 90° to the threat bearing (`InterceptBeamOffsetDeg`): there one's own velocity has no
component on his line of sight, which is exactly the clutter filter of a pulse-Doppler device — and it is
the ONLY geometry in which chaff is worth anything at all (`sensors/FBRadarSystem`'s notch). Of the two
ways the shorter one, because the turn itself is time in his favour.

#### 7.4 `CanPressOn` — the three instruments of resumption

```
weapons = Stores.H.Readable() && Stores.LoadedCount > 0
bingo   = Warnings.H.Readable() && (Warnings.Active & FBWarnBingo)
sensor  = Radar.H.Readable() && Radar.Radiating
return weapons && !bingo && sensor;
```

Three instruments, three reasons to fly home, each read off the BUS instead of known: a jet with empty
racks cannot kill him, one at BINGO will not get home afterwards, and one whose set is not radiating will
not even find him. The fuel judgement is carried by the WARN block, because BINGO is a number the PILOT
committed to (`systems/FBWarningSystem` against the briefed threshold) — not a fraction this class may
invent.

**The resumption searches the DATUM, not the briefed vector.** A fighter that broke off to defend and
survived does not resume an air controller's vectoring as if nothing had happened — he goes back to where
he last knew him. Heading, altitude band, antenna elevation AND weave width all come from
`FBBfmTrack::Datum`. If he has never seen anything, the datum is invalid and every number stays byte for
byte the briefed one — which is why an intercept without a single contact is untouched by all of this.
[MESS, `bvr-duel.fbm`] before, both continued to fly their vector after the defensive manoeuvre and
separated for **474 s** up to 70.7 km, each with a missile aboard. Afterwards both turn around (greatest
separation 55.6 km at t≈280 s), go back into `closing` at t≈355 s, into `attack` at t≈365 s, second shot
at t = 527 s (43.6 m miss distance, defeated in the notch like the first). Timeout therefore 320 → 700 s.

#### 7.4a The minimum-fuel decision — and the branch order that made it unreachable

**The finding this section answers** ([`campaigns/w3-desert-storm.md`](../mods/f16/mods/f16/doc/campaigns/w3-desert-storm.md)
§State, finding 1): `CanPressOn` above is the only line in the pilot that reads the BINGO warning, and
its branch could not be reached. On `w3-06-bingo.fbm` the bit stood for **5,200 of 5,200** telemetry
rows while `eng_state` was byte-identical to the control run with the bingo line deleted — seven of 184
columns moved and not one metre.

**Why it was unreachable, exactly.** The chain read

```
if (defendDue)                                     Defend
else if (EngState_ == Defend && now − IntThreatLastS_ ≥ DefendHoldS)   CanPressOn ? Search : Abort
else if (EngState_ == Support)                     …
else if (EngState_ != Abort)                       { !haveTgt → Search; !weapons → Abort; … }
```

`IntThreatLastS_` is refreshed on every tick `mustDefend` holds. At the first tick after the threat
symbol goes out, `defendDue` is false and the elapsed time is therefore **one tick**, so the second
branch is false and the fourth one fires and assigns the state away from `Defend`. `Defend` is never
seen again by the branch that would have timed it out. **Two defects in one line:** the fuel test was
dead code, and the defence hold — 12 s, and there for a stated reason — was dead with it, so the beam
was broken off at exactly the instant it was working.

**The fix is a guard, not a reversal.** `Defend` gets a branch of its own that owns the state while its
timer runs:

```
if (defendDue)                     Defend
else if (EngState_ == Defend)      { if (now − IntThreatLastS_ ≥ DefendHoldS) EngState_ = CanPressOn ? Search : Abort; }
else if (EngState_ == Support)     …
else if (EngState_ != Abort) {
    if (BINGO)                     Abort        ← NEW, and first in this chain
    else if (!haveTgt)             Search
    …
}
```

**Why the fuel line sits first in the general chain and nowhere higher.** Rank by what the statement is
*about*:

| Rank | Test | What it is a statement about |
|---|---|---|
| 1 | `defendDue` | somebody's round is in the air **at me**. Nobody stops defending because of fuel |
| 2 | `Support` with a live shot | **my** round still needs my uplink. Leaving throws away a shot already paid for; the state ends by itself and the fuel test then fires on the next tick |
| 3 | **BINGO → `Abort`** | the **aeroplane**. True whether or not anything is on the scope |
| 4 | `!haveTgt` / `!weapons` / range gates | the **picture** |

**Only the fuel instrument is lifted out of `CanPressOn`; the other two stay behind it.** `Radar
.Radiating` is a briefed TACTIC — lifting it would send every EMCON jet home the moment it went silent
(`w1-07-emcon`, `duel-emcon`, `w3-05-emcon`, `w4-05-emcon`). `Stores.LoadedCount` already has its own
branch with its own stated exception ("an abort for empty racks before anything was ever seen is simply
a jet that leaves"). Fuel has neither property: it is monotone, it is not a choice, and it is the one
of the three that does not depend on what the pilot is looking at.

**What the pilot DOES at minimum fuel is `Abort`** — turn cold, 180° from the last thing that pointed at
him, terminal. Not a route change: the flight plan is the mission's contract and the judge reads it, so
a pilot who deletes waypoints changes the verdict of a mission that never declared he might. A
fuel-driven RTB onto the landing waypoint is therefore named in §Gaps and not built.

**Measured** (`bingo-abort.fbm` against `bingo-press.fbm`, one declaration apart, identical fuel state):

| | `bingo-abort` | `bingo-press` |
|---|---|---|
| `brief_bingo_lbs` | 6000 | *(line absent)* |
| fuel at t = 0 | 4,183 lb | 4,183 lb |
| `warn_active` | **2** (`FBWarnBingo`) throughout | 0 throughout |
| `eng_state` | search → **abort at t = 4.1 s**, terminal | search → closing 52.0 → attack 140.1 → support 147.9 → attack 172.5 |
| heading | 90.0° → 230.4° (t = 60 s) → 263.5° | 90.0° → 112.1° |
| the log line | `intercept BINGO_ABORT fuelLbs=4174.3 bingoLbs=6000 from=search haveTgt=0` | — |
| result | TIMEOUT, no shot, 65 km **west** of the spawn | AIM-120 at 0.55 m, bandit destroyed t = 172.8 s |

#### 7.4b What the branch order moved, mission by mission

Snapshot of all 238 stock missions before and after (`tools/fb_regress.sh` ×2, `tools/fb_regress_diff.py`).
**196 missions are byte-identical.** 42 move, and each one is attributed to one of the two changes:

| Cause | Missions | How to tell |
|---|---|---|
| **the BINGO line** (new) | `w3-06-bingo` — and only this one | it is the only stock mission that both declares `brief_bingo_lbs` and runs `set task intercept`. `cmd-avionics.fbm` declares one too but flies `set task bfm`, so the intercept machine never runs and it is byte-identical; `w1-07-emcon` names the key in a comment only |
| **the `Defend` hold guard** | the other 41 | every one of them is a mission in which a jet enters `Defend` and the threat then stops. The jet now finishes its beam instead of snapping back into the intercept one tick later |

**Reports differently vs flies differently** — the split is whether any flight-state column
(`lat`/`lon`/`altM`/`hdgDeg`/`pitchDeg`/`rollDeg`/`casKt`/`mach`/`vsMs`/`aglM`) moved:

| Class | Count | Missions |
|---|---|---|
| **flies differently**, verdict unchanged | 39 | `bvr-defend`, `bvr-duel`, `bvr-duel-decided`, `duel-doctrine-f16`, `duel-emcon`, `duel-fulcrum-high`, `duel-headon`, `duel-viper-high`, `four-4v4-asym`, `o1-01-controlled`, `o1-02-no-gci`, `o1-03-gci-cut`, `o1-04-beam`, `o1-06-blind`, `o1-10-mole-cricket`, `o2-01-vector`, `o2-02-wrong-altitude`, `o2-09-exercise-one`, `o2-10-exercise-two`, `o3-09-two-fronts`, `o3-10-october-six`, `o4-02-bvr-offset`, `o4-03-energy-split`, `o4-10-two-v-two`, `o5-05-escorted`, `o5-08-night-one`, `o5-09-night-two`, `o5-10-batajnica`, `pair-2v2-f16`, `w1-02-two-v-one`, `w1-04-bvr-pair`, `w1-08-degraded`, `w1-09-lfe-four`, `w1-10-graduation`, `w3-06-bingo`, `w3-07-mig-cap`, `w3-09-saturation`, `w3-10-package-q`, `w4-07-cap-intercept` |
| **reports differently only** — the flown path is byte-identical, the commands and counters are not | 1 | `o1-07-early-launch` (`eng_state`, `eng_defend_s`, the chaff counters, `cmd_*`, `pitchCmd`/`rollCmd`; no state column moves before the run ends) |
| **the verdict moves** | 2 | below |

**The two verdict changes, each with its reason:**

| Mission | Before | After | Why, and whether it is right |
|---|---|---|---|
| `o3-07-top-cover` | exit **1**. The Israeli CAP's second AIM-120 pair kills the Egyptian striker `y7sb` at t = 512.6 s; `y7sb` FAILs ("survive objective lost") and its escort `y7cb` FAILs ("protected unit lost") | exit **3**. The run reaches its 600 s timeout with **all four Egyptians reporting SUCCESS** ("objectives met, survived") | **the chain is traceable tick by tick, and none of it is the fuel line.** `y7ba` holds the beam to the end of its 12 s hold instead of snapping back into the intercept at t = 189.3, which puts it somewhere else at t = 220.6 — and there an R-73 (7.4 kg) arrives at **2.67 m, 67,285 J/m²**, the FIRST arrival that mission's cover has ever scored (its own header records "ZERO ARRIVALS. All eight rounds expire or fall"). Its radar goes `degraded`, the Radar block stops being readable, and the resumption test — reachable for the first time — answers "no sensor" and sends it home at t = 225.9. The remaining CAP jet alone does not get the second pair away in time. The mission's ANSWER changes, and its header no longer describes it: booked in [`campaigns/o3-yom-kippur-1973.md`](../mods/f16/mods/f16/doc/campaigns/o3-yom-kippur-1973.md) |
| `w4-10-allied-force` | exit **3**, all units TIMEOUT at 700.0 s | exit **2**: `kamig4` departs (`monitor KO … stall/mush`) at t = **695.3 s**, 4.7 s before the timeout would have ended the run | the same displaced trajectory, landing on the known MiG-29 fragility (§Gaps 2.9): the jet arrives at t = 695 in a slightly different energy state and departs there. **4.7 s of 700**, and everything the capstone measures — the four strikers, the two Weasels, the four ground objects — is unchanged up to that tick. It is a worse ending on the same substance, and it is booked rather than smoothed over |

#### 7.5 The antenna control

- `Search`: `wantEl = atan2(bandAltM − st.elev, distM)·rad2deg − st.pitch`. One's **own pitch** is what
  makes this a command instead of a constant: the pattern is bolted to the nose, so a climbing jet looks
  out of the band it is supposed to search unless the antenna is pushed back by exactly this angle.
- `Closing`/`Attack`/`Support`: `wantEl = tgtElDeg − (pitch_now − pitch_at_the_last_look)` — **centre**
  the return, do not track it relatively. Both the reported contact elevation and the centre of the
  volume are body-referenced, so the desired antenna position IS the angle at which the contact came
  back.
  [MESS] ADDING to the current command made the beam wander away from the target look by look and lost a
  head-on contact twenty seconds after acquisition.
  **The pitch term is what makes that true while the contact COASTS.** The reported angle is
  body-referenced and was measured in the attitude of the LOOK; between two looks the only thing this
  pilot knows has changed is his own attitude, so he takes exactly that out and nothing else. On a
  fresh look (`LookAgeS == 0`) the term is identically zero, which is why nothing that never coasted
  moves by a tick. [MESS, `duel-fulcrum-high`] without it the N019's ±6° bar froze after ONE look while
  the jet descended 6,000 m and gave away 7° of pitch: the contact never came back, coasted its 6 s and
  fell — one track per approach and no shot in 600 s. Effect on the existing tree: three BVR missions
  gained one or two extra `RadarSlewEl` commands (`bvr-duel`, `bvr-duel-decided`, `mig29-intercept`).
  `cmd_*` counters move; in `bvr-duel` 29 of 7,000 ticks additionally carry one extra SEARCH-class RWR
  contact behind the jet, which `mustDefend` skips by construction. Every flight-state column, every
  shot and every verdict is unchanged.

#### 7.6a What he is looking at (`SelectCockpitPage`) — every phase, not just this one

It sits with the briefed entries at the head of `Run()`, under the identical gate ("not `Idle`, not on
the wheels"), and it is the only place in this class that posts `FBCommandTarget::MfdPageSelect`.

**The rank is this file's own rank, applied to attention** (§7.4a): survival first, then the aeroplane,
then the picture.

| Rank | Read from | Wants, in order |
|---|---|---|
| 1 | `Rwr.MissileLaunch` | `Rwr`, `Sys` |
| 2 | `Warnings.Active != 0` | `Sys`, `Fcr` |
| 3 | `Phase::Attack` | `Sms`, `Fcr`, `Sys` |
| 3 | `Phase::Bfm` / `Phase::Intercept` | `Fcr`, `Rwr`, `Sys` |
| 3 | everything else | `Hsd`, `Fcr`, `Sys` |

**Why a LIST per rank and not one page.** A cockpit that does not offer the wanted page — because the
airframe never had it, or because the loadout has just taken it away — would otherwise leave the pilot
staring at a dead display. The list is short, every entry is defensible, and `Sys` closes it because an
aeroplane always has an aeroplane.

**Why its own spacing timer** (`MfdLastActionS_`, same constant as `InterceptCockpit`'s): the rule
"one operating action per decision tick" holds for page selection against ITSELF, but a bezel button
must not delay a chaff throw or a shot by half a second. That the tactical channels did not move is a
measured claim, not an assumed one.

He posts nothing when the wanted ordinal is already on the attention bay, and nothing at all when
`FBMfdBlock` is unreadable — a module without a bank is untouched by this method.

#### 7.6 The hands (`InterceptCockpit`)

**At most ONE operating action per `Tuned(ActionSpacingS, kInterceptActionS)`** (default 0.5 s), in a
fixed priority order:

1. **Chaff** (`CmDispense`, value 0 = the program selected by the PRGM knob) — the aircraft being shot at
   does not edit a radar mode.
2. **Shot** (`WeaponRelease`).
3. **Designation** (`Designate`, value = track number; 0 = break) — only if the device is not already
   holding the demanded lock: a designation is a decision, not a repeated demand.
4. **Antenna elevation** (`RadarSlewEl`), with a dead band `kInterceptElDeadDeg` = 2.0° — a knob that is
   tapped every tenth of a second is not being flown; a search pattern is ±10.5° high
   (`modules/f16/FBF16Fcr`), so 2° lie well inside the beam.
5. **Search mode**, ONCE (`RadarMode`, `SearchRadarModeOrdinal()`). Ordinal < 0 = "this module has no
   non-locking search mode of its own, the device stays as the mission set it".

#### 7.6b The knob belongs to the SET, not to the picture — and asking the wrong one made EMCON one-way

**The defect.** Both mode branches of `InterceptCockpit` were guarded by `state.Radar.H.Readable()`.
That head is the PICTURE's, and `FBRadarSystem::Run` invalidates it the moment nothing radiates
(*"a set that is not radiating has no picture — not an empty one. Invalid, so that 'found nothing'
cannot be confused with 'did not look'"*). So the ONE state the branch exists to leave — silent —
was also the state in which the branch could not run. Going quiet was a one-way door, and the door was
not the EMCON rule but the hand that reaches for the knob.

**How the mechanism was separated from the one the finding named.** `X-6` read the latch as the gate
expression `EmconSilent_ = other && nearestM > radiateM` on a datalink report that nothing could
refresh. A probe on that expression, logged every decision tick, says otherwise
[MESS, `sat-02-picture-split`, `--threads 1 --elev const`, all four sweep members]:

| Quantity, per F-16 | Value |
|---|---|
| ticks with `EmconSilent_` **true** | **10 of 5,200 (0.19 %)** — t = 57.0 … 57.9 s and nowhere else |
| ticks with the radar **off** (`fcr_on = 0`) | **4,626 of 5,200 (89.0 %)** — t = 57.5 s to the end |
| the mate's report the gate stood on (`Engaging`) | present t = 57.0…57.9, **gone at t = 58.0**, one net cycle later |

The gate opened again 0.9 s after it closed, exactly as written, and the radar stayed off for the next
462 s. **The report ages correctly and needs no age threshold of its own**; the reported point never
became the frozen thing the finding describes, because it was read exactly ten times.

**The second half of the finding dissolves the same way.** `flt_src = 0` for the same 462 s is not a
contradiction with the emission gate: `FBFlightPicture::Assign` returns `None` whenever
`!Radar.H.Readable() || ContactCount == 0`, because a sort correlates a mate's reported POINT against
THIS jet's own echoes and a silent jet has none. The two layers read the same `state.Datalink` and agree
completely; they answer different questions — "does anybody hold a picture" and "can I match his point
to one of my returns".

**The repair, and the invariant it encodes.** *Every emission state the pilot ENTERS, he must be able to
LEAVE.* Two things stood in the way of that and both are in this one branch:

1. **The knob belongs to the set, not to the picture.** To move an FCR mode a pilot needs: that the
   airframe has such a mode (the module's ordinal hooks — already asked), that there is a set to move
   (new: `Radar.Powered`, [`core.md`](core.md) §1.1a), and where the knob stands now
   (`Radar.ModeOrdinal`, written on every `Run()` before the picture is decided). None of the three is a
   picture, so none may be gated on one.
2. **He takes back his OWN silence and never the brief's** (`IntEmconSilenced_`). A `set fcr_mode off`
   is a decision above him — the same ranking that puts `HaveOrderEmcon_` above these lines twenty
   lines further down. Without it the branch's `else` arm ("not silent ⇒ be in the search mode") would
   power up every briefed-quiet set in the first tick of an intercept.

**The one-shot search-mode assertion below it keeps `H.Readable()`, and that is point 2 from the other
side:** it corrects a RADIATING mode that is not the search mode, and a briefed-quiet set stays as the
mission set it. `BfmSelectRadarMode` keeps it for the same reason — it selects a scan volume on a
producing set, it never enters an emission state, so it has nothing to undo.

**Measured cost of getting point 2 wrong**, and it is why it is stated as an invariant rather than as a
guard swap: the first cut applied point 1 to both branches and moved four missions whose whole premise
is a briefed-off radar — `bvr-defend`, `bvr-defend-blind` (*"the DEFENDER is `set task intercept` with
NO weapons and its RADAR OFF"*), `damage-amraam` (*"it never looks and never warns anyone"*) and
`o5-04-no-radar`. With the invariant they are byte-identical again and `sat-02`'s numbers below do not
move by one bit.

**The pilot does not see more than before.** `Powered` and `ModeOrdinal` are his own aircraft's switch
state, published by his own radar system into his own bus. No registry reader was added
(`make -C sim verify-layers`: **6**, unchanged), no contact gained an identity.

**Measured, `sat-02-picture-split`, before → after** (ground truth, not the pilot's own opinion of it):

| | before | after |
|---|---|---|
| `fcr_on`, `pb1` | 574 / 5,200 = **11.0 %** | 2,170 / 2,486 = **87.3 %** |
| `fcr_lock` ticks, `pb1` | **0** | **18** |
| blue `sms LAUNCH_SOLUTION` lines | **0** of 18 | **4** of 12 |
| `eng_shots` per sweep member | 0 / 0 / 0 / 0 | 1 / 1 / 1 / 1 |
| named `kill unit <MiG>` bits met | **0 of 8** | **2 of 8** (`pb3`→`pmia2`, `pb4`→`pmib2`) |
| radar on/off spells, `pb1` | 1 (on 57.5 s, then off forever) | 4 — on 57.4 s, **off 1.0 s**, on 7.7 s, **off 30.6 s**, on to the end |

The last row is the one that says the doctrine now RUNS rather than merely stops latching: the jet is
quiet while a mate's reported point sits beyond its own radiate gate and comes back up when it does not.
The 2-of-8 result is the same outcome key the two independent genes that bypass the latch reached
(`pilot_emcon_frac ≥ 1.35`, `dl=off` — both **2 of 8**, `doctrine-evolution.md` `X-6`), reached here from
the structure instead of from a tuning value. **It is corroboration, not the argument** (principle 1).

**The run now ENDS EARLY, and that is a cost this rig pays.** The baseline flew the full 520 s, which
`sat-02`'s own header cites as the reason no verdict there is a function of the truncation instant. With
the radar working, a MiG-29 flies into the ground at t = 248.6 s (`monitor KO … CFIT`) and the run stops
there. The rig keeps its S7 chaos immunity claim only for the baseline it was measured on;
re-measuring it is `doctrine-evolution.md`'s work, not this file's.

#### 7.6c What the guard moved, mission by mission

Two `tools/fb_regress.sh` snapshots over all **287** stock missions, `--threads 1`, one binary each.

| | |
|---|---|
| byte-identical | **246** |
| moved (telemetry and `events.log` both) | **41** |
| exit code changed | **2** |

**The 41 are one class and the membership test is exact.** A mission moves **iff** it contains an F-16
running `set task intercept` whose radar is not briefed off, together with at least a second datalink
terminal — which is precisely the condition under which `EmconSilent_` can ever become true. There are
**72** such missions; the other 31 never trip the geometry (nobody's reported point lies beyond the
radiate gate while a mate is engaging), and **0** missions outside the class move.

**Every one of the 41 gains `RadarMode` traffic**, which is the mechanism read directly off the log: in
the baseline the count is exactly two log lines per silencing jet (the ISSUE and the ACK of the one-way
`Off`); afterwards it is the round trips. Examples: `o1-01-controlled` 4 → 36, `sat-02-picture-split`
8 → 28, `w1-04-bvr-pair` 4 → 32, `ar-22-headon-three-two` 16 → 87.

**The two exit codes, individually.** Both files' own binding reading rules state that the exit code is
not their verdict, and both moves are a Red aircraft dying:

| Mission | Exit | Why |
|---|---|---|
| `o3-10-october-six` | 3 → 2 | `yxh` (MiG-29, hostile) takes a hit at t = 385.9 (`damage KILL … combat ineffective`, RWR + CMS + engine 2 failed), flies on wrecked for 16.5 s and reaches the ground at t = 402.4 — `monitor KO … ATTITUDE_CONTACT`. The file's own header names this shape in advance: *"STANDALONE the same file gives exit 2 = CRASH, because the aircraft that falls there reaches the ground first."* Blue launches 15 → 14; the difference is that one of them arrives |
| `ar-01-headon-noon` | 3 → 1 | `ar01hi4` (MiG-29, hostile) is shot down at t = 417.3 and loses its `survive` objective, so a `FAIL` now exists in a run that previously had none. Blue launches 21 → 25, Red aircraft killed 0 → 1. The rung's `(V, M)` — its own binding measurement — is **Blue (20, 12) unchanged, Red (34, 8) → (33, 7)** |

**The three saturation rigs, on their own `(V, M)` instrument:** `sat-02` **(14, 8) → (14, 10)**,
`sat-01` **(32, 23) → (30, 23)**, `sat-03` **(24, 20) → (26, 21)**. `sat-02`'s pair is exactly the one
`X-6` measured for both of its bypass genes.

#### 7.6d The emission is an AXIS, not a mode — and the picture that permits the silence need not be a block

**Two things in §7.6a/b were written in the F-16's shape and read as the rule.** Round `E20` found both
by measuring the MiG-29 side at the bit level; neither is a bug in the F-16's path, which is
byte-identical across this change — 293-mission regression, **237 byte-identical, and every one of the
155 missions that contain no MiG-29 at all is among them**.

**(1) The switch.** The EMCON actuation posted `FBCommandTarget::RadarMode`. On an aircraft whose mode
selector ALSO carries the search/close-combat decision (`SearchRadarModeOrdinal` against
`BfmRadarModeOrdinal`) that is not merely inelegant — quietening the mode and coming back through
`SearchRadarModeOrdinal` would overwrite the close-combat pattern every time the jet left a merge. The
MiG-29 has a documented switch for exactly this (`PUR-31 ILLUM / DUMMY / OFF`, `DCS-EA p.63`), so the
hook is now the SWITCH rather than an ordinal:

```cpp
struct FBEmissionControl { FBCommandTarget Target; int Silent; int Radiate; };
virtual FBEmissionControl EmissionControl() const {                       // the default IS the old code
  return {FBCommandTarget::RadarMode, SilentRadarModeOrdinal(), SearchRadarModeOrdinal()};
}
```

The readback follows the target — `FBRadarBlock` gained `EmissionOrdinal` beside `ModeOrdinal`, written
in every `Run()` before any picture is decided, which is §7.6b's own invariant applied to the second
axis. **DUMMY and not OFF is this jet's silent state, and that is structural rather than a preference:**
`OFF` drops the set's power, and `Powered == false` is precisely the state §7.6b proves a pilot cannot
come back through. It is also the START state the mission briefs, i.e. a decision above him.

**(2) The picture.** `EmconSilent_ = other && nearestM > radiateM` computed `other` from
`FBDatalinkBlock` + `FBNetLinkBlock` alone. Read as the rule that says *whoever is quiet without a
picture is quiet blind*, that is right; read as code it says **an airframe with no cooperative terminal
may never be quiet** — which turns the one aircraft in this tree whose entire doctrine is emission
discipline into an F-16 that lost its datalink. One virtual, defaulting to "no such picture":

```cpp
virtual bool BriefedPictureRangeM(double &rangeM) const { (void)rangeM; return false; }
```

`FBMig29Pilot` answers it with the **controller's last call** — a range it was told and typed
(`doc/modules/mig29/datalink-gci.md` §2.2), carrying no identity, no type and no track file. It expires
at the controller's own CADENCE (the briefed gap to the previous call), which is the mission's number
and not a new one; past it the pilot has no outside picture and the rule forbids the silence, so a jet
whose controller falls silent radiates rather than going dark for the rest of the run.

**Measured, `sat-07-dry-merge`, MiG `ma1`** — the doctrine now runs on the airframe that owns it:
`gci BRAA` 120 km → quiet · picture expires at t = 40 → **illum 43.9** · 90 km → **dummy 63.9** ·
60 km → **dummy 112.4** · last call expires at t = 140 → **illum 144.1**. Three ILLUM and four DUMMY
spells per jet, every entered state left.

**And the conservation proof is the gene's own rail:** at `pilot_emcon_frac = 2.5` and `3.0` — where
`f × 27.0 nm` exceeds every briefed call range, i.e. "never be quiet" — the repaired MiG-29's telemetry
is **byte-identical to the pre-repair binary's** on all three cells (`sat-02` `0d8d78e666d7a8a9`,
`sat-04` `010f15d3c0b73011`, `sat-07` `5cd231b5129bce09`). The capability is added; nothing is removed,
and the old behaviour is one brief away.

---

### 8. `pilot/FBEngagement` — the state machine as data and the debriefing

**Scoreboard, not brain.** `FBBfmTrack` carries both roles (picture + metric), because a tracker has to be
ONE object; here the picture is already on the bus, and what is missing is a place to REMEMBER what
happened. `FBPilot::Run` decides, this class records.

**Events, each exactly once (first occurrence wins):**

| Method | What it records |
|---|---|
| `NoteContact(now)` | first firm radar contact on the target being worked |
| `NoteLock(now)` | first single target track |
| `NoteShot(now, R, ata, aspect, raero, rtr, rmin, tta, tti)` | the trigger WITH the whole launch envelope as the fire control reported it AT THAT INSTANT — the prediction against which the flown result is later measured. ONLY the first shot describes the metric (a second one is a different decision with its own geometry; averaging would describe neither), the counter says how many fell |
| `NoteThreat(now)` | first track-class warning |
| `NoteDefensiveAction(now, cueS)` | first command as an ANSWER, measured from the CUE |
| `NoteChaff(n)` | CARTRIDGES actually ejected (the set's count, not that of the switch throws) |
| `NoteSupport(locked, now, dt)` | one tick in the guidance window `[start, start + TTA]` |

**Guidance is MEASURED IN SECONDS IN WHICH THE UPLINK WAS ACTUALLY FED**, not in seconds since launch:
whoever keeps the nose in but loses the track through the gimbal limit is no longer supporting the shot.
`Pitbull` is the derived verdict, passed exactly once at the END of the window. The window closes BEFORE
this tick's count, so that the sum never exceeds the window length — `eng_support_f` is a FRACTION and is
additionally capped at 1 (0.1 s tick against a window quantised onto the integration step).

**The `eng_*` channels (source `eng`, 27 columns, appended right at the end).** Each is (a) computable
from one's OWN instruments and (b) a quantity a real debriefing would argue about:

| Column(s) | Measures |
|---|---|
| `eng_state` | the state from § 7.3 |
| `eng_tgt_nm`, `eng_ata`, `eng_aspect`, `eng_clos`, `eng_locked` | the current geometry of the contact being worked (−1 = none) |
| `eng_detect_s`, `eng_lock_s` | **time to acquisition** — whoever puts the antenna at the wrong altitude finds him late or never |
| `eng_shot_s`, `eng_shot_nm`, `eng_shot_ata`, `eng_shot_aspect` | shot moment, range, geometry |
| `eng_shot_rtr_nm`, `eng_shot_raero_nm`, `eng_shot_rmin_nm` | the launch envelope AT THE MOMENT of the shot — a shot is only as good as the geometry in which it fell |
| `eng_tta_s`, `eng_tti_s` | the two predictions of the fire control computer (to self-guidance / to impact) |
| `eng_support_s`, `eng_support_f`, `eng_pitbull` | **the difference between a launch and a kill** |
| `eng_threat_s`, `eng_react_s` | **reaction time** — the one channel that describes purely the PILOT and not the geometry |
| `eng_defend_s`, `eng_shots`, `eng_chaff` | seconds in defence, shots, cartridges ejected |
| `eng_es`, `eng_es_min` | energy height now and its minimum SINCE THE START OF THE ENGAGEMENT (not since the start of the run: whoever is not fighting yet has not spent anything yet) |

All of them survive the engagement, which is why the last line of a run IS the whole debriefing.
**That is the significance for the evolutionary round:** because every channel is computable from one's
own perspective, the fitness does not score knowledge the pilot did not have — and because they are in
the telemetry and not recomputed in the analysis, the analysis does not judge on its own copy of the
geometry.

---

### 9. `pilot/FBPilotTuning` — the variant as mission data

**The table.** A fixed array `{Have_[], Value_[]}` over the parameter ordinal: no allocation, no map
lookup in the decision path. Read exclusively through

```
double Tuned(FBPilotParam p, double own) const { return Tune_.Or(p, own); }
```

`own` is ALWAYS this pilot's own hook — the airframe's numbers thereby stay in the airframe's class, and
the override stays thin and visible at the point of use.

**"Not set is not zero."** An empty entry means "this pilot's own number". A mission without a `pilot_*`
line therefore flies byte-identically to one that never saw this class (measured).

**Band check.** `Set(key, value)` rejects an unknown key or a value outside the band;
`FBF16Module::ApplySetup` turns that into a mission FAIL. A mistyped tournament number therefore does not
silently fly the default. The bands are deliberately WIDE — they catch a typo or a unit confusion, they
do not encode taste: a tournament may try a bad idea, it may not try 6,000 nm.

| Key | Band | Decides | Note |
|---|---|---|---|
| `pilot_speed_kt` | 150…900 | intercept speed (kt TAS) | |
| `pilot_lock_nm` | 1…100 | where the lock (and its warning) is issued | upper limit [SET]: no fire control in this tree gates further, and the SET's own gate rejects a longer lock anyway — the rail does not borrow one aircraft's number |
| `pilot_shot_rtr` | 0.1…3.0 | trigger at this multiple of Rtr | > 1 = beyond Rtr |
| `pilot_shot_ata_deg` | 1…60 | how far off nose a shot is still taken | upper limit = gimbal limit |
| `pilot_shot_spacing_s` | 0…120 | spacing of two shots at the same target | |
| `pilot_crank_deg` | 0…60 | how far away the supported shot is cranked | |
| `pilot_abort_nm` | 0…40 | below this the intercept is over | |
| `pilot_beam_deg` | 0…180 | defensive turn against the threat bearing | |
| `pilot_chaff_s` | 0.2…60 | dispensing interval in the defence | |
| `pilot_defend_hold_s` | 0…120 | hold time after the last warning | |
| `pilot_react_s` | 0…30 | human reaction time | **PILOT property** |
| `pilot_action_s` | 0.1…30 | one operating action per this time | **PILOT property** |
| `pilot_gun_burst_s` | 0.1…1.0 | length of ONE trigger squeeze | upper limit = `core/FBGun.h`'s `MaxBurstS`; the duration is NOT the bus spacing |
| `pilot_gun_tol_frac` | 0.05…1.0 | how tightly the pipper is held (fraction of the funnel tolerance) | |
| `pilot_bfm_ctrl_min_nm` | 0.05…5.0 | near edge of the control position | |
| `pilot_bfm_ctrl_max_nm` | 0.05…10.0 | far edge | gun position IN the funnel, missile position outside |
| `pilot_attack_bias_s` | −10…+10 | release s seconds after the cue | deliberately WIDE and SIGNED: the parameter for a DELIBERATELY wrong release |
| `pilot_attack_ccip_m` | 1…2000 | CCIP pipper tolerance | |
| `pilot_energy_frac` | 0.7…1.2 | **`Scale`** — the speed the BFM throttle insists on, as a multiple of this jet's own `BfmCornerSpeedKt` | G4, [`doctrine-evolution.md`](doctrine-evolution.md) §2.1 |
| `pilot_cover_frac` | 0…3.0 | **`Scale`** — how many of its OWN weapon's binding times a member holds its trigger for under the cover rule. 0 = rule off | G2, ditto |

**Two entry KINDS since round `E1`, and the difference is a matter of syntax rather than of discipline**
([`doctrine-evolution.md`](doctrine-evolution.md) §2.2):

| Kind | Declared | Read through | It cannot |
|---|---|---|---|
| `Free` | `{key, param, Free, lo, hi, ""}` — a pure pilot decision | `Tuned(p, own)`, unchanged | — |
| `Scale` | `{key, param, Scale, lo, hi, "<hook>"}` — a multiple of a NAMED airframe hook | `TunedScale(p, own)` = `own · Or(p, 1.0)` | **express an absolute.** The value never leaves the class unmultiplied, and two `static_assert`s refuse a `Scale` band that is not dimensionless (`0 ≤ lo < hi ≤ 3.0`) or that names no hook |

The runtime half is measured as an exit code, not read off the source:
`missions/genome-absolute-refused.fbm` writes the aircraft's OWN corner speed (380, the most plausible
mistake there is) into `pilot_energy_frac` and **exits 1** with `SET_INVALID_VALUE` + `SET_REJECTED`
before the run starts; `missions/genome-scale-flown.fbm` is the same file at 0.85 and flies, moving the
throttle on [MESS] **2,979 of 3,001** ticks with a maximum |Δthrottle| of **0.680**.

The whole table is readable from outside the binary: `fb-gym --pilot-keys` prints
`<key> free|scale <lo> <hi> <hook>` per line, which is where `tools/fb_evolve.py` gets its genome —
so no tool carries a second copy of the alphabet.

**Why a population is a set of TEXT LINES.** A variant is thereby a LINE in a mission file instead of a
class — nothing is compiled between two candidates, and there is no tournament code in the simulator.
Reaction and action times still lie ON TOP of the bus latency of the respective class: no variant can
answer faster than the jet allows.

#### 9.1 The tournament runner (`sim/tools/fb_tournament.py`)

Stdlib Python, not a build target, no dependency under `sim/build` except the `fb-gym` binary.

- **What it flies:** every unordered pair of variants, in BOTH side assignments (A west/B east and B
  west/A east), on one starting geometry. Flying both seats removes the positional advantage from the
  result — the two runs of a pair are mirror images, so the SUM measures the variant and not the seat.
  Runs go through `fb-gym --threads N` and are byte-reproducible (`--check-determinism` flies every
  pairing additionally with `--threads 1` and compares the telemetry byte by byte).
- **Airframes:** a variant line may carry `module=<name>` (default `f16`), which selects the arena
  block for that seat — so a tournament can be MIXED, an F-16 doctrine against a MiG-29 doctrine
  (`tools/variants-mixed.txt`, [`duels.md`](duels.md)). Only the box-specific `set` lines differ
  between the two blocks; the spawn, the master-arm call, `set task intercept`, the vector, the
  objectives and the `pilot_*` lines are shared text. An f16-only variant file generates byte-identical
  missions to the version before the key existed (verified over all 60 pairings of both geometries).
- **Geometries:** `mirror` (head-on, co-altitude, co-speed, both outside the 40 nm search gate at t=0 —
  both runs begin with a real search phase, neither gets a detection for free) and `split` (the energy
  difference of `bvr-duel-decided.fbm`: 6,000 m and 150 kt — not "who wins an equal fight" but "who makes
  more of both ends of an unequal one").
- **Evaluation** exclusively from `telemetry*.csv` (the `eng_*` and `dmg_*` columns of the last line)
  plus the `UNIT_RESULT` lines from `events.log`.

**The evaluation rule since round `E1`: the order is LEXICOGRAPHIC, not weighted.** The fitness moved
out of this tool into `tools/fb_fitness.py`, which the tournament, the arena gate and the evolution
runner all import — one scorer, no copies. Derivation, evidence and the two items that were REMOVED
rather than re-tuned: [`doctrine-evolution.md`](doctrine-evolution.md) §1.

```
key(unit, run) = ( V , M , C )        compared LEFT TO RIGHT; larger is better

V ∈ {0..3}   the judge's verdict, read from UNIT_RESULT and never recomputed
             3 SUCCESS · 2 TIMEOUT/NONE · 1 FAIL · 0 CRASH/LOC
M ∈ {0..k}   how many of the objectives this unit DECLARED the judge marked `met`, counted off the
             `mission OBJECTIVE` lines FBMissionMonitor publishes at every conclusion
C            craft — bounded, and consulted ONLY when V and M are exactly equal. Since `E6` it is a
             PAIR, `(air, aim)`, compared by DOMINATION and never added: better in one and not worse in
             the other wins, better in one and worse in the other is incomparable and ties
```

| Item | Weight | Note |
|---|---|---|
| shot geometry | ×100 | `q = zone · cos(ata)`, plateaued at Rtr — unchanged |
| support | ×80 | `eng_support_f`, a fraction, so unfarmable |
| shot lead | ×40 | `tanh((t_foe − t_me)/15)`, relative to the opponent in the same run |
| defence | ×40 | only where a warning existed and was survived |
| energy | ×40 | `eng_es_min / es_start` |
| rounds | −25 each, floored at −150 | bounded by the loadout, not by the run length |
| **hits landed** | **REMOVED** | it paid per event on a count the simulator partitions into TICKS — measured at 900–1,278 fitness points per second of trigger |
| **no shot** | **REMOVED, replaced by a GATE** | a price can be bought back; `C = −∞` for a unit that neither fired, locked nor delivered cannot be |
| **aim** (`E6`, the ground currency) | ×100, in a component of its OWN | `mean over the unit's deliveries of 1/(1 + aimErrM/10 m)`, off the judge's `stores DELIVERY` line. A MEAN and not a sum, so a fourth bomb cannot buy what the first three missed; in a second component, so no metre of aim error is ever priced in shot-geometry points |

**Aggregation is pairwise domination, not a mean.** Per pairing the two sides' keys are compared
directly and the variant takes 1 / ½ / 0; the fitness is `Σ points / (2 · N_opponents) ∈ [0,1]`, a
normalised win rate. A mean of ranks is a number in a currency that has no units, and it would make M
and C dead code because two float means are never equal.

**The tool now says which level decided.** Every run is counted, and a field in which no run was
decided at V or M prints `SATURATED` and the instruction not to read the ranking — a rank change inside
level C is expressly not a finding ([`doctrine-evolution.md`](doctrine-evolution.md) §6).

### 10. The mission control loop as a way of working

```
define a mission (.fbm)  →  simulate headless (fb-gym)  →  analyse the telemetry mechanically
      ↑                                                                       │
      └───────────────────────────── correction ←────────────────────────────┘
```

- **Define:** `mods/f16/src/missions/*.fbm`. For a fight, `set datalink off` is mandatory (otherwise the sensor
  restriction would only be claimed) plus an auto-lock mode (`set fcr_mode acm_hud` or similar — CRM does
  not lock by itself). For an intercept, `set fcr_mode crm` and starting geometries outside the search
  gate.
- **Simulate:** `build/fb-gym --mission FILE --out DIR [--threads N] [--elev const|swiss]`.
  Combat and intercept missions end by **TIMEOUT (exit 3)** — an engagement has no waypoint objective; the
  verdict is in the telemetry, not in the exit code. Since the combat objectives (`objective kill unit
  …`) a duel can deliver SUCCESS/FAIL instead.
- **Analyse:** the LAST LINE carries all the integrals. Channels by question:

| Question | Channels / events |
|---|---|
| Where was the pilot? | `phase`, `activeWp`, `distToWpM` |
| How did the turning fight go? | `bfm_*` (§ 6.3), `pilot GUN_TRACK`/`GUN_BREAK` |
| How did the intercept go? | `eng_*` (§ 8) |
| What did he operate? | `cmd_*` columns, `cmd CMD_ISSUE`/`CMD_ACK`/`CMD_REJECT` |
| What did he see? | `blk_*` (block validity — a held value otherwise looks like a fresh one), `warn_*` |
| What did he hit? | `dmg_hits`/`dmg_failed`/`dmg_degraded`/`dmg_effective`, `damage DAMAGE`/`SYSTEM`/`KILL`, `gun HIT`/`MISS`/`DRY` |
| What did he drop? | `pilot ATTACK_RELEASE`, `stores DELIVERY` |
| How did it end? | `UNIT_RESULT unit=… result=… reason="…" decisive=…`, `RESULT`, `SUMMARY` |

- **Rule for new channels:** ALWAYS append at the end (`units/FBSimUnit::StartTelemetry`), so that no
  column ever measured loses its position. `FBBfmTrack` and `FBEngagement` are separate telemetry sources
  for exactly this reason, and not folded into the three `pilot` columns that sit in the middle of every
  existing `telemetry.csv`.

---

### 11. Pilot property vs. aircraft property

The separation is structural: aircraft numbers are **virtual hooks** on `FBPilot` which the module
overrides; pilot numbers are **constants in `FBPilot.cpp`**, because a human is the same in every cockpit
— and both are overridable from the mission side through `FBPilotTuning` where a variant makes sense.

**PILOT properties** (constants, not hooks):

| Constant | Value | Justification |
|---|---|---|
| `kInterceptReactionS` | 1.0 s | the one number in this file that models a HUMAN: perceive, recognise, decide, move. Published values for a trained pilot on an unambiguous, expected stimulus are 0.5–1.5 s; 1.0 s is the middle. It sits ON TOP of the 0.5 s HOTAS latency — so the earliest a defensive command can act is one and a half seconds after the light comes on. That is exactly what it exists for: an AI answering in the same tick wins with reflexes it does not have |
| `kInterceptActionS` | 0.5 s | the same for the HANDS: a pilot operates one lever after the other, and the avionics itself needs 0.5 s to distinguish two actions on one switch |
| `kInterceptElDeadDeg` | 2.0° | dead band of the antenna elevation, within the beam width |
| `kInterceptLostS` | 10.0 s | two CRM frames plus margin |
| `kInterceptTrackSettleS` | 2.0 s | several filter time constants + the switch work of a real firing sequence |
| `kBriefRetryS` | 2.0 s | a rejected DED entry is a hand and a head, not a loop |
| `kBfmTurnTimeS`, `kBfmRollFullDeg`, `kBfmRollRateMaxDegS`, the PI gains | see § 5 | control law parameters, partly derived, partly measured |

**AIRCRAFT properties** (virtual hooks; F-16 values from `modules/f16/FBF16Pilot.h`):

| Hook | Generic default | F-16 | Source |
|---|---|---|---|
| `RotationSpeedKt(w)` | 65 | table 128…198 KIAS over 20,000…44,000 lb, interpolated | [DOK] `procedures-takeoff-taxi.md` |
| `RotationLeadKt` / `RotationPitchDeg` | 10 / 8° | 15 kt / 10° | [DOK] "~15 kt below Vr in AB", "8–12° rotation attitude" |
| `GearUpLimitKt` / `ClimbSpeedKt` / `TakeoffThrottleNorm` | 150 / 100 / 1.0 | 300 / 350 / 1.0 | [DOK] / mission profile / [DOK] "full afterburner" |
| `ApproachSpeedKt` | 90 | **154** | [MESS] trimmed level, gear down, ~40 % fuel: 11.0° AoA at 154 KCAS — an open-loop substitute for a closed AoA loop, faithful to the model's trim curve instead of to a copied number. Was 165 while `MODEL-DELTAS.md` D1's flaperon mixer kept the trailing edge flaps from lifting |
| `GlidepathAngleDeg` | 3° | 3° | [DOK] `navigation-ils.md` |
| `FlareTargetPitchDeg` / `AerobrakePitchDeg` / `AerobrakeSpeedKt` | 8 / 10 / 100 | 12.5 / 12.0 / 100 | [DOK] short final/roll-out (≤13° AoA, hold ~13° until ~100 kt) with margin against the flight monitor's 15° attitude knockout |
| `BfmCornerSpeedKt` / `BfmCornerG` | 300 / 4.0 | **380 / 5.4** | [MESS] `make -C sim test-corner`, re-run after `MODEL-DELTAS.md` D1: 280→13.7 °/s @3.5 g \| 340→15.2 @4.6 \| **380→16.2 @5.4 (corner)** \| 400→16.4 @5.8 (peak) \| 420→15.0 @5.7 \| 500→11.5 @5.4 \| 620→12.8 @7.3. The corner SPEED did not move; the g at it did |
| `BfmMinSpeedKt` | 220 | 300 | [MESS] there the rate is ~13 % below the peak |
| `BfmMaxG` / `BfmUnloadG` | 6.0 / 3.0 | 9.0 / 3.0 | structural limit / [SETZ] |
| `BfmControlMinNm`…`BfmControlAtaDeg` | 0.5 / 1.5 / 30° / 30° | identical | [SETZ] |
| `BfmClosureGainKtPerNm` / `BfmMaxClosureKt` | 120 / 200 | identical | [SETZ] |
| `BfmBrakeMs2` | 1.5 | **1.2** | [MESS] the decay of the CLOSURE under idle + full speedbrake in the stern conversion, N=4,595 one-second windows: median 1.86 / p20 1.16 / p90 5.76 m/s². The p20, because this number is a LIMIT (§ 5.2). Replaces the level-flight 2.4, which measured the airframe rather than the quantity the schedule bounds |
| `BfmLead*`, `BfmLagTimeS`, `BfmYoYoHeightM` | 45°/3 nm/4 s, 2.5 s, 400 m | ditto, yo-yo 600 m | [SETZ] |
| `BfmScanAmplitudeDeg` / `BfmScanPeriodS` | 8° / 30 s | identical (~1.7 °/s weave: a scan, not a turn) | [HERL] from `2π·A/T` |
| `BfmFloorFt` | 2000 | 2000 | [SETZ] |
| `BfmGunBurstS` / `BfmGunTrackMaxErrDeg` / `BfmGunFireTolFrac` | 0.5 s / 20° / 0.35 | no override → generic values | [SETZ], burst length = the bus minimum spacing, hence continuous fire as long as the funnel holds |
| `SearchRadarModeOrdinal` | −1 | **1** (`FBF16FcrMode::Crm`) | the only F-16 mode that searches wide and does NOT lock by itself |
| `InterceptSpeedKt` | 300 | **550 kt TAS** | at 8,000 m ≈ 375 KCAS = the measured corner speed; at the same time the launch speed the round inherits |
| `InterceptLockRangeNm` | 20 | **16** | outside every head-on measured Rtr (~11 nm at 8,000 m [MESS]) and inside the 40 nm search gate |
| `InterceptCrankAtaDeg` | 45 | **45** | [HERL] STT gimbal limit ±60° minus 15° of reserve for a manoeuvring target |
| `InterceptShotAtaDeg` | 30 | 30 | a rail-launched round given more than ~30° to make up spends its motor on that |
| `InterceptAbortRangeNm` | 5 | 5 | below that, Rmin and the merge are the same problem |
| `InterceptShotRtrFactor` / `ShotSpacingS` / `BeamOffsetDeg` / `ChaffIntervalS` / `DefendHoldS` | 1.0 / 12 s / 90° / 3 s / 12 s | identical | [SETZ] resp. notch geometry |
| `AttackReleaseBiasS` / `AttackCcipTolM` | 0 / 60 m | 0 / **45 m** | [HERL] the fragment pattern of a Mk-82 takes a soft installation out to ~25 m and degrades it out to ~45 m (`modules/ground/FBGroundTarget.h`) — further off than that the run achieves nothing, and that is exactly when a pilot does not pickle |
| `AttackEgressTurnDeg` / `ClimbM` / `RangeM` / `S` | 120° / 500 m / 12 km / 25 s | **135°** / 600 m / 12 km / 30 s | an escape turn well behind the beam; 135° at the bank limit are comfortable in 30 s |
