# W1 — Red Flag / Nellis (DACT against aggressors)

**Status: BUILT AND FLOWN 2026-07-30** — ten `mods/f16/src/missions/w1-*.fbm` + `mods/f16/src/campaigns/w1-red-flag.fbc`,
both determinism criteria on the first attempt, the replay run after the FIRST mission, three findings,
and the anchor raised to **[T1]** on the same round (the 403-blocked fact sheet was read through the
Wayback Machine). See §State. **It is also the only campaign checked against
[`../doctrine-evolution.md`](../doctrine-evolution.md)'s saturation gate, and the gate REFUSES it** —
with three numbers, in §State.

**What this file is:** a **campaign spec** — ten missions derived from one historical anchor, plus the
cast they need and the honest list of what FlightBox cannot do for them yet. It is not a distillation
of a single manual; it has two source classes and they are kept apart.

| Source class | What it is | Where |
|---|---|---|
| **Anchor sources** | the public record of Red Flag and the USAF aggressor enterprise | §Knowledge 1, every fact cited and tiered |
| **FlightBox sources** | what a `.fbm` can declare and what the two modules can do | [`../missions/syntax.md`](../missions/syntax.md), [`../missions/verdict.md`](../missions/verdict.md), [`../missions/combat.md`](../missions/combat.md), [`../missions/weapons.md`](../missions/weapons.md), [`../missions/sensors.md`](../missions/sensors.md), [`../duels.md`](../duels.md), [`../formation.md`](../formation.md), [`../modules/f16/module.md`](../modules/f16/module.md), [`../modules/mig29/module.md`](../modules/mig29/module.md) |

Confidence legend (identical in all ten campaign files): **[T1]** official/government/service
document · **[T2]** service or manufacturer publication, official history · **[T3]** established
literature and specialist press · **[T4]** encyclopaedic/community consensus · **[DISPUTED]** sources
conflict, both values given · **[SET]** a FlightBox setting chosen here, not sourced · **[DERIVED]**
computed from a named relation. Gap IDs `C0…C24` are the shared catalogue in
[`INDEX.md`](INDEX.md).

**Temporal honesty:** none needed. The F-16 is Red Flag's most-flown participant and the
aggressor F-16 emulating a Fulcrum is the documented arrangement [T4]. FlightBox inverts it in a way
worth stating: **at Nellis the "MiG-29" is an F-16 pretending; here the MiG-29 is the real module.**
The campaign is therefore easier for us than for the USAF and harder in one respect — our Fulcrum has
the R-73 and the IRST the aggressor jet never had.

---

## Spec

### 1. The anchor, in one table

**Six of these rows moved to [T1] on 2026-07-30**, when the 414th CTS fact sheet was read through the
Wayback Machine after three rounds of HTTP 403 (§State, §Knowledge 1). The tiers below are the current
ones.

| Fact | Value | Tier |
|---|---|---|
| Exercise | RED FLAG, Nellis AFB, Nevada, over the Nevada Test and Training Range (NTTR) | **[T1]** |
| Purpose | give aircrew their **first ten combat missions** in training, because Vietnam-era loss rates fell sharply once a pilot had survived ten | **[T1]** |
| Started | 1975, the idea of Lt Col Richard "Moody" Suter, directed by Gen Robert J. Dixon (TAC) | **[T1]** |
| Blue mission types | offensive counter air, SEAD, combat search and rescue, dynamic targeting, defensive counter air | **[T1]** |
| Range target inventory | mock airfields, vehicle convoys, tanks, parked aircraft, bunkered defensive positions, missile sites | **[T1]** |
| The aggressors' own job | *"a scalable threat presentation to Blue forces which aids in achieving the desired learning outcomes for each mission"* — i.e. the ladder | **[T1]** |
| Red force organisation, 2022 | aligned under the **57th Operations Group**, seven aggressor squadrons including fighter and air-defence units | **[T1]**, and [DISPUTED] against the row below |
| Opposing force | **64th Aggressor Squadron**, F-16C/D in adversary schemes, **emulating the MiG-29 Fulcrum** | [T4] |
| Aggressor organisation | consolidated under the **57th Adversary Tactics Group**, formed 1 July 2005 | [T4] |
| Form | large-force employment: a Blue package (strike, escort, SEAD, support) against a Red air and ground threat array | [T4] |
| Anchor region | Nellis AFB ≈ 36.24 N 115.03 W; NTTR ranges ≈ 37.0–37.6 N 115.5–116.6 W (**approximate, verify against DEM before building**) | [T4] |

### 2. The campaign contract

| Contract | Acceptance / measurement anchor |
|---|---|
| The campaign is a **training ladder**, not a war | every mission has a stated skill it isolates; a mission that tests two things at once is split |
| Difficulty rises monotonically | mission `n+1` adds exactly one degree of freedom over `n` (one more aircraft, one more axis, one fewer sensor) |
| **Ground targets appear in every mission**, not only the strike ones | the NTTR is a range: even a 1v1 carries a range pit to hit or defend. `module target_soft`/`target_hard` per [`../missions/weapons.md`](../missions/weapons.md) |
| Every mission has ONE tactical question | stated in its row; a run whose telemetry cannot answer it is a badly built mission, not a result |
| The verdict is machine-read | `objective` lines per [`../missions/verdict.md`](../missions/verdict.md); combat missions read out of `eng_*`/`bfm_*`/`flt_*` as in [`../duels.md`](../duels.md) |
| Nothing is scripted | each jet gets a vector, a master-arm call and a brief; no briefed release in the air-to-air rides |
| Determinism holds | one fingerprint per mission over `--threads 1/2/4` × 3 repeats, as every campaign in the tree |

### 3. The ten missions

Package sizes are Blue × Red and every one of them is `[SET]`: the [T1] fact sheet gives cumulative
totals since 1975 and no air tasking order. "Wx" is the `wx` line
([`../missions/weather.md`](../missions/weather.md)). **The "Time" column was written when `C2` was open
and it is closed** — the `time` line has existed since 2026-07-28, `w1-10-graduation.fbm` declares
`2022-08-17T07:00:00Z` and the runner logged `sunElDeg −38.088` for it. The table is left as written; the
`## State` section is the record of what was flown.

