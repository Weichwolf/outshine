# Duels — the asymmetric measurement campaign

**Source state:** commit `c637ed3` (MiG-29 stage 2c) plus this round. The subject is not a class and
not a directory: it is a **family of missions** (`sim/missions/duel-*.fbm`) and what they measure.

| Place | Role |
|---|---|
| `sim/missions/duel-*.fbm` | the campaign itself — one file per geometry, each carrying its own reading rule |
| `sim/tools/fb_duel_report.py` | the analysis tool: both sides' `eng_*` debriefing plus the EMCON timeline, straight out of one run's artefacts |
| `sim/tools/fb_tournament.py`, `sim/tools/variants-mixed.txt` | the same question inside the tournament's own scoring, on MIXED pairings |
| [`pilot.md`](pilot.md) §7–§9 | the intercept phase machine, the `eng_*` channels, the variant table |
| [`modules/mig29/module.md`](modules/mig29/module.md) | what the opponent is |
| [`missions/verdict.md`](missions/verdict.md) | why a declared `kill` objective gives a duel one winner and one loser |

Convention: **[MESS]** = measured in this campaign, **[HERL]** = derived from a named relation,
**[SETZ]** = a declared setting.

---

## Spec

An F-16 against a MiG-29 is the first pairing in the tree in which the two sides are **not the same
aircraft**. [`pilot.md`](pilot.md) gap 2.3 records why that matters: the symmetric F-16 duel is a
stalemate by construction — every long shot is defeated in the notch and the outcome is a coin toss —
so nothing about a decision rule can be learnt from it. The MiG-29 exists to break that symmetry
([`modules/mig29/module.md`](modules/mig29/module.md), "Why it exists at all"). This campaign is the
measurement that says whether it did.

| Contract | Acceptance / measurement anchor |
|---|---|
| The campaign is a FAMILY OF GEOMETRIES, not one fight | head-on, offset, energy advantage to each side in turn, an EMCON probe, a merge; one `.fbm` per geometry, each with its own reading rule in its head |
| Both sides fly the SAME phase machine with their OWN numbers | nothing scripted: no briefed release, no briefed chaff, no briefed manoeuvre. Each jet gets a vector and a master-arm call (the MiG additionally a GCI call where its own sensor geometry needs one) |
| Every outcome is explained by a MEASURED quantity | detection range, lock, Rtr at the moment of the shot, support seconds, miss distance, energy — all `eng_*`/`dmg_*` columns, none of them recomputed by the analysis |
| A duel has ONE winner and ONE loser, or an explained draw | both units declare `kill unit <foe>` + `survive`, so `missions/verdict.md`'s expected-loss rule turns the two verdicts into one |
| A loss caused by an AI DEFECT is not a result | if one side loses because its pilot flies its weapon system badly, the defect is fixed and the geometry re-measured. If it loses because its weapon system is worse, that IS the result |
| The tournament is the cross-check | `fb_tournament.py` runs the same question on MIXED pairings with a fitness written before this campaign existed; a conclusion that only holds in the named missions is not a conclusion |
| Every run is deterministic | one fingerprint over `--threads 1/2/4` × 3 repeats |

### What the campaign is NOT

It is not a claim about the real F-16 versus the real MiG-29. The scale is staggered on purpose
(`modules/mig29/module.md`, "Scale"): the F-16 exactly, sensors and weapon envelopes believable, the
opponent hits its envelope and nothing more. What the campaign measures is whether **FlightBox's own
model of the two systems produces decided engagements for reasons that can be named**.

### Round `D3a` (2026-07-31) — "silent" must stop meaning "blind"

**Added before the change.** The tree already contains the sentence this contract enforces, and the
code contradicts it: `pilot/FBPilot.cpp`'s engagement loop says *"„Radar strahlt nicht" ist eine
gebriefte Taktik (EMCON)"* and deliberately does not abort on it, while the ENTRY gate three hundred
lines earlier does — `CanPressOn` reads `state.Radar.Radiating` and turns the jet for home.

| # | Contract | Acceptance / measurement anchor |
|---|---|---|
| **D3a** | **The press-on test asks for a PICTURE, not for a transmitter.** A live source is the own radar WHILE IT RADIATES, or a report the flight or the controller sent — and `pilot/FBFlightPicture` already assembles exactly those, in every phase, from `FBDatalinkBlock` and `FBRadarBlock`. Nothing new is read, no new block, no new command | **the change must be a NO-OP today**: no mission in the tree ever switches its emission off, so the term is true either way and **all 251 missions stay byte-identical on every column and every exit code**. A single moved value falsifies the claim that this is a precondition rather than a behaviour change |
| **D3c** | **BUILT (2026-07-31): the pilot manages his own emission, and G5 is a key.** The rule is *"radiate ⟺ the nearest point my picture holds is within `emcon_frac × EmconRadiateNm()`, OR I have no other picture at all"* — a lone fighter cannot go silent, because then he goes blind, which is D3's own sentence made structural. Two new virtuals, both defaulting to "this aircraft does not manage emission" (`SilentRadarModeOrdinal() = -1`, `EmconRadiateNm() = 0`), so an airframe that overrides nothing is a compile-time no-op. The F-16 overrides them with `FBF16FcrMode::Off` and the FCR's own 40 nm search gate — **no new capability is claimed**: its module still REJECTS `RadarEmission` as `NotImplemented`, and `Off` sets `Active = false`, which `FBRadarSystem` turns into `Radiating = false` and an EMPTY emitter signature | [MESS] the gene bites where E-22 demanded it: on `w3-09-saturation:f16` two of three EMCON levers move the outcome class, `(16, 10)` → `(20, 12)`. **The pre-round shape is reachable**: `emcon_frac = 3.0` reproduces `(20, 12)`, the value measured before the gene existed. Behaviour: **20 of 251 missions moved, one exit code** (`w4-10-allied-force` 2 → 3); eight missions lose EVERY `FAIL`, two lose `SUCCESS`es. Determinism over `--threads 1/2/4`: identical MD5 |
| **D3b** | **What it unblocks is stated, and is NOT built here.** G5 (`pilot_emcon_frac`) needs a pilot who MANAGES emission; this contract only removes the reason he could never survive doing so | and the acceptance for G5 is already fixed by [`doctrine-evolution.md`](doctrine-evolution.md) `E-22`, before it is designed: its levers must move ≥ 3 of their own on ONE cell. The natural candidate is measured — `w1-07-emcon:f16`, the rung whose own subject is emission discipline, at 5 movers of 21 against a threshold of 7 |

