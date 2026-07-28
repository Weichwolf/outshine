# W1 — Red Flag / Nellis (DACT against aggressors)

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
computed from a named relation. Gap IDs `C0…C21` are the shared catalogue in
[`INDEX.md`](INDEX.md).

**Temporal honesty:** none needed. The F-16 is Red Flag's most-flown participant and the
aggressor F-16 emulating a Fulcrum is the documented arrangement [T4]. FlightBox inverts it in a way
worth stating: **at Nellis the "MiG-29" is an F-16 pretending; here the MiG-29 is the real module.**
The campaign is therefore easier for us than for the USAF and harder in one respect — our Fulcrum has
the R-73 and the IRST the aggressor jet never had.

---

## Spec

### 1. The anchor, in one table

| Fact | Value | Tier |
|---|---|---|
| Exercise | RED FLAG, Nellis AFB, Nevada, over the Nevada Test and Training Range (NTTR) | [T4] |
| Purpose | give aircrew their **first ten combat missions** in training, because loss rates historically fell sharply after the tenth | [T3] |
| Started | 1975 | [T4] |
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

Package sizes are Blue × Red. "Wx" is the `wx` line ([`../missions/weather.md`](../missions/weather.md)).
Time of day is **not declarable today** (gap `C2`) — it is stated because the mission means it, and
the gap list says so.

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

`w1-01`, `w1-02`, `w1-03`, `w1-04` are buildable **today** with a stated deviation (no AAA, no
tanker, day-only). `w1-05` onwards needs at least one item from the gap list below.

---

## State

**Nothing built.** No file exists under `sim/missions/campaigns/`; this is a spec written before the
missions, per [`../conventions.md`](../conventions.md)'s spec-first rule.

What already exists and would be reused without change:

| Piece | Where |
|---|---|
| The BFM phase with its own control law, the datum, the corner-speed hooks | [`../pilot.md`](../pilot.md) §5, [`../missions/combat.md`](../missions/combat.md) |
| The BVR engagement state machine and its sixteen `eng_*` channels | [`../pilot.md`](../pilot.md) §7–§9 |
| Flight declaration, station keeping, target sort, cover rule, fourteen `flt_*` channels | [`../formation.md`](../formation.md) |
| The eight duel geometries as a template for the air-to-air rides | [`../duels.md`](../duels.md) |
| Ground targets, load-out, CCIP/CCRP release, the measured error budget | [`../missions/weapons.md`](../missions/weapons.md) |
| The mixed tournament as the doctrine cross-check | [`../missions/combat.md`](../missions/combat.md), `sim/tools/fb_tournament.py` |

---

## Gaps

Ordered by how much of this campaign they block. IDs are the shared catalogue in
[`INDEX.md`](INDEX.md).

| ID | What is missing | Blocks here |
|---|---|---|
| `C0` | **no campaign layer** — ten `.fbm` files are ten unrelated runs; nothing carries losses, stores, fuel or a destroyed target from one to the next | the campaign as a campaign (all 10) |
| `C1` | **no active surface-to-air threat** — `target_soft`/`target_hard` are inert; nothing emits, nothing shoots | the NTTR threat array; the reason Red Flag flies the profiles it flies (5–10) |
| `C2` | **no time of day in mission data** — `.fbm` has no clock; the renderer's ephemeris is a client-side switch | `w1-10` is a night ride and cannot say so |
| `C6` | **no live AWACS/GCI unit** — GCI is `set brief_gci` static text with entry latency, not a controller reacting to a moving picture | the exercise's whole command layer (1–10, softly) |
| `C5` | **no aerial refuelling, no external tank in the catalogue** | fuel planning is absent from every ride |
| `C11` | **no strafing** — gun bundles are not resolved against ground targets | the range gunnery half of `w1-01`/`w1-02` |
| `C15` | **no package coordination** — no time-on-target, no deconfliction, no lead tasking, combat spread only, no rejoin | `w1-06`, `w1-09`, `w1-10` |
| `C12` | **objective vocabulary is four kinds** — no *protect*, no *escort*, no *deny*, no time window | `w1-05` and `w1-10` must express "the asset survives" as somebody else's failed `kill` |
| `C3` | **no visual acquisition** — the merge is fought on radar/IRST alone | every BFM ride is more sensor-driven than the real thing |
| `C21` | **no declarable initial damage** — a jet cannot start degraded, only switched off | `w1-08` fakes it with `set fcr_mode off` |
| `C4` | **no terrain masking** | the NTTR's terrain is scenery, not cover |
| `C18` | **no radio between units** | no "blind", no "press", no bugout call |