| # | Mission | Task | Time | Wx | Blue | Red | Ground targets | Victory condition | **The one tactical question** |
|---|---|---|---|---|---|---|---|---|---|
| 1 | `w1-01-merge` | BFM, neutral merge | day | calm | 1 F-16 | 1 MiG-29 | 1 `target_soft` range pit (post-fight Mk-82 pass) | `kill unit red1` + `survive` | Does the pilot convert a NEUTRAL merge into a firing position before its own energy is spent — and does it still have the fuel and the height for the range pass afterwards? |
| 2 | `w1-02-two-v-one` | BFM, section against a single | day | calm | 2 F-16 (flight) | 1 MiG-29 | 1 `target_soft` | `kill unit red1` + both `survive` | With only one target for two shooters, does the sort **deliberately** double up instead of leaving one member idle? (`flt_dup` = 1 with `flt_free` = 0 is correct here — the inverse of the `formation.md` acceptance) |
| 3 | `w1-03-bvr-single` | BVR intercept | day | calm | 1 F-16 | 1 MiG-29 | 1 `target_hard` behind the merge | `kill unit red1` | At what multiple of `Rtr` does this pilot fire, and does the answer move when the opponent is the one with the longer `Raero`? (`duels.md` §Knowledge 4) |
| 4 | `w1-04-bvr-pair` | BVR intercept, 2v2 | day | calm | 2 F-16 (flight) | 2 MiG-29 (flight) | 1 `target_hard` | `kill team hostile` + both `survive` | Does the cooperative sort hold two distinct targets through the whole run, and what does the cover rule cost when both members reach `Rtr` at once? |
| 5 | `w1-05-defensive-ca` | defensive counter-air | day | calm | 2 F-16 | 2 MiG-29 with `set task attack` | 1 `target_hard` (the asset), 2 `target_soft` | Red's `kill unit` on the asset must FAIL | Can a CAP that does not know where the strike will cross deny a target it is standing over? (**`C12`**: "the asset must survive" is not declarable — it is read as Red's failed `kill`) |
| 6 | `w1-06-strike-escort` | strike with escort | day | calm | 2 F-16 strike + 2 F-16 escort | 2 MiG-29 | 1 `target_hard` + 2 `target_soft` array | Blue `kill unit` on the array + escorts `survive` | Does the escort stay tied to the strikers, or does it chase the picture and leave them naked? (there is no escort role — `C12`, `C15`) |
| 7 | `w1-07-emcon` | silent ingress | day | calm | 2 F-16, `set fcr_mode off` until commit | 2 MiG-29 with KOLS armed | 1 `target_hard` | Blue `kill unit` on target + `survive` | What does going silent buy against an opponent whose primary sensor is **passive**? (the mirror of `duel-emcon`: there the MiG was silent) |
| 8 | `w1-08-degraded` | fight without the radar | day | calm | 2 F-16, one with `set fcr_mode off` for the whole run | 2 MiG-29 | 1 `target_soft` | `kill team hostile` + both `survive` | Is a jet with no radar of its own still a shooter on the flight's shared picture, or only a passenger? (`C21`: a real battle-damaged start cannot be declared) |
| 9 | `w1-09-lfe-four` | large force, four-ship | day | `wx fixture` | 4 F-16 (one flight) | 4 MiG-29 (one flight) | 3 `target_soft` + 1 `target_hard` | `kill team hostile` + ≥3 of 4 `survive` | With four shooters and four contacts, does the minimum-cost matching still produce one target each — and how far does the assignment churn (`flt_switch`) rise with the extra degrees of freedom? |
| 10 | `w1-10-graduation` | full LFE | **night** | `wx fixture` | 4 F-16 strike + 2 F-16 escort (two flights) | 4 MiG-29 + 2 with `set task attack` | 1 `target_hard` (Blue's), 3 `target_soft`, 1 `target_hard` (Red's, defended by Blue) | Blue `kill unit` on Red's array AND Blue's own asset not killed | Does the package hold together — strike on target, escort on the threat axis, nobody engaged twice — when both sides have a job on the ground as well as in the air? |

### 4. The cast this campaign needs

| Unit | Class | Exists today | Note |
|---|---|---|---|
| F-16C | flyable module | **yes** (`f16`) | Blue |
| MiG-29 | flyable module | **yes** (`mig29`) | Red — the FlightBox aggressor is the real jet, not an emulator |
| `target_soft` | ground | **yes** | range pits, vehicle parks |
| `target_hard` | ground | **yes** | range structures, mock HAS |
| AAA site | ground, shooting | **no** (`C1`) | NTTR threat array; without it a Red Flag ride has no reason to fly high |
| SAM simulator (SA-6/SA-8 class) | ground, emitting + shooting | **no** (`C1`) | the range's threat emitters are the whole point of the ground array |
| AEW (E-3 class) | air, support | **no** (`C6`) | the exercise's picture provider |
| Tanker (KC-135 class) | air, support | **no** (`C5`) | every Red Flag ride tanks |
| Range control / "kill removal" | infrastructure | **no** | not a unit — a campaign-layer function (`C0`) |

### 5. What must be true before mission 1 can fly

*Written before the build; the count was wrong and the correction is the point.* `w1-01`, `w1-02`,
`w1-03`, `w1-04` are buildable **today** with a stated deviation (no AAA, no
tanker, day-only). `w1-05` onwards needs at least one item from the gap list below.

**Re-checked against the tree on 2026-07-30 before a line was written (campaign rule 7): six of ten
"blocked" rungs were not.** `C2` (the clock), `C12` (`protect`, and with it both the asset and the escort
verdict) and `C9` (a MiG-29 that can fly `set task attack`) had all closed since the spec was written,
and `C1` was never needed — W1's Red threat is aircraft, not batteries. **10 of 10 ran and 10 of 10
answered**; no spec mission was dropped, folded or added, because the ten rungs are the anchor's own
number and dropping one would break the ladder.

---

## State

**BUILT AND FLOWN 2026-07-30.** Ten `mods/f16/src/missions/w1-*.fbm` plus `mods/f16/src/campaigns/w1-red-flag.fbc`. The
ninth of the ten campaigns to exist as files, and the first whose **anchor moved to [T1] on the round
that built it**. No `sim/src/` file, no tool and no asset was touched — `git status --porcelain` lists
eleven new untracked files and no modified one, so the 226 pre-existing missions are byte-identical by
construction.