---

## State

**Built and measured.** NINE missions, three AI defects found and fixed, FOUR decided duels, one gap
reproducer. The mixed tournament runs. The ninth mission and the two decided merges are this round's:
the close fight can now employ the round on the rail ([`pilot.md`](pilot.md) §5.11).

### The table: geometry × outcome

Rows 1-8 flown at `c637ed3`+this round, row 9 is this round's, `fb-gym`, default elevation provider. Values are
**viper (F-16) / fulcrum (MiG-29)**; `-` means the channel never got a value.

| # | Mission | exit | detect s | shot s | shot nm | Rtr nm | closest arrival | outcome |
|---|---|---|---|---|---|---|---|---|
| 1 | `duel-headon` | 3 | 24.0 / 105.2 | 156.6 / 155.2 | 9.57 / 10.02 | 9.78 / 10.25 | 7.52 m / 14.96 m | **draw** |
| 2 | `duel-offset` | 3 | 24.0 / 117.2 | 171.8 / – | 9.63 / – | 9.79 / – | 19.4 m / – | **draw** |
| 3 | `duel-viper-high` | 3 | 20.0 / 96.2 | 143.6 / 148.2 | 10.66 / 9.20 | 10.84 / 9.46 | 4.57 m / 262 m | **draw** |
| 4 | `duel-fulcrum-high` | 3 | 20.0 / 102.2 | – / 144.5 | – / 12.27 | – / 15.12 | – / 9.81 m | **draw** |
| 5 | `duel-emcon` | 3 | 24.0 / – | 156.6 / – | 9.57 / – | 9.77 / – | 8.29 m / – | **draw** |
| 6 | `duel-doctrine-mig` | 0 | 24.0 / 105.2 | – / 141.5 | – / 14.41 | – / 10.60 | – / 9.35 m | **MiG wins** |
| 7 | `duel-doctrine-f16` | 0 | 20.0 / 96.2 | 127.9 / 146.8 | 15.65 / 9.93 | 11.30 / 10.10 | 1.77 m / – | **F-16 wins** |
| 8 | `duel-merge` | **0** | 1.9 / 3.8 | – / – | – | – | – / – | **F-16 wins.** Its AIM-9 arrives 1.93 m out at t = 10.5 s → `damage KILL`; the MiG's R-73 arrives 2.64 m out at t = 11.7 s and only degrades. See §row 8 |
| 9 | `duel-merge-stern` | **0** | – / 1.5 | – / 1.5 | – | – | – / 1.86 m | **MiG wins.** The documented R-73 employment (2.2 km astern, 590 m/s closure) → `damage KILL` at t = 21.3 s. The mission this campaign needed so the merge is not one-sided |

**Why each cell came out that way**, one line each, every number off the run:

1. **head-on — draw.** The F-16 detects 81 s earlier (52.1 nm against 26.0), and the lead buys nothing:
   both pilots ship with the same doctrine (shoot at Rtr), and the two Rtrs sit half a mile apart, so
   the triggers fall 1.4 s apart. Two AIM-120s arrive 7.52 m and 8.83 m out; the R-27R runs out of
   flight time 14.96 m out. A 20.5 kg warhead at 7.5 m wrecks avionics and leaves engine, flight
   controls and structure intact.
2. **offset (50° crossing) — draw, for opposite reasons on the two sides.** The F-16 shoots twice and
   both rounds expire 25.5 m and 19.4 m out: a crossing target spends the round's energy on turning.
   The MiG *finds* the F-16 (t=117.2, azimuth +36.5° inside the ZONE the controller gave it) and
   **never locks** — its track drops three times with `coastS=9`, because one look through a ±6°
   elevation bar is all a walking depression angle allows and the 6 s inertial-tracking limit expires
   before the next 3 s frame reaches it.
3. **F-16 with the energy — draw.** 6,000 m and 100 kt move the F-16's Rtr outwards (9.78 → 10.84 nm)
   and the MiG's inwards (10.25 → 9.46); the shot lead goes from −1.4 s to +4.6 s. The AIM-120 arrives
   4.57 m out — three metres closer than head-on, still not a kill. The MiG's R-27R, fired from 6,000 m
   below at a climbing target, misses by 262 m.
4. **MiG with the energy — draw, and the MiG's best default-doctrine run.** Its Rtr goes 10.25 → 15.12
   nm and its Raero 39.2 → 55.7, so it shoots **first** (t=144.5, 12.27 nm) and gets three rounds away
   while the F-16 never fires at all — its own trigger is still 2 nm out when the receiver lights up.
   The two R-27Rs arrive 11.24 m and 9.81 m out against a 13.8 m fuze: inside the ring, outside the
   lethal radius.