### Known FlightBox behaviours that will shape the results (not gaps — findings to expect)

| Behaviour | Consequence for this campaign | Known from |
|---|---|---|
| The symmetric F-16 arena is a stalemate | `w1-01`…`w1-04` are asymmetric on purpose (F-16 vs MiG-29) — a Blue-vs-Blue Red Flag ride would measure nothing | [`../pilot.md`](../pilot.md) gap 2.3, [`../duels.md`](../duels.md) |
| Neither ACM box re-acquires after the first pass in a merge | `w1-01` will likely degenerate into two blind turns; the mission's value is that it MEASURES that | [`../duels.md`](../duels.md) row 8, `pilot.md` gaps 2.9/2.8 |
| A separated wingman rejoins badly or not at all | `w1-09`/`w1-10` will show station errors in the tens of kilometres | [`../formation.md`](../formation.md) F1 |
| The cover rule is nearly free for the AIM-120 (0.3 s) | `w1-04`'s cover question is cheap for Blue and would be expensive for Red — which is the point | [`../formation.md`](../formation.md) |

---

## Knowledge

### 1. The anchor with its sources

- **Red Flag's purpose and form.** A contested combat-training exercise run at Nellis AFB over the
  Nevada Test and Training Range, involving US and allied air forces [T4]. Its founding rationale is
  the "first ten combat missions" observation — historical loss rates fell sharply once a crew had
  survived roughly ten sorties, so the exercise exists to supply those ten in peacetime [T3]. Started
  1975 [T4].
  Sources: [414th Combat Training Squadron "Red Flag" fact sheet (nellis.af.mil)](https://www.nellis.af.mil/About/Fact-Sheets/Display/Article/2605882/414th-combat-training-squadron-red-flag/)
  *(the fact sheet returned HTTP 403 to automated retrieval on this pass — the purpose statement above
  is carried at [T3]/[T4] from the sources below until the primary is read; flagged in
  [`PROGRESS.md`](PROGRESS.md))*,
  [Exercise Red Flag (Military Wiki)](https://military-history.fandom.com/wiki/Exercise_Red_Flag),
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
| Exercise cadence, package sizes, sortie counts per Red Flag | **not established** on this pass; the primary fact sheet was unreachable (403). The package sizes in §3 are therefore **[SET]** — a ladder chosen to isolate one variable per step, not a reproduction of an ATO |
| The NTTR threat-emitter inventory (which SAM simulators, how many) | **not sourced.** No number is given above; the cast list says "SA-6/SA-8 class" as a CLASS, not a count |
| Aggressor tactics doctrine (what a Red Flag Fulcrum-emulating F-16 actually flies) | **not sourced and deliberately not guessed.** FlightBox's Red air flies the MiG-29 module's own GCI-led doctrine ([`../modules/mig29/datalink-gci.md`](../modules/mig29/datalink-gci.md)) — which is a different thing and is labelled as such |

### 3. Why this campaign is the ladder and not the showpiece

Every other campaign in the set has a historical outcome that the missions must be able to reproduce
or refute. This one does not: a training exercise has no outcome, only a syllabus. That makes W1 the
right place to put the **capability ladder** — the ten missions are ordered so that each one is the
smallest runnable increment over its predecessor, and the first four are buildable against today's
tree. If W1 cannot be flown, no other campaign can.