### Why it was unblocked, and what that cost

The spec called W1 blocked on the aggressors: the adversary rows in the air catalogue are `ALPHA` and
`A15` forbids an `ALPHA` row from being scored in a gun fight. **W1's own point dissolves it.** At Nellis
the "MiG-29" is an F-16 of the 64th Aggressor Squadron in an adversary scheme [T4]; here it is the real
`mig29` module. All eleven files fly `f16` and `mig29` and **no catalogue row at all**, which is the same
discipline W3, W4 and O3 demonstrated, and `A15` is untouched. The substitution's DIRECTION is in every
header: this aggressor carries the R-73, the KOLS and a helmet cue the real one never had, so Red is
materially **stronger** than the historical opposing force.

### The blocked source was read, through the Wayback Machine

`PROGRESS.md` has carried the 414th CTS "Red Flag" fact sheet as **HTTP 403 to automated retrieval**
since run 1. `web.archive.org/web/20240116184035/` returns it complete ("current as of October 2022"),
which is the path that lifted O2's two CIA documents. Five claims move from [T3]/[T4] to **[T1]**:

| Claim | Status now |
|---|---|
| Purpose and form — a contested combat-training exercise coordinated at Nellis, flown over the NTTR | **[T1]** |
| Founded 1975, the idea of Lt Col Richard "Moody" Suter, directed by Gen Robert J. Dixon (TAC) | **[T1]** |
| **The ten-mission rationale, verbatim** — *"if a pilot survived his first 10 combat missions, his probability of survival for remaining missions increased substantially… Red Flag was designed to expose each 'Blue' force pilot to their first 10 'combat missions' here at Nellis"* | **[T1]** — the campaign's own mission count is the anchor's number, not a `[SET]` choice |
| The five Blue mission types: offensive counter air, SEAD, combat search and rescue, dynamic targeting, **defensive counter air** | **[T1]** — rungs 5 and 6 are the anchor's words |
| The NTTR target inventory: mock airfields, vehicle convoys, tanks, parked aircraft, bunkered defensive positions, missile sites | **[T1]** — every ground object in all ten files is drawn from this list |
| The aggressors provide *"a scalable threat presentation… which aids in achieving the desired learning outcomes for each mission"* | **[T1]** — **the ladder itself** |
| Package sizes, exercise cadence, sorties per exercise | **still unsourced.** The fact sheet gives cumulative totals since 1975 (30,268 aircraft, 423,248 sorties, 529,722 personnel) and no air tasking order. Every force size in W1 stays `[SET]` |
| Aggressor organisation | **[DISPUTED] and carried both ways**: the spec has the 57th Adversary Tactics Group, formed 1 July 2005 [T4]; the 2022 fact sheet aligns the Red threats under the 57th **Operations** Group [T1] |

### The arena, and its one decisive property

One east–west lane at 37.30 N from −116.58 (the aggressors' entry) to −115.52 (Blue's, the Nellis side),
inside the spec's own NTTR box. `--elev const`, 0 m range floor, so every declared altitude is a height
above a flat plane and not a chart figure (the real floor is ~1.2–1.5 km; `C4` means there is nothing to
mask behind it anyway). Passing `--elev const` explicitly is a **precondition** and not a convention —
O3's warning, and every command in this record does it.

**The lane is 93.9 km long and the APG-68's gate against a Fulcrum-sized target is 100.0 km.** THE RANGE
IS SHORTER THAN BLUE'S RADAR REACH, so no Blue jet on this arena flies a search phase it was not given:
`w1-03` detects at t = 3.9 s / 49.35 nm where [`../duels.md`](../duels.md) row 1 measured 24.0 s /
52.19 nm for the identical radar over 111 km of open water. The N019's gate against an F-16 is 50.0 km,
53 % of the lane. **Every W1 result is a result about a short arena**, and a Red Flag lane cannot be
longer.

### The ten rungs, with their outcome

Campaign exit **3**, step exits `0 0 0 3 3 3 3 3 3 3`, whole campaign 22.5 s of wall clock.
`ATTRITION unitsFriendly=2 unitsHostile=4 groundFriendly=0 groundHostile=5`;
`EXPENDED aim120=22 aim9=4 r73=21 r27r=25 mk84=5 fab500=4`.