5. **EMCON — draw, and the only run in which a MiG survives an AIM-120.** See §2 below.
6. **MiG doctrine — MiG wins.** See §3.
7. **F-16 doctrine + energy — F-16 wins.** See §3.
8. **merge — DECIDED (this round), and by the round on the rail.** The four earlier control-law
   blockers are all closed (the MiG's departure, its acquisition, the F-16's roll law, the MiG's rate
   damper — [`pilot.md`](pilot.md) §5.10/§5.10a, §5.7.3), and what was still missing was an employment
   path: `Phase::Bfm` ended its tick in `BfmGunfire` and the AIM-9/R-73 on the rails had none.
   [`pilot.md`](pilot.md) §5.11 gives it one. `duel-merge` **exit 3 → 0**: both jets fire at the merge
   (viper t = 1.9 s at 7,061 m, fulcrum t = 3.8 s at 6,125 m — the 1.9 s is the F-16 spawning in
   `acm_hud` while the MiG's pilot must switch the N019 to ACM over the bus), and both rounds arrive.
   The AIM-9 at **1.93 m** puts 218,781 J/m² into the centre zone → flight controls FAIL → `damage KILL`
   at t = 10.5 s; the R-73 at **2.64 m** puts 94,388 J/m² in → flight controls DEGRADED, and the F-16
   flies on to `SUCCESS` at t = 68.2 s. The gun is no longer what the merge turns on, but it is not
   silent either: the same fix round corrected the trigger's prediction horizon, and over the 120-run
   merge arena the rounds ON TARGET go **139.8 → 449.1** off **6,318 → 5,850** fired.

9. **merge-stern — the mirror, and the reason it exists.** A close fight only the F-16 can win is a
   defect and not a result, and the head-on merge IS one-sided: [MESS] with the N019 already in ACM the
   MiG shoots FIRST (t = 1.5 against 1.9) and still loses, because the two rounds' kill radii against
   this airframe are **2.32 m (AIM-9M, 9.4 kg)** and **2.08 m (R-73, 7.4 kg)** and a 1,050 m/s head-on
   pass gives the terminal loop the least time to close on either. So the mission asks the question on
   the geometry the R-73 is documented for: 2.2 km astern, co-heading, 40 kt in hand. Closure **590 m/s**
   instead of 1,050, the round arrives **1.86 m** out, and the F-16 dies at t = 21.3 s. **Both cells can
   die in close combat**, and the reason either one does is a warhead mass and an arrival geometry that
   are both in the catalogue.

### The one-sentence answer

**Neither side structurally dominates. The DOCTRINE does.** With both pilots on the same rule — shoot
at Rtr, the range from which the round arrives even if the target turns and runs — five of five BVR
geometries draw, because the two Rtrs are within half a mile of each other and every round then
arrives outside its warhead's lethal radius. Change the rule on either side and the same geometry
decides. What each side needs to decide one is **not** the same, and that difference is the campaign's
real finding:

| | what it takes to decide | why |
|---|---|---|
| MiG-29 | the early launch ALONE, on the flat co-altitude geometry | it carries the LONGER Raero (39.2 nm against 31.1) and the SHORTER Rtr, and it has to illuminate to impact either way — so closing to Rtr buys it nothing it can use |
| F-16 | the early launch AND 6,000 m of energy | [MESS] its early launch alone gets the shot away 10.7 s ahead of the MiG's and still draws: the AIM-120 arrives 4.79 m out, `failed=4088 degraded=8195`, and the MiG stays combat-effective. From 6,000 m higher the identical decision arrives 1.77 m out and kills |

### The three AI defects this campaign found

Each was found by measurement, each is fixed where it belonged, and the effect on the existing tree is
stated. **66 of the 69 stock missions are byte-identical**; the three that moved are named below.

| # | Defect | Where | Effect on stock missions |
|---|---|---|---|
| **M1** | **The GCI entry chain did not retry a refused entry.** Every other briefed input in the tree retries on a bus rejection (`FBPilot::EnterBriefedItems`, `kBriefRetryS`); this one advanced regardless of the acknowledgement, so the ONE entry that makes the N019 exist could be lost to a single g-loaded tick. [MESS, first cut of `duel-emcon`] ILLUM rejected `sequence_precondition` at t=174.2 and the MiG then flew 400 s of the duel blind and never fired | `modules/mig29/FBMig29Pilot.cpp` | **none** — 69/69 byte-identical |
| **M2** | **The antenna was centred on a COASTED look while the jet's own attitude moved.** The reported contact elevation is body-referenced and was measured at the attitude of the look; during a coast the only thing the pilot knows has changed is his own pitch, and he did not take it out. [MESS, first cut of `duel-fulcrum-high`] the N019's ±6° bar froze after ONE look while the MiG descended 6,000 m and gave away 7° of pitch: the contact never came back, coasted 6 s and fell — one track per approach, no shot | `pilot/FBPilot.cpp` (generic) | 3 missions, and only in what the extra antenna slew touches. `bvr-duel`: `cmd_*` counters (+2 issued) plus **29 of 7,000 ticks** in which the viper's `rwr_*` block carries one extra SEARCH-class contact (t=245.0 onwards, bearing −171.6°, i.e. behind it) — search-mode threats are skipped by `mustDefend`, so nothing acts on it. `bvr-duel-decided`: `cmd_*` only (+1). `mig29-intercept`: `cmd_*` only (+1). **No flight-state column, no shot and no verdict changed in any of the three** |
| **M3** | **`FBMig29Pilot::InterceptSpeedKt` was a unit error.** The AP speed loop controls TRUE airspeed; the hook's derivation compared this jet's corner in CAS against the F-16's ROUTE speed (300) instead of its intercept speed (550), and then fed the CAS answer to a TAS command. [MESS] the MiG therefore cruised to every BVR merge at **217 KCAS / M 0.54**, 40 % below its own `BfmMinSpeedKt` of 380 KCAS. Corrected to 600 by the F-16's own documented rule (the TAS whose CAS at the 8,000 m band IS the measured corner): [MESS] 422.3 KCAS / M 1.00 against a measured corner of 420 | `modules/mig29/FBMig29Pilot.h` | 1 mission, `mig29-intercept`: same exit code and same verdict, everything earlier and tighter — designate 58.4 → 52.4 s, shot 60.9 → 54.9 s, kill 87.7 → 78.1 s, miss 1.13 → 0.34 m |

**M3 is also the campaign's second-most-important number**, because of what it did to the table: with
the 330 kt hook the F-16 won four of the five BVR geometries outright. Fixing it turned all four into
draws. **The F-16's apparent BVR dominance was, to that extent, a MiG tuning error and not a weapon
system difference** — which is exactly the class of thing a campaign like this exists to separate.

### Tournament cross-check

`fb_tournament.py` gained one key: `module=` on a variant line (default `f16`, so `variants-bvr.txt`
is unchanged — verified by generating every mission of every pairing from the HEAD script and from
this one: **0 of 60 differ**). `variants-mixed.txt` runs the same three doctrines on both airframes.

Mirror geometry (head-on, co-altitude, co-speed), 30 runs, both seats, `--timeout 420`:

| variant | fitness | outcome | craft | kill | lost | draw |
|---|---|---|---|---|---|---|
| `f16_long` (rtr 1.4, lock 20) | 603.3 | 460.0 | 143.3 | 4 | 0 | 6 |
| `f16_base` | 601.8 | 550.0 | 51.8 | 4 | 0 | 6 |
| `mig_long` (rtr 1.4, lock 16) | 585.0 | 460.0 | 125.0 | 4 | 0 | 6 |
| `f16_deep` (rtr 0.7) | −97.0 | 30.0 | −127.0 | 0 | 0 | 10 |
| `mig_base` | −393.7 | −490.0 | 96.3 | 2 | 6 | 2 |
| `mig_deep` (rtr 0.7) | −850.6 | −930.0 | 79.4 | 0 | 8 | 2 |

The **symmetric** F-16-only tournament on the same arena and the same day: 30 runs, **0 kills and 0
losses across the entire field** — every pairing a draw, the ranking carried by craft terms alone
(`longshot` 308.7 > `baseline` 278.1 > `earlylock` 273.1 > `slowhand` 234.6 > `latelock` −101.9 >
`deepshot` −126.5).

**What shifts, therefore:** the ORDER of the doctrines does not (long > base > deep on both airframes
in both tournaments), but its CONSEQUENCE does. Symmetric, doctrine only orders draws; mixed, it
decides 12 of 30 runs. And the early launch is worth an entire outcome band on the MiG (−393.7 →
+585.0, six losses → none) against essentially nothing on the F-16 (601.8 → 603.3) — the same
asymmetry the named missions found, reproduced by a fitness function written before this campaign
existed.

On the `split` geometry (the 6,000 m / 150 kt energy difference) the picture inverts as expected:
`f16_base` 1085.1 with **7 kills**, every MiG variant negative. Energy is worth more to the side whose
round is fire-and-forget.

**RE-MEASURED under the lexicographic fitness (round `E1`, 2026-07-29, same 30 runs).** The old table's
top two were separated by **1.5 points** with the LARGER "outcome" band on the loser — the defect
[`doctrine-evolution.md`](doctrine-evolution.md) §1.1 calls Exhibit B. Under the new order the two carry
the **identical result** (V = 2.40, M = 1.40 both), because the 900 points of bursts that killed nobody
are not an outcome any more; they are ordered by craft, which is what craft is for.

| variant | fitness (win rate) | V | M | craft | kill / lost / draw |
|---|---|---|---|---|---|
| `mig_long` | **1.000** | 2.40 | 1.40 | +125.0 | 4 / 0 / 6 |
| `f16_long` | 0.800 | 2.40 | 1.40 | +193.3 | 4 / 0 / 6 |
| `f16_base` | 0.600 | 2.40 | 1.40 | +151.8 | 4 / 0 / 6 |
| `mig_base` | 0.400 | 1.60 | 0.60 | +96.3 | 2 / 6 / 2 |
| `f16_deep` | 0.200 | 2.00 | 1.00 | +73.0 | 0 / 0 / 10 |
| `mig_deep` | 0.000 | 1.20 | 0.20 | +79.4 | 0 / 8 / 2 |

The symmetric F-16 field on the same arena now prints its own indictment instead of a ranking:
`latelock` shows **`GATE x8`** — eight of its ten runs never fired and never locked, and the engagement
GATE says so where the old −250 could be repaid by energy plus defence plus support.

### The campaign extension, and the entry-range sweep this family never had

[`campaigns/o4-gaf-mig29g-dact.md`](campaigns/o4-gaf-mig29g-dact.md) re-frames this family around its
historical anchor and was BUILT on 2026-07-29 (`sim/missions/o4-*.fbm`). Three of its results belong
beside the table above rather than in a campaign narrative:

| Finding | Number |
|---|---|
| **The head-on draw of row 1 was a MAGAZINE, not a stalemate.** The same geometry with four AIM-120 instead of two reproduces every sensor and trigger number to within 0.05 nm — and then the F-16 re-merges at t = 540 and kills with its second round | arrival **6.54 m** (no kill) then **2.23 m** (`damage KILL`), `o4-01-bvr-headon` exit 0 |
| **The entry-range sweep**, which this family only ever probed at 8 km and 2.2 km | 10.00 nm **MiG wins** (its shot is offered at t = 1.5 / 17,796 m against the AIM-9's t = 12.5 / 11,963 m — Raero 20,000 against 12,000 m, cueing ±60° against ±30°); 5.00 nm **trade** 0.1 s apart; 2.00 nm **F-16 wins** on 1.40 m against 2.61 m |
| **`D6`'s remaining half NARROWED**: the IR shot a flare defeats, measured against one it does not — inside an engagement, though still not inside a merge (no MAWS) | `o4-07-flares` runs both dispensers dry on the identical briefed schedule: the BVP-30-26 is empty at t = **23.8 s** and the ALE-47 at **42.7 s**. A round arriving in that 18.9 s window detonates **0.0035 m** from the dry MiG and is seduced nine times away from the still-throwing F-16 |

### The flight extension of this campaign

The duels are one against one by construction. Since this round they have a two- and four-ship
counterpart in [`formation.md`](formation.md), and it answers a question the 1v1 cannot: what the
SARH obligation costs a FLIGHT rather than a jet.

| Finding | Number |
|---|---|
| The obligation, side by side on one run (`pair-2v2-asym.fbm`) | R-27R **17.3 s** bound per shot, `eng_pitbull` 0; AIM-120 **0.3 s**, `eng_pitbull` 1. A factor of **58** |
| The rule that keeps one member of a flight free is therefore almost free for the F-16 and would be expensive for the MiG — and the MiG cannot apply it at all | it has no cooperative terminal, so "my leader is bound" has no channel to travel on. Measured F-16 deferral: **7.8 s** (`pair-cover.fbm`); measured MiG deferral: 0 s, by construction |
| The two sorts, on the same 4v4 geometry, as distinct targets per engaged member | cooperative **0.962**, briefed contract **0.750** — one in four contract shooters is doubled up |
| The flight tournament reproduces the doctrine finding one level up | on the `split` geometry `f16_net` scores 940.9 with a flight kill against `f16_solo` 770.0 with none; `mig_pair` (contract) beats `mig_solo` (nothing) by an outcome band on both geometries |

### Determinism

All NINE duels: **one fingerprint** (SHA-256 over all `telemetry*.csv` + normalised `events.log` +
exit code) over `--threads 1/2/4` — re-measured this round for `duel-merge` and `duel-merge-stern`,
one fingerprint each. The mixed tournament's
own `--check-determinism`: 0 of 60 files differ between `--threads 2` and `--threads 1` over 30
pairings.

---

## Gaps

| # | Thing | Known from |
|---|---|---|
| ~~**D1**~~ | **The merge is DECIDED from both sides — closed.** Its five named blockers are gone in order: the MiG's close-combat departure (`pilot.md` §5.10), its acquisition (`FBMig29Radar::kAcm*`), the F-16's roll law (`pilot.md` §5.7.3), the MiG's missing rate damper (`pilot.md` §5.10a) and — this round — the missing WEAPON path (`pilot.md` §5.11: `Phase::Bfm` ended its tick in `BfmGunfire` and the rounds on the rails had no employment). `duel-merge` **exit 3 → 0** and `duel-merge-stern` **exit 0**; over the 120-run merge arena the outcome classes go from **60/60 (2,1)** on all three cells to `xmerge` **30 (3,2) + 30 (1,0)** — every run decided. What the closure did NOT fix, and is now `pilot.md` 2.9 alone: both fights are decided on the FIRST pass, because neither ACM box re-acquires afterwards. |
| **D2** | **An AIM-120's terminal miss is a strong function of closure**, and nothing in the tree says whether that is the round or the physics. [MESS, `duel-headon` with the MiG's cruise swept 330→600 kt TAS] target speed 169/206/237/268/288 m/s ⇒ closure 744/842/919/1000/1053 m/s ⇒ miss **1.37/2.13/4.74/3.15/7.66 m**. Since `core/FBDamageModel` is 1/r², those six metres are the difference between a kill and a jet that flies on with wrecked avionics — i.e. the single most outcome-sensitive number in the whole campaign. It belongs beside `bvr-duel-decided`'s terminal-loop finding | this campaign |
| **D3a** | **LOCATED (`E9`, 2026-07-31) — the line that makes "silent" mean "blind" is `pilot/FBPilot.cpp:765`.** `CanPressOn` reads `bool sensor = state.Radar.H.Readable() && state.Radar.Radiating`, so a pilot who switches the emission off decides in the same tick that he cannot continue the mission, and turns for home. The command path he would need is already complete and unused by him: `FBCommandTarget::RadarEmission` exists, `FBMig29Emission::Off` exists, both modules honour it, and `FBMig29Pilot` already posts `Illum` on a GCI cue. **What each airframe would fly silent on differs and both sources are already published:** the MiG-29 on `FBIrstBlock` (angle only, no range, no identity — `core/FBIrstContact.h`), the F-16 on `FBDatalinkBlock`/`FBNetLinkBlock`. **The acceptance is falsifiable in advance** ([`doctrine-evolution.md`](doctrine-evolution.md) `E-22`): G5's levers must move ≥ 3 of their own on ONE cell, or they raise S2's bar by `k/3` and change no verdict. Today's best cell is `w3-09-saturation:f16` at 6 movers of 21 against a threshold of 7 — deficit **1** | `E9` |
| **D3** | *(blocks G5 of [`doctrine-evolution.md`](doctrine-evolution.md); `tools/fb_evolve.py` prints the blocker at start and refuses the gene. **CONFIRMED as a hard block, round `E2`:** `set pilot_emcon_frac 0.5` gets `SET_INVALID_VALUE` + `SET_REJECTED` at t = 0.0 and the run **exits 1** — the key does not exist, so the degenerate band is not even reachable.)* **The pilot does not use the IRST.** `sensors/FBIrstSystem` publishes an `Irst` block and the only consumer in the tree is a missile seeker; `pilot/FBPilot`'s intercept picture is built from the Radar block alone. So the MiG's one genuinely passive sensor cannot cue anything, and "IRST-EMCON" is a doctrine the campaign could only test as "silent and blind" | `duel-emcon` |
| **D4** | **Weapon selection is not a decision the pilot can make.** `FBCommandTarget::WeaponSelect` is `NotImplemented` on both modules, so the selected station is whatever the SMS's station step arrived at. A jet carrying an AIM-120 and an AIM-9 will offer whichever pylon comes first in the module's own list, and the missions in this family work around it by loading the racks in the order they want the rounds fired | this campaign |
| ~~**D5**~~ | ~~The MiG has no dispenser.~~ — **closed this round.** The MiG has the BVP-30-26 (`modules/mig29/FBMig29Cmds`, 60 cartridges, [SET] 30/30 split), and its flares seduce the AIM-9 through the SAME deterministic model that seduces the R-73 (`sensors/FBIrstSystem::SelectFlare`): `mig29-defend.fbm` measures `FLARE_SEDUCED tgtIntensity=0.16` and the round expiring 16.0 m wide, against an astern control that detonates 0.04 m out. The defensive asymmetry is now TWO-SIDED. What it does NOT yet do is auto-defend in the merge (the 9-12 has no MAWS, defence-rwr-cm.md §5, so an infrared shot is answered only by a briefed throw), and the BVR duels do not arm it, so their outcomes are unchanged (only the MiG's `cmd_*` bookkeeping moves: its intercept CmDispense is no longer rejected `NotImplemented`) | this round |
| **D7** | The campaign is 1v1 and the flight campaign is 2v2/4v4, and the two do not share a geometry table. The four-ship in `formation.md` is one geometry; the eight duel geometries have no flight counterpart, so nothing says whether the energy split or the offset behaves the same with two aircraft a side | this round |
| **D6** | ~~The campaign measures BVR only.~~ — **HALVED, and the remaining half NARROWED on 2026-07-29.** `o4-07-flares.fbm` (O4) measures an IR shot a flare defeats and one it does not, in one file, at the same aspect, with the magazine as the only difference — so the DEFENDED infrared shot is no longer untested. What is still untested is that shot **inside a merge**, and the blocker is unchanged and structural: neither jet has a MAWS, so an infrared launch can only be answered by a BRIEFED throw, which a merging pilot cannot schedule. Was: The IR shot is exercised and it DECIDES: `duel-merge` (head-on, F-16 wins on 1.93 m) and `duel-merge-stern` (rear quarter, MiG wins on 1.86 m). The gun is exercised too and no longer wastes its drum ([`pilot.md`](pilot.md) §5.8: merge-arena rounds on target 139.8 → 449.1), but it still decides nothing — it needs 17.0 landed 30 mm rounds and delivers 9.53. neither jet auto-dispenses against an infrared launch (the F-16 has no MAWS in the tree either, `defence-rwr-cm.md` §5), so the two decided merges are undefended shots | this campaign, + O4 |

### Rejected / not attempted, with the measurement

| Approach | Why |
|---|---|
| Gating the WVR shot on Rtr, as the intercept phase does | Rtr means "the round arrives even if he turns and runs". Inside two miles that is not the question, and the number is not the answer either: [MESS] the AIM-9's stored table gives `rtrM` **22,740 m** at a 3.2 km launch and the R-73's **16,212 m**, because the integration runs until the round drops below `MinSpeed`. For a WVR round Rtr and Raero are both vacuous and only **Rmin** binds. Declining Rtr is therefore not a relaxation; adopting it would have been a gate that measures nothing |
| A load-factor gate on the WVR launch | there is no mechanism to gate on — [`pilot.md`](pilot.md) 2.13. A round leaves with the launcher's attitude and velocity and the release path models no rail load, so every candidate number would have been fitted |
| Raising `core/FBDamageModel`'s fragility ladder so the gun kills | it is four `[SET]` numbers that also price every warhead in the tree, and the measurement says they are not the limiter: a perfectly aimed GSh-301 drum at the range the merge is fought at lands **20.2** rounds against a **17.0**-round threshold. The gun's problem was the aim, and the aim had a derivation error in it ([`pilot.md`](pilot.md) §5.8) |
| Making `InterceptShotRtrFactor` > 1.0 the MiG's DEFAULT hook | it wins, and it is not derivable. Rtr means "the round arrives even if he runs"; nothing in `doc/modules/mig29/` states a launch doctrine, so a number chosen because it wins a duel would be a fitted constant wearing a derivation's clothes. It lives where the tree already puts a doctrine: in mission text (`set pilot_shot_rtr`) and in the tournament |
| Reading `duel-emcon` as "EMCON is better" | it is not: the MiG survived and never fired. The run measures a TRADE (warning + defence + life against blindness), and both halves are in its head |
| ~~A merge mission as a real duel~~ | **built, and since this round DECIDED on both sides.** The five earlier blockers are closed in order: the MiG's departure, its acquisition, the F-16's roll law, the MiG's rate damper, and — this round — the missing employment path ([`pilot.md`](pilot.md) §5.11). `duel-merge` exit 3 → 0 (F-16, AIM-9 at 1.93 m), `duel-merge-stern` exit 0 (MiG, R-73 at 1.86 m). The WEAPON thesis is measured at last, and its answer is not the one the mission header guessed: it is the WARHEAD, not the gun and not the turn rate. What is still true of the old entry is the re-acquisition — the fight is decided on the FIRST pass, and neither ACM box finds the other again afterwards (`pilot.md` 2.9) |

---

## Knowledge

### 1. The four asymmetries, each with its number

None of these is mission data — they are what the two modules ARE.

| Asymmetry | F-16 | MiG-29 | Where it comes from |
|---|---|---|---|
| **Radar reach against the OTHER aircraft** | 40 nm gate × (4.0/1.2)^¼ = **100.1 km** | 27 nm gate × (1.2/1.2)^¼ = **50.0 km** | `sensors/FBRadarSystem::GateRangeM`, `kRefRcsM2` = 1.2 (the F-16's own, so every F-16-vs-F-16 measurement stays byte-identical) |
| **Search bar** | ±60° az, ±10.5° el, 40 nm | ±30° az about a three-position ZONE switch, **±6° el**, 3.0 s frame, 6 s inertial coast | `modules/f16/FBF16Fcr`, `modules/mig29/FBMig29Radar` |
| **Weapon obligation** | AIM-120: ends at the activation ring (`ttaS` 4.75–9.25 s [MESS]) | R-27R: **runs to impact** (`ttaS` = −1); `SupportInhibitsDefend` makes the phase machine obey it | `core/FBStore.h`, `pilot/FBPilot` |
| **Launch zone shape** | Raero 31.1 nm / Rtr 9.78 nm head-on [MESS `duel-headon`] | Raero **39.2** nm / Rtr **10.25** nm | `weapons/FBLaunchZone` integration from each launcher's own velocity |
| **Warhead** | 20.5 kg, 10 m fuze | 39 kg, 13.8 m fuze | `core/FBStore.h`. [MESS] an R-27R **9.35 m** out kills (`duel-doctrine-mig`); an AIM-120 **7.52 m** out does not (`duel-headon`) |
| **Countermeasures** | ALE-47, 60/60 | **BVP-30-26, 30/30** (this round) | `modules/f16/FBF16Cmds`, `modules/mig29/FBMig29Cmds`. Both sets' flares seduce an IR seeker through `sensors/FBIrstSystem::SelectFlare`; the asymmetry is now the magazine size, not presence |

### 2. The EMCON timeline, measured

`duel-emcon` against `duel-headon`, same geometry, one switch different. The whole point of the
mission is this table.

| t | `duel-headon` (N019 ILLUM from t=0) | `duel-emcon` (N019 OFF, commit call at t=170) |
|---|---|---|
| 0.1 | MiG's SPO-15: **blind forward** — its own transmitter blanks the hemisphere the F-16 is in | MiG's SPO-15 already has the APG-68 |
| 16.9 | F-16's ALR-56M has the MiG — **7.1 s before its own radar finds anything** | nothing to hear |
| 105.2 | MiG's first radar contact (26.0 nm) | — (never radiates, never detects) |
| 140.8 | — | MiG's missile-launch warning; answers in 1.1 s, beams for 118.9 s |
| 147.9 / 155.3 | F-16's threat / launch warning (the MiG's lock, then its R-27R) | — |
| 173.2 | — | AIM-120 arrives **8.29 m** out (against 7.52 m in `duel-headon` — the beam is the difference). 50.5 kJ/m² in the NOSE zone: air data, radar altimeter, nav, radar, fire control, datalink all **failed**; engine, flight controls, structure intact, `dmg_effective` = 1 |
| 174.2 | — | the controller's commit call lands INTO the defensive turn. ILLUM refused `sequence_precondition` (manoeuvre lock) |
| 176.3 | — | the pilot retries (**M1**). Refused `system_failed`: the box no longer exists |
| end | MiG dead in the M2 build, drawing in the final one | MiG **alive**, blind, its N019 destroyed, never fired a round |

Two structural facts fall out of it, and neither was designed:

- **Radiating costs this aircraft its own warning receiver in the sector that matters.** The SPO-15's
  forward blanking (`modules/mig29/FBMig29Rwr::Blanked`, ±90° while the own set radiates) means a
  head-on MiG that illuminates cannot hear the radar working it. In `duel-headon` its `eng_threat_s`
  is **−1** for the entire run: it never knew, and never defended.
- **Going loud and defending are mutually exclusive**, and not because anybody wrote that rule.
  `RadarEmission` is a DED-class command; the bus locks head-down inputs while the jet is manoeuvring
  (`core/FBCommandBus`, `kDedMaxG`). A MiG that is beaming a launch cannot switch its radar on.

### 3. The two decided duels, side by side

Both are the same file as a geometry above plus two `set pilot_*` lines. No code differs between them
and the drawn runs.

| | `duel-doctrine-mig` | `duel-doctrine-f16` |
|---|---|---|
| geometry | `duel-headon` (co-altitude, co-speed) | `duel-viper-high` (12,000 m / 500 kt vs 6,000 m / 400 kt) |
| variant | fulcrum `pilot_shot_rtr 1.4`, `pilot_lock_nm 16` | viper `pilot_shot_rtr 1.4`, `pilot_lock_nm 20` |
| designation | MiG t=138.8 (15.43 nm), F-16 t=140.7 | F-16 t=116.7, MiG t=135.8 |
| shot | MiG t=141.5, **14.41 nm** at Rtr 10.60 | F-16 t=127.9, **15.65 nm** at Rtr 11.30 |
| the loser's shot | **never taken** — its receiver lit at t=138.9, it answered in 1.1 s and defended for 457.4 s, spending all 60 chaff | MiG shot at t=146.8 (9.93 nm), 18.9 s late; its shooter died 7.6 s later and the round was still flying its last information when the run ended |
| support | 25.8 s of unbroken illumination, `eng_support_f` 0 (a SARH window is the whole time of flight and never completes) | 9.8 s, `eng_pitbull` 1 |
| arrival | R-27R **9.35 m**, aspect −94.4° (into the beam) → `damage KILL`, `failed=1020` | AIM-120 **1.77 m** → `damage KILL`, `failed=16383` |
| the price | 25.8 s pointing at somebody who can see it doing so | 24 chaff and 16.8 s defensive against the MiG's answering shot |

**Why the early launch works at all**, stated as the mechanism rather than the outcome: the trigger is
not the only thing it moves. A launch puts a missile symbol on the other jet's receiver, and
`pilot/FBPilot`'s rule is that a **seeker** on one's own aircraft is never negotiable (§7.2). The
victim therefore turns before reaching its own Rtr, and the shot it would have taken never happens.
[MESS] on `duel-headon` the margin is **1.5 s**: the F-16's own early-launch doctrine fires at
t=144.2 when it is left alone (`long/base`), and against the MiG's early launch it goes defensive at
t=142.7 (`long/long`) and never gets there.

### 4. The geometry sweeps behind the table

**Cruise speed (M3's evidence)** — `duel-headon`, `set pilot_speed_kt` on the fulcrum, everything else
fixed:

| commanded TAS | mean CAS | Mach | throttle | fuel | MiG shot | outcome |
|---|---|---|---|---|---|---|
| 330 (the old hook) | 217.2 | 0.541 | 0.28 | 3,039 lb/h | t=199.2, 8.39 nm | F-16 wins |
| 460 | 315.0 | 0.768 | 0.31 | 3,107 | t=177.0, 9.29 nm | F-16 wins |
| 520 | 360.1 | 0.868 | 0.34 | 3,670 | t=167.8, 9.58 nm | F-16 wins |
| 560 | 390.9 | 0.935 | 0.39 | 4,738 | t=161.4, 9.81 nm | **MiG wins** |
| **600 (the derived hook)** | **422.3** | 1.002 | 0.45 | 6,325 | t=155.2, 10.02 nm | draw |
| 640 | 454.1 | 1.069 | 0.49 | 8,050 | t=149.2, 10.29 nm | draw |

The hook is set to **600 because that is what the rule gives** (the F-16's own: the TAS whose CAS at
8,000 m is the measured corner; 422.3 against a corner of 420), not because of what it wins — 560
wins and is not derivable from anything.

**Launch doctrine** — `duel-headon`, `pilot_shot_rtr` on the fulcrum with `pilot_lock_nm` moved to
match (a shot cannot be taken from further out than the lock it rides on):

| factor / lock | MiG shot | outcome |
|---|---|---|
| 1.0 / 13.5 (default) | t=155.2, 10.02 nm | draw |
| 1.2 / 13.5 | t=150.5, 11.54 nm | **MiG wins** |
| 1.4 / 16 | t=141.5, 14.41 nm | **MiG wins** |
| 1.6 / 18 | t=135.5, 16.32 nm | **MiG wins** |
| 1.8 / 20 | t=129.5, 18.20 nm | draw (the shot is now defeatable) |
| 2.5 / 26 | t=108.5, 24.89 nm | draw |

The window is real and it has both edges: too early and the round expires (at 2.5×Rtr its own
`closestM` runs to tens of metres), too late and the F-16's shot arrives first.

**Doctrine matrix**, `{F-16, MiG} × {default, early}`, three geometries:

| geometry | base/base | long/base | base/long | long/long |
|---|---|---|---|---|
| head-on | draw | draw | **MiG** | **MiG** |
| F-16 high | draw | **F-16** | **F-16** | draw |
| MiG high | draw | draw | draw | draw |
| 50° offset | draw | draw | draw | draw |

`long/long` on the F-16-high geometry drawing while both single-sided versions decide is the honest
shape of the thing: when both sides move their trigger outwards, both are defensive before either is
in parameters, and the engagement dissolves.

### 5. How to run the campaign

```
make -C sim gym
for m in headon offset viper-high fulcrum-high emcon doctrine-mig doctrine-f16 merge merge-stern; do
    build/fb-gym --mission missions/duel-$m.fbm --out /tmp/duel/$m
done
tools/fb_duel_report.py --table /tmp/duel/*          # geometry x outcome
tools/fb_duel_report.py /tmp/duel/headon             # both sides' eng_* + EMCON spans + timeline
tools/fb_tournament.py --variants tools/variants-mixed.txt --out /tmp/tourney --geometry mirror \
                       --timeout 420 --check-determinism
```

`fb_duel_report.py` recomputes nothing: the `eng_*` block is the last line of each `telemetry*.csv`
(those channels survive the engagement, `pilot.md` §8), the radiating spans are the `fcr_on`/`n019_on`
columns, and the timeline is `events.log`. Weapon units are skipped by the presence of an `msl_*`
column — a launched round is an `FBSimUnit` with an `FBPilot`-derived guidance, so it carries `eng_*`
too.