| # | Mission | exit | The answer to its one question |
|---|---|---:|---|
| 01 | `w1-01-merge` | **0** | **Yes, in 10.1 s, and with the round on the rail.** AIM-9 at **1.819 m** kills; the R-73 back at **2.622 m** only degrades — `duel-merge` measured 2.64 m and the same survival, so W1 reproduces it to **0.02 m** on another continent. Neither jet fires a single gun round |
| 02 | `w1-02-two-v-one` | **0** | **The sort doubles up deliberately, and the section pays one F-16 for one Fulcrum.** Two `SORT_ASSIGN` at t = 3.9 s, both on track 1, both `free=0 dup=1`; `flt_dup && flt_free > 0` is **0 of 948 ticks**; `flt_defer_s` **0.0** — the cover rule correctly does not fire. Two AIM-120 at 2.191/1.988 m kill; the R-27R at 2.320 m kills the lead 0.3 s later |
| 03 | `w1-03-bvr-single` | **0** | **The trigger is Rtr minus one decision tick and it is blind to the opponent's envelope.** Blue 9.570 nm at Rtr 9.783 = **0.978 × Rtr**; the aggressor 10.016 at 10.254 = **0.977 × Rtr**, with a Raero a factor of **1.26** longer. The two ratios differ by **0.0015**. duels.md row 1's published 9.57/9.78 and 10.02/10.25, to two decimals, on a new arena |
| 04 | `w1-04-bvr-pair` | 3 | **The cooperative sort does not split the pair's rounds and the cover rule costs nothing.** 163 `SORT_ASSIGN`, `flt_switch` **120/41** against the contract's **1/1**, both AIM-120 on the same aggressor (5.93/6.12 m, no kill), 0 `COVER_DEFER`, four R-27R expiring at 15.28/17.86/22.48 m. Four aircraft alive |
| 05 | `w1-05-defensive-ca` | 3 | **This CAP denied nothing, and the control run is what says so.** Both aggressors release; `aimErrM` **101.05/102.13 m** against a 68.4 m weapon. Attribution run A1 (CAP deleted): the **identical digits at the identical ticks**, and the CAP is worth **11 of 184** telemetry columns on the lead and **9 of 184** on the wingman — all RWR bookkeeping — with **zero trajectory cells** over 5,200 rows |
| 06 | `w1-06-strike-escort` | 3 | **Tied where it matters, gone afterwards.** Separation escort↔striker **0.49 km at the release**, 7.15 km at t = 200, **60.30 km at t = 383.5**. Both aim points destroyed (`aimErrM` 13.07/18.87 m), both `protect` objectives **met**, and an R-27R at 12.001 m inside its own 13.8 m fuze that does not kill |
| 07 | `w1-07-emcon` | 3 | **Total silence bought survival and cost the entire engagement.** `fcr_on` 0 for 6,000 ticks, 0 shots — and `eng_state` is NOT `idle`: the pilots defend **471.8 s** and **39.9 s** and spend all 60 chaff. Red fires **10 rounds and lands 0**, closest 14.85 m against a 13.8 m fuze. The passive half of the question is unanswerable: two live KOLS tracks, no consumer (`D3`) |
| 08 | `w1-08-degraded` | 3 | **A radar-less wingman is a passenger, and worse than predicted.** `fcr_contacts` max 0, `dl_tracks` max 1 (it HAS the picture) and `flt_assign` max **0** — it is never even assigned a target. One token turns a flight that never fires into a flight that fires once (4.740 m, no kill) |
| 09 | `w1-09-lfe-four` | 3 | **The churn is not the story: three of four Blue shooters never pressed.** `flt_switch` **306** against **5**, `flt_dup && flt_free > 0` **0 of 6,000 on all eight**. `nvblu1/2/3` each hold `attack` for exactly **152 ticks** and then defend, because their warning lands at **117.9 s** and the aggressors' four R-27R leave at 126.1–126.7 s. `nvblu4`'s warning is **8.4 s** later and it is the only Blue jet that fires |
| 10 | `w1-10-graduation` | 3 | **The package half-holds.** Night confirmed by the runner (`sunElDeg −38.088`). Three of four aim points destroyed, Blue's own asset held, Blue loses one aircraft — the fourth striker, killed by an R-27R at **1.788 m** before its release, so the bunker's 17.7 m question is **not answered** and the file says so. The fixture's wind costs **~40 m** of cross-track (`aimAcrossM` 44–47 m against w1-06's 7.25 m in calm air) |

### The carry — kill removal, and it is worth one aircraft

W1 does **not** narrow the carry the way O4 did, and the reason is the anchor: the spec's own cast table
lists *"Range control / kill removal"* as **"not a unit — a campaign-layer function (`C0`)"*. Dropping a
destroyed unit from the next mission is exactly what the layer does, so W1 is the campaign in which the
carry performs the anchor's own range procedure. It lands in **one** place: sorties 09 and 10 are one
range period flown by one set of airframes (`nvblu1..4`, `nvagr1..4`) over one range array; the other
eight sorties carry fresh callsigns, ground objects included.

| Measured | Value |
|---|---|
| What the layer did | **7 `campaign CARRY` lines** in step 10: `nvagr4` **dropped** (destroyed in step 09) plus **six** `set store` lines deleted from the three survivors |
| What it is worth | run standalone with no state, the same file loses **two** Blue aircraft instead of one — `nvagr4` flies with a full rack and kills `rf10esc2` |
| How deep it reaches | **1 of 17** common telemetry files byte-identical, 30 files against 32, the run **246.8 s against 175.2 s**, the three surviving aggressors differing in **90 / 62 / 88 of 184** columns |
| What it does NOT move | the same three aim points destroyed, the same bunker intact, the same asset held. **The carry moves the air result and not the ground one** |
| Attribution, one fact at a time (`--carry`) | `units` alone → 1 Blue loss · `stores` alone → 1 Blue loss · `ground` alone (i.e. neither) → **2**. Either half alone is enough: the fourth aggressor is only lethal with a full rack |

### Determinism — both criteria, on the first attempt

| # | Criterion | Measured |
|---|---|---|
| **1** | one campaign fingerprint over 3 reps × `--threads 1/2/4` | **9 runs, 1 fingerprint** `5de43dd58a859b51319ebd3a4b7c27bbe5c0e2c42c7010cb67a0b3b1d8d89a32`, `--elev const`, `time 2022-08-16T17:00:00Z` |
| **2** | each step's per-mission fingerprint equals the same mission run STANDALONE with step *k−1*'s state | **10/10 MATCH** — and **the replay was run after the FIRST mission**, on a throwaway one-step `.fbc` that was deleted afterwards (`01 … fp=3555795c02ef346f MATCH`) |
| **1 — re-run 2026-07-30** | the same criterion under the branch-order change of `b433950` ([`../pilot.md`](../pilot.md) §7.4a) | **9 runs, 1 fingerprint** `0e32e6a8c02b153eddaa1b7fcf00278fa0f7318e3ebf088ea95bd181d3677add`, `--elev const`. **The value above is kept with its date; this is the current one.** Step exits `0 0 0 3 3 3 3 3 3 3` — unchanged; **8 of 10 step fingerprints moved, steps 1 and 5 held** |
| **2 — re-run 2026-07-30** | every step re-run STANDALONE against the new reference tree | **10/10 MATCH**, exit codes included |
| Conservation | annotating all ten files with their MEASURED blocks afterwards left **all ten per-mission fingerprints and the campaign fingerprint unchanged** |
| Per-step fingerprints, as built | `3555795c02ef346f 63fe25362f2c83c8 74c5812afdbd87e7 b690acd9c60de591 b43917d0e0581b44 423844acb453ed54 f50b23b55b6c7495 900fbe8e4af71bf8 ca0cbbcdf7f02fd5 a9ad699a4e298df5` |
| Per-step fingerprints, 2026-07-30 | `3555795c02ef346f e081ae58576df8d2 f2db17354494b8a4 1f0eb659cfab9be5 b43917d0e0581b44 d73c4369619330bf 0e76d1dc5ba38cf3 0916c5c1e98de80b 9879e7b636b6b42a c5afe4cfef819b66` |
| **1 — re-run 2026-07-30 (`E6`)** | the same criterion after the judge-completion fix of [`../doctrine-evolution.md`](../doctrine-evolution.md) X-1 | **9 runs, 1 fingerprint** `0e32e6a8c02b153eddaa1b7fcf00278fa0f7318e3ebf088ea95bd181d3677add`, `--elev const`. Step exits `0 0 0 3 3 3 3 3 3 3` — unchanged. **Unchanged, byte for byte**: the campaign fingerprint and all ten step fingerprints are the row above. Not one W1 rung ends before its judges are finished |
| **2 — re-run 2026-07-30 (`E6`)** | every step re-run STANDALONE against the new reference tree | **10/10 MATCH**, exit codes included |

**And the two instruments do not measure the same run, which the re-run made visible.**
[`../pilot.md`](../pilot.md) §7.4b's regression names `w1-02`, `w1-04`, `w1-08`, `w1-09` and `w1-10` as
the W1 files the branch order moved; the campaign fingerprint moved **eight** of ten, adding steps 03,
06 and 07. There is no contradiction: `tools/fb_regress.sh` runs every mission **standalone and
unclocked**, and nine of the ten W1 files declare no `time` of their own — the campaign supplies
`2022-08-16T17:00:00Z` through `--campaign-time`. Measured directly on `w1-03`, the clock alone moves
**2 columns on the shooter and 7 on the aggressor** (`blk_env`, `vis_glare`, `vis_contacts`,
`vis_best_*`) and **zero trajectory columns**. The regression compares flight columns of an unclocked
run; the fingerprint hashes all 184 columns plus the log of a clocked one. **A mission list from the
one cannot predict the other, and this is where that was paid.**

### The saturation gate — the one measurement W1 owes the other nine

[`../doctrine-evolution.md`](../doctrine-evolution.md) §4.2 with its own numbers: modal outcome class
≤ **60 %**, ≥ **3 of 9** doctrine levers moving the outcome class, ≥ **6** geometries per arena of which
≥ 3 informative. **The denominator had to be named, because the gate's own instrument cannot be pointed
at this campaign**: `tools/fb_arena_check.py` flies a fixed six-variant yardstick over the tournament's
*generated* geometries and W1's are hand-authored files. Per rung the population here is therefore the
**nine declared doctrine levers** (`fb_tournament.DOCTRINE_VARIANTS`, baseline included) applied to one
seat — exactly the population S2 is defined on — and S1's modal share is taken in it. 90 runs per seat,
run twice, byte-identical both times. `S3` is `n/a` for the reason the gate's own tool prints: both
airframes are FlightBox's read-only model copies and have no declared-ignorance band to perturb.

**Verdict: REFUSED, in both seats, at 2 informative geometries against a required 3.**

Each half of the table is one complete gate run: the levers on that seat, the modal share and the movers
counted in that seat's own class. `S1 ∧ S2` = informative.

| Rung | Blue seat: modal share | modal class | movers of 9 | S1 | S2 | Red seat: modal share | modal class | movers of 9 | S1 | S2 |
|---|---:|---|---:|---|---|---:|---|---:|---|---|
| 01 merge | 100.0 % | (3,2) | 0 | NO | NO | 100.0 % | (1,0) | 0 | NO | NO |
| 02 two-v-one | 66.7 % | (4,3) | **3** | NO | ok | 88.9 % | (1,0) | 1 | NO | NO |
| 03 bvr-single | **55.6 %** | (3,2) | **4** | **ok** | **ok** | 88.9 % | (1,0) | 1 | NO | NO |
| 04 bvr-pair | 88.9 % | (4,2) | 1 | NO | NO | 66.7 % | (4,2) | **3** | NO | ok |
| 05 defensive-ca | 100.0 % | (4,4) | 0 | NO | NO | 100.0 % | (4,2) | 0 | NO | NO |
| 06 strike-escort | 88.9 % | (10,10) | 1 | NO | NO | **55.6 %** | (4,2) | **4** | **ok** | **ok** |
| 07 emcon | 88.9 % | (4,2) | 1 | NO | NO | 100.0 % | (4,2) | 0 | NO | NO |
| 08 degraded | 88.9 % | (4,2) | 1 | NO | NO | 77.8 % | (4,2) | 2 | NO | NO |
| 09 lfe-four | 88.9 % | (8,4) | 1 | NO | NO | **55.6 %** | (7,3) | **4** | **ok** | **ok** |
| 10 graduation | **55.6 %** | (14,10) | **5** | **ok** | **ok** | 77.8 % | (12,6) | 2 | NO | NO |

The classes are componentwise SUMS over a side's members ([`../doctrine-evolution.md`](../doctrine-evolution.md)
§1: `side_key`), so a class is comparable *within* a rung and not between rungs of different size — which
is why the criterion is per geometry and the campaign-level spread of the ten baselines is not a gate
number.

- **S4 size: 10 geometries ≥ 6 — ok.**
- **S5 yield: 2 informative ≥ 3 — NO.** Blue seat `{03, 10}`, Red seat `{06, 09}`. Four *distinct*
  geometries are informative in at least one seat, but the gate sweeps one seat and the count is 2.
- **S6 distinctness: 0 identical class vectors — ok.**
- **S1's threshold is coarse at this denominator and the coarseness is stated:** with n = 9 the modal
  share can only be 9/9, 8/9, 7/9, 6/9 or 5/9, so *"≤ 60 %"* means *"≥ 4 runs outside the modal class"*.
  The gate's own 60 % is taken in a 60-run field, where it admits 36. Rung 02 fails S1 at 66.7 % on a
  grid with no value between 66.7 and 55.6.
- **Where the levers bite is a property of the SEAT, and the two seats bite on disjoint rungs.** The
  F-16 seat's movers sit on rungs **02 (3 of 9), 03 (4) and 10 (5)** — the two smallest rungs plus the
  capstone — and are 1 or 0 on every other. The MiG-29 seat's sit on **04 (3), 06 (4) and 09 (4)** — the
  multi-ship rungs — and are 1 or 0 on the small ones. **The two sets do not intersect anywhere.** That is
  [`../duels.md`](../duels.md)'s own tournament finding — the early launch is worth an entire outcome
  band on the MiG and essentially nothing on the F-16 (601.8 → 603.3) — reproduced on ten hand-authored
  geometries.

**So: does the ladder rise, or only claim to?** It rises, and the ground half rises further than the air
half:

| Reading | Number |
|---|---|
| Rungs that DECIDE (exit 0, Blue meets everything it declared) | **3 of 10 — and they are rungs 1, 2 and 3.** No rung from 4 onwards concludes anything |
| Force ratio of every deciding rung | 1v1, 2v1, 1v1. Every 2v2 and 4v4 rung ends with everybody alive |
| Air-to-air kills across the campaign | 4 Red, 2 Blue — and 3 of the 4 Red losses are on rungs 1–3 |
| Rounds that arrive INSIDE their own fuze radius and do not kill | **five** (R-27R at 12.00, 13.51, 13.66, 13.75 m against 13.8 m; plus 14.85 m just outside). O2's rule, five more times |
| Ground objects destroyed | 2 of 3 attacked on rung 6, 3 of 4 on rung 10 — the ground half decides at every scale |

**CONFIRMED OVER THE OTHER NINE, 2026-07-30** ([`../doctrine-evolution.md`](../doctrine-evolution.md)
§State `E5`): the same gate, with the fixed yardstick as S1's population instead of the lever set W1
had to substitute, was pointed at **all 154 cells of the ten campaigns** — 3,388 runs. **0 informative
cells** under the genome's own alphabet and **2** under the loosest reading the gate admits, i.e. W1's
own verdict reproduced at fifteen times the size. 89 of the 154 cells are moved by no lever at all,
and the two that pass both criteria are `w3-09-saturation` and `w3-10-package-q` — two capstone
packages, both far above two aircraft a side, both moved by the GROUND gene and the channel bit. W1's
sentence below is therefore not a property of Nellis.

**The honest verdict:** W1's difficulty rises monotonically as a syllabus, and its measured outcome
saturates from rung 4 on for a reason the ladder did not choose — a 2v2 in this tree is more saturated
than a 1v1, which is exactly what [`../doctrine-evolution.md`](../doctrine-evolution.md) already measured
on generated geometries. **The air half of a training ladder cannot be graded above two aircraft a side
in this tree; the ground half can.** An evolution run must not start on this arena, and the gate says so
with three numbers.

### What already existed and was reused without change

| Piece | Where |
|---|---|
| The BFM phase with its own control law, the datum, the corner-speed hooks | [`../pilot.md`](../pilot.md) §5, [`../missions/combat.md`](../missions/combat.md) |
| The BVR engagement state machine and its sixteen `eng_*` channels | [`../pilot.md`](../pilot.md) §7–§9 |
| Flight declaration, station keeping, target sort, cover rule, fourteen `flt_*` channels | [`../formation.md`](../formation.md) |
| The eight duel geometries as a template for the air-to-air rides | [`../duels.md`](../duels.md) |
| Ground targets, load-out, CCIP/CCRP release, the measured error budget | [`../missions/weapons.md`](../missions/weapons.md) |
| `protect` as the escort and the asset verdict (round `C12`) | [`../missions/verdict.md`](../missions/verdict.md) |
| The campaign layer's carry, state file and both acceptance criteria | [`../missions/campaign.md`](../missions/campaign.md) |

### Found while building — three, none fixed here

| # | Finding | The measurement |
|---|---|---|
| **1** | **A `set task bfm` jet with NO `wp` line has no fire control at all, and the failure is silent.** `modules/f16/FBF16FireControl.cpp:141` invalidates the whole block when `state.Nav` is unreadable, and `systems/FBNavSystem` publishes nothing without a steerpoint (`FBNavSystem.cpp:53`, `if (Have \|\| HaveBull)`). So the gun solution, the DLZ and the WVR missile gate all depend on a navigation waypoint | The first cut of `w1-01`, written without one: `blk_firecontrol` **0 for all 3,001 rows**, `gun_sol_err` −1 throughout, **0** `pilot BFM_SHOT`, 0 rounds fired, exit 3 with both jets alive after **14.8 s of unbroken lock** from 7.5 km down to 185 m. One `wp` line per jet and nothing else: exit 0, AIM-9 away at t = 1.0 s. **And it is a property of the committed tree, not of this campaign**: the shooters of `bfm-basic`, `bfm-merge`, `bfm-offset` and `bfm-blind` all declare no `wp`, while `gun-bfm` and `gun-turning` — the two that fire — both do |
| **2** | **The MiG-29's OPT director releases on a solution it has already computed as a miss.** There is no refusal gate on predicted delivery error | `w1-05`: `sms RELEASE_SOLUTION … aimMissM=97.87` and `99.03` logged BEFORE the pickle, both accepted, both landing at `aimErrM` 101.05 / 102.13 m against a 68.4 m lethal radius. The same shape appears in `w1-10` at 88.22 / 97.46 m |
| **3** | **An air-to-air hit that degrades a striker's radar costs the strike nothing, because the striker was not using it.** `w1-05`'s AIM-120 arrives 9.469 m from the lead aggressor, fails nothing and degrades `radar` — on a jet declared `n019_emission off` | The full-file diff against attribution run A1: **11 of 184 columns** on that aircraft, **0 trajectory cells** over 5,200 rows, and `aimErrM` identical to five decimals. `FBSystemHealth` has no notion of which systems the mission needs |

### The extension of the cross-tree numbers

| Number this campaign extends | W1's point |
|---|---|
| O3's striker cross-track ladder (+48.3 m at 6 km, +87.2 at 12, +90.9 at 24) | **+99.7 m and +101.2 m at a 44 km diagonal ingress** (`w1-05`), and +84.3 / +94.5 m at 28 km (`w1-10`) |
| W4's wind: 5.014 m of along-track bomb miss per knot | on the F-16/Mk-84 pair the fixture costs **~40 m of CROSS-track**: `aimAcrossM` 44.46/46.03/47.22 m under `wx fixture` against 7.25/7.28 m in calm air, same aircraft, same store, same altitude band |
| O4-10's cooperative-versus-contract churn (52/19 against 2/1) | **120/41 against 1/1** at 2v2 and **306 against 5** at 4v4 — and the churn per aircraft is flat between the two (80.5 → 76.5), so the extra members do not multiply it |
| duels.md Knowledge 3's 1.5 s early-launch margin | **8.4 s at four-ship scale, and it is worth three of four triggers** (`w1-09`) |

---

## Gaps

Ordered by how much of this campaign they block. IDs are the shared catalogue in
[`INDEX.md`](INDEX.md). **Re-checked line by line on 2026-07-30 (campaign rule 7); the "State after the
build" column is the whole point of re-checking.**

| ID | What is missing | Blocks here | State after the build |
|---|---|---|---|
| `C0` | **no campaign layer** — ten `.fbm` files are ten unrelated runs; nothing carries losses, stores, fuel or a destroyed target from one to the next | the campaign as a campaign (all 10) | **CLOSED 2026-07-28 and flown here.** W1 carries all three facts and its `units` half performs the anchor's own kill removal — worth one F-16 |
| `C1` | **no active surface-to-air threat** — `target_soft`/`target_hard` are inert; nothing emits, nothing shoots | the NTTR threat array; the reason Red Flag flies the profiles it flies (5–10) | **CLOSED for the eastern rows, and W1 NEVER NEEDED IT**: the aggressors are aircraft. Still open where it bites here — there is **no Western AAA or SAM row of any kind**, so the aggressor air-defence squadrons the [T1] fact sheet names have no counterpart and W1 measures nothing about the threat array that decides a Red Flag ride's altitudes |
| `C2` | **no time of day in mission data** — `.fbm` has no clock; the renderer's ephemeris is a client-side switch | `w1-10` is a night ride and cannot say so | **CLOSED 2026-07-28.** `w1-10` declares `time 2022-08-17T07:00:00Z` and the runner logs `sunElDeg −38.088`; the nine day rides take the campaign clock at `+45.71` |
| `C6` | **no live AWACS/GCI unit** — GCI is `set brief_gci` static text with entry latency, not a controller reacting to a moving picture | the exercise's whole command layer (1–10, softly) | **open, and priced.** `w1-07`'s aggressors are talked onto the picture by three typed calls; their radar comes on at t = 27.9 s and the brief is what turns it on (O2's finding, reproduced). A controller that reacted would be a different experiment |
| `C5` | **no aerial refuelling, no external tank in the catalogue** | fuel planning is absent from every ride | **open.** Every W1 jet spawns airborne at 70–80 % and no ride is fuel-limited |
| `C11` | **no strafing** — gun bundles are not resolved against ground targets | the range gunnery half of `w1-01`/`w1-02` | **open, and it is not the only blocker**: `set task` selects one phase at spawn and `pilot/FBPilot` has **no Bfm → Attack transition at all**, so the jet that fought the merge could not have bombed the pit either way. Both rungs say so in their headers |
| `C15` | **no package coordination** — no time-on-target, no deconfliction, no lead tasking, combat spread only, no rejoin | `w1-06`, `w1-09`, `w1-10` | **open, and MEASURED as a number**: `w1-06`'s escort holds 0.49 km at the release and **60.30 km** by t = 383.5 s. `protect` gives the verdict; nothing gives the station |
| `C12` | **objective vocabulary is four kinds** — no *protect*, no *escort*, no *deny*, no time window | `w1-05` and `w1-10` must express "the asset survives" as somebody else's failed `kill` | **CLOSED 2026-07-28, and the workaround is superseded.** `w1-05` and `w1-10` declare `protect unit` on the asset and `w1-06`/`w1-10` on the strikers; in `w1-05` the protector's `met` and the attacker's `unmet` agree, which the workaround could never check |
| `C3` | **no visual acquisition** — the merge is fought on radar/IRST alone | every BFM ride is more sensor-driven than the real thing | **CLOSED 2026-07-28 (`FBVisualSystem`), and it changes nothing here**: in `w1-01` the aggressor's eye reports `vis CONTACT` at t = 12.0 s and `vis RECOGNISED … type=f16` at t = **14.0 s** — 3.9 s after the AIM-9 had already killed it. At merge closure the eye is a spectator |
| `C21` | **no declarable initial damage** — a jet cannot start degraded, only switched off | `w1-08` fakes it with `set fcr_mode off` | **open**, and `w1-08`'s header states the substitution's direction: a switched-off radar leaves the fire-control block alive where a shot-out one would not, so the measured answer (a passenger) is the generous one |
| `C4` | **no terrain masking** | the NTTR's terrain is scenery, not cover | **open.** W1 is flown `--elev const` on a 0 m plane, so the NTTR's 1.2–1.5 km floor is not even present as data. Every altitude in every file is a height above that plane |
| `C18` | **no radio between units** | no "blind", no "press", no bugout call | **open.** `w1-09`'s three non-shooters go defensive silently; nothing in the tree could have told them the fourth was still in parameters |

### Known FlightBox behaviours that will shape the results (not gaps — findings to expect)

| Behaviour | Consequence for this campaign | Known from | What happened |
|---|---|---|---|
| The symmetric F-16 arena is a stalemate | `w1-01`…`w1-04` are asymmetric on purpose (F-16 vs MiG-29) — a Blue-vs-Blue Red Flag ride would measure nothing | [`../pilot.md`](../pilot.md) gap 2.3, [`../duels.md`](../duels.md) | **worse than expected: the ASYMMETRIC arena is also a stalemate above two aircraft a side.** Rungs 04, 07, 08 all end with four alive; only the 1v1 and 2v1 rungs decide |
| Neither ACM box re-acquires after the first pass in a merge | `w1-01` will likely degenerate into two blind turns; the mission's value is that it MEASURES that | [`../duels.md`](../duels.md) row 8, `pilot.md` gaps 2.9/2.8 | **it never got the chance**: the merge was decided at t = 10.1 s on the first pass, by the AIM-9's 1.819 m against the R-73's 2.622 m |
| A separated wingman rejoins badly or not at all | `w1-09`/`w1-10` will show station errors in the tens of kilometres | [`../formation.md`](../formation.md) F1 | **confirmed with a number, and on the ESCORT rather than the wingman: 60.30 km** (`w1-06`, t = 383.5 s), after holding 0.49 km through the release |
| The cover rule is nearly free for the AIM-120 (0.3 s) | `w1-04`'s cover question is cheap for Blue and would be expensive for Red — which is the point | [`../formation.md`](../formation.md) | **it is free because it never fires**: 0 `COVER_DEFER` and `flt_both_s` = 0.0 on all four aircraft of `w1-04` and all eight of `w1-09` |

---

## Knowledge

### 1. The anchor with its sources

- **Red Flag's purpose and form — [T1] since 2026-07-30.** A contested combat-training exercise
  coordinated at Nellis AFB and conducted over the Nevada Test and Training Range, one of a series of
  advanced training programmes of the USAF Warfare Center [T1]. Established **1975** as the idea of
  **Lt Col Richard "Moody" Suter**, one of the initiatives directed by **Gen Robert J. Dixon**, then
  commander of Tactical Air Command [T1]. Its founding rationale, verbatim: *"Lessons from Vietnam showed
  that if a pilot survived his first 10 combat missions, his probability of survival for remaining
  missions increased substantially. Red Flag was designed to expose each 'Blue' force pilot to their
  first 10 'combat missions' here at Nellis"* [T1] — **so the ten missions of this campaign are the
  anchor's own number and not a `[SET]` choice.** A Blue package executes *"offensive counter air,
  suppression of enemy air defense, combat search and rescue, dynamic targeting, and defensive counter
  air"* against NTTR targets *"such as mock airfields, vehicle convoys, tanks, parked aircraft, bunkered
  defensive positions, missile sites"* [T1]; the Red threats are aligned under the **57th Operations
  Group**, seven aggressor squadrons *"including fighter, space, information operations and air defense
  units"*, whose job is *"a scalable threat presentation to Blue forces which aids in achieving the
  desired learning outcomes for each mission"* [T1] — which is this campaign's ladder, sourced.
  Cumulative since 1975, current as of October 2022: **30,268 aircraft, 529,722 personnel trained,
  164,724 aircrew, 423,248 sorties, 783,907 flying hours, 29 countries** [T1]. **No package size, no
  cadence and no per-exercise sortie count appears anywhere in it**, which is why §3's force sizes stay
  `[SET]`.
  Source: [414th Combat Training Squadron "Red Flag" fact sheet (nellis.af.mil)](https://www.nellis.af.mil/About/Fact-Sheets/Display/Article/2605882/414th-combat-training-squadron-red-flag/)
  — **READ 2026-07-30 through the Wayback Machine.** The live `nellis.af.mil` path has returned
  **HTTP 403** to automated retrieval on every pass since run 1;
  [`web.archive.org/web/20240116184035/`](https://web.archive.org/web/20240116184035/https://www.nellis.af.mil/About/Fact-Sheets/Display/Article/2605882/414th-combat-training-squadron-red-flag/)
  returns the identical document complete. **The blocked path was not the only path** — the same lesson
  O2 booked for the two CIA reading-room documents, now confirmed on a second host.
  Cross-checks (unchanged, [T4]): [Exercise Red Flag (Military Wiki)](https://military-history.fandom.com/wiki/Exercise_Red_Flag),
  [Red Flag and other Air Exercises in the NTTR](https://www.dreamlandresort.com/info/flags.html).
- **The aggressors.** The 64th Aggressor Squadron flies F-16C/D at Nellis and **emulates the MiG-29
  Fulcrum**; its jets in adversary schemes are the backbone of Red Flag's opposing force [T4]. All
  aggressor activity was consolidated under the 57th Adversary Tactics Group on 1 July 2005 [T4].
  Sources: [64th Aggressor Squadron](https://en.wikipedia.org/wiki/64th_Aggressor_Squadron),
  [57th Adversary Tactics Group](https://en.wikipedia.org/wiki/57th_Adversary_Tactics_Group),
  [Red Stars Over Nevada (MiGFlug)](https://migflug.com/jetflights/red-stars-over-nevada-inside-the-usafs-aggressor-squadrons/).

### 2. Where the sources are thin, and it is stated rather than filled

| Thing the campaign would like | Status |
|---|---|
| Exercise cadence, package sizes, sortie counts per Red Flag | **still not established, and now for a better reason than a 403.** The primary was read on 2026-07-30 and contains cumulative totals only — no air tasking order, no package composition, no per-exercise count. The package sizes in §3 are therefore **[SET]** and stay so: a ladder chosen to isolate one variable per step, not a reproduction of an ATO |
| Aggressor organisation | **[DISPUTED] and both values carried.** 57th Adversary Tactics Group, formed 1 July 2005 [T4]; 57th **Operations** Group with seven aggressor squadrons, as of October 2022 [T1]. The two are eighteen years apart and no source in hand reconciles them |
| The NTTR threat-emitter inventory (which SAM simulators, how many) | **not sourced.** No number is given above; the cast list says "SA-6/SA-8 class" as a CLASS, not a count |
| Aggressor tactics doctrine (what a Red Flag Fulcrum-emulating F-16 actually flies) | **not sourced and deliberately not guessed.** FlightBox's Red air flies the MiG-29 module's own GCI-led doctrine ([`../modules/mig29/datalink-gci.md`](../modules/mig29/datalink-gci.md)) — which is a different thing and is labelled as such |

### 3. Why this campaign is the ladder and not the showpiece

Every other campaign in the set has a historical outcome that the missions must be able to reproduce
or refute. This one does not: a training exercise has no outcome, only a syllabus. That makes W1 the
right place to put the **capability ladder** — the ten missions are ordered so that each one is the
smallest runnable increment over its predecessor, and the first four are buildable against today's
tree. If W1 cannot be flown, no other campaign can.

**And it is why W1 is the campaign that carries the saturation gate.** A ladder is the only one of the
ten whose CONTRACT is that its rungs give different answers, so it is the only one whose failure to do so
is a result rather than a disappointment. Measured (§State): the gate REFUSES this arena at 2 informative
geometries of a required 3, in both seats, and the three rungs that DECIDE are exactly the three with at
most two aircraft in them (1v1, 2v1, 1v1). The transferable
statement for the other nine campaigns is that **a FlightBox air-to-air result above 2v2 is a fixed
point** — and the eight already-built campaigns whose capstones are 4v4 or larger should be read with
that in front of them.
