# AR — the arena: ten measuring rungs for the doctrine evolution

**What this file is:** the spec of an **instrument**, not of a campaign. `mods/f16/src/campaigns/ar-arena.fbc`
plus `mods/f16/src/missions/ar-01-*.fbm` … `ar-10-*.fbm` are ten synthetic cells built to the four properties
[`../doctrine-evolution.md`](../doctrine-evolution.md) round `E11` **measured** a cell must carry before
it can be informative — and to nothing else.

**It is deliberately NOT one of the ten campaigns of [`INDEX.md`](INDEX.md), and it does not appear in
that index.** Those ten are scenario specifications with cited anchors, and INDEX.md's own sentence is
the reason this one stands apart: *"a campaign without a cited anchor is a mood, and a mood cannot be
measured."* This file has **no anchor and claims none**. There is nothing here to be faithful to: the
units, the coordinates, the clock and the atmosphere are instrument settings, and every one of them is
labelled `[SET]`, `[DERIVED]` or `[MESS]` in the mission file that carries it.

**Status: BUILT AND FLOWN 2026-07-31** (tree `8cd3a74`). Ten of ten run, all ten publish both sides'
objective vectors, all ten are deterministic over `--threads 1/2/4`, and **8 of 10 carry property 4 on
both graded sides at S7's own threshold of zero flips**. §State has every number; the two that do not
are named there and in their own headers rather than repaired.

| Source class | What it is | Where |
|---|---|---|
| **The requirement** | the four properties, each one a measurement over 154 committed cells | [`../doctrine-evolution.md`](../doctrine-evolution.md) §State `E11` §§2, 4, 6, 9, 10 |
| **The gate this feeds** | S1/S2/S7 and the (V, M) outcome class | `sim/tools/fb_campaign_arena.py`, `sim/tools/fb_fitness.py` |
| **FlightBox sources** | what a `.fbm` may declare | [`../missions/syntax.md`](../missions/syntax.md), [`../missions/verdict.md`](../missions/verdict.md), [`../missions/weather.md`](../missions/weather.md), [`../missions/campaign.md`](../missions/campaign.md) |

---

## Spec

### 1. The problem this is the answer to, stated as `E11`'s own numbers

`E11` flew all **154** cells of the campaign breadth under **24** levers (3,850 runs) and closed on a
diagnosis in which not one row is about the genome or the gate:

| a cell can be informative only if it is | in the campaign breadth |
|---|---|
| **large** — ≥ 4 aircraft on the graded side (`E11` §6) | 44 of 154 |
| **opposed** — a hostile fighter in the file (`E11` §9) | 36 of those 44 |
| **visible to level M** — objectives the run can reach (`E11` §10) | 13 cells carry `M = 0` on all 24 levers and **not one has a single mover** |
| **robust** — 0 flips of its own baseline over the ±3 m spawn grid (`E11` §2, `E-21`) | of the top twelve by movers, ten are; **the two most gradable are not** |
| and then reach S2's 8 movers of 24 | **1 of 36** — and it is one of the two chaotic ones |

S5 wants **three** informative cells. The breadth supplies **zero**. `E11` §8 then located the cause and
explicitly cleared the campaigns of blame: every campaign is a **ladder**, 107 of 154 cells are its lower
rungs at one or two aircraft, and *"ten ladders produce ten tops"* — only two rungs in the whole breadth
put eight or more aircraft on the graded side.

**So the missing thing is rungs, not genes** (`E11` §7: *"There is no gene left to add"*). This arena
adds ten, and it adds them **beside** the campaigns:

> Editing a committed rung **because** it would raise a mover count is selecting the arena on the result,
> which `E11` §4 forbids and doc/doctrine-evolution.md has declined five times. A new, separately named,
> openly synthetic arena is the only way to add rungs without touching that rule — which is why this file
> says what it is in its first sentence and in the header of every one of its eleven files.

### 2. The four properties, and how each rung carries them

| # | Property | How every `ar-*` rung carries it |
|---|---|---|
| **1** | **LARGE** | **8 F-16 and 8 MiG-29**, each in **two flights of four**. Four seats deep is not cosmetic: `aftM = element × trail` with `element = (position−1)/2`, so a pair has no second element and **two of G1's six levers are structurally unreachable on it** (`E11` §4 measured exactly that on `o5-09-night-two`) |
| **2** | **OPPOSED** | eight MiG-29 with R-27R + 3 × R-73, `n019 rad` at 54 nm, `n019_emission illum`, three GCI calls each, on the **graded** hostile team. `E11` §9: the eight large unopposed cells never exceed 2 movers, because an air doctrine has nobody to be a doctrine against |
| **3** | **VISIBLE TO LEVEL M** | **two objectives on each of the 16 aircraft**: one contested `kill unit <named>` and one deferred `survive`. `kill team` is refused throughout — on these files it would require every ground unit dead as well and could therefore never be met, which is exactly the invisibility `E11` §10 measured. **Measured:** every rung publishes 32 `mission OBJECTIVE` lines and `M ≥ 8` on **both** sides |
| **4** | **ROBUST** | measured per rung with `fb_campaign_arena.py`'s own instrument — the ±3 m spawn-longitude grid `kChaosSteps`, 8 samples, applied to the cell's own aircraft, compared against the cell's own baseline `(V, M)`. Published pass or fail in §State and in each rung's header |

### 3. The two axes the ten rungs span, and what the set is NOT

Five entry **geometries** × ten distinct **environments** (clock and atmosphere):

| rung | geometry | red spawn / heading | clock (UTC) | sun el [MESS] | atmosphere |
|---|---|---|---|---:|---|
| 01 headon-noon | **A** head-on | 1.20 E / 270 | 12:00 | +64.16° | calm |
| 02 headon-night | A head-on | 1.20 E / 270 | 00:30 | −25.59° | `wind 335 19.6` |
| 03 offset-morning | **B** offset | 1.20 E, +0.22 lat / 250 | 08:00 | +33.34° | GFS fixture |
| 04 offset-night | B offset | 1.20 E, +0.22 lat / 250 | 22:00 | −19.83° | calm |
| 05 beam-dawn | **C** beam | 0.58 E, +0.35 lat / 180 | 04:40 | −0.81° | `wind 315 15.9` |
| 06 beam-afternoon | C beam | 0.58 E, +0.35 lat / 180 | 16:30 | +30.30° | GFS fixture |
| 07 close-dusk | **D** close | 0.90 E / 270 | 19:20 | +1.15° | calm |
| 08 close-day | D close | 0.90 E / 270 | 10:00 | +53.23° | GFS fixture |
| 09 vertical-predawn | **E** vertical | 1.20 E / 270, 10–11.5 km | 03:00 | −14.68° | `wind 359.6 25.6` |
| 10 vertical-evening | E vertical | 1.20 E / 270, 10–11.5 km | 18:00 | +14.48° | calm |

**NO RUNG IS A CONTROL OF ANY OTHER.** Two coordinates differ between any pair, by construction. The set
is a **spread**, not a factorial: its job is to hand the evolution ten *independent* cells, never to
attribute an effect to daylight or to wind. A reader who reads a light effect out of two rungs has read a
geometry change as well, and the mission headers say so.

Five of the ten sun elevations are **below or on the horizon** (−25.59, −19.83, −14.68, −0.81, +1.15) and
five are above (+14.48, +30.30, +33.34, +53.23, +64.16); the set contains astronomical night, nautical
twilight, both horizon crossings and solar noon. Four rungs are calm, three carry a closed-form wind and
three carry the committed GFS blob.

### 4. The range, and why its coordinates are honest

**45.00000 N / 000.00000 E** `[SET]`. A coordinate decides exactly two things in this tree — **where the
sun is** and **how many metres a degree is worth** — because `--elev const` puts a 0 m datum under
everything and there is no terrain masking (gap `C4`). This one makes both checkable by eye:

- on the prime meridian **UTC is local apparent solar time**, so `time 12:00:00Z` *is* solar noon by
  construction and the measured +64.16° is a number a reader can re-derive;
- scale: 1° latitude = 111 132 m, 1° longitude = 111 320 × cos 45° = **78 715 m** `[DERIVED]`, the same
  cosine `fb_campaign_arena.chaos_flips` uses for its own perturbation.

The rung is **result-neutral in the elevation source**: 45 N / 0 E lies outside the baked Swiss DEM
island, and `[MESS]` `--elev const` and the gym's default `--elev swiss` produce the same duration, the
same exit code and the same objective vector.

### 5. Every rung is air AND ground, and the ground half is graded where it is not a coin

| unit | what it is | graded? |
|---|---|---|
| `arNNts1…4` | `target_soft`, the four aim points | **yes** — one `objective kill unit` per striker |
| `arNNth1/2` | `target_hard` | **no, deliberately** |
| `arNNnod` | P-18, `emcon free` | no — the net's node and the only ground emitter |
| `arNNsam` / `arNNaaa` | SA-6 (3 rounds) / ZSU-23-4 (40), both `emcon hold` | no |
| `arNNnet` | wire, period 4.0, hold 3, `wcs free`, fire units on `autonomy hold` | — |

**Why the hard targets carry no objective, derived rather than asserted.** The Mk 84's own fragment law
is `flux(r) = 2.81e7 / r²` J/m² (`mods/f16/src/missions/mk84-radius.fbm`), `target_hard`'s structure fails at
`9.0e4` and `target_soft`'s at `2.8e3` (`sim/src/modules/ground/FBGroundTarget.h`). So

    hard: r_fail = sqrt(2.81e7 / 9.0e4) = 17.7 m       soft: r_fail = sqrt(2.81e7 / 2.8e3) = 100.2 m

against a CCRP delivery this tree lands at **31.7…87.0 m** over the ten rungs `[MESS]`. A graded
objective on a hard target would be a **coin**, and property 4 forbids putting the fitness on one. The
soft aim points are reached with **13…68 m of margin** in every rung, and all 40 of them are destroyed.

**A FlightBox battery has no IFF interrogator**, so what the belt shoots at is part of the measurement
and not a briefing error — every rung fires exactly 3 `site LAUNCH` and the reading rule says to read
`brgDeg` before anything else.

### 6. The callsign topology, and why the carry is inert on purpose

Every callsign is prefixed with its own rung (`ar01*` … `ar10*`), so the ten casts are **pairwise
disjoint in every unit they can lose**. `carry units ground stores` is declared (the default, written
out) and has nothing to apply. Three consequences, and all three are what an arena needs:

1. each rung is measurable **standalone**, which is how `fb_campaign_arena.py` flies a cell;
2. campaign step *k* is byte-identical to its own standalone run — [`../missions/campaign.md`](../missions/campaign.md)
   §5 criterion 2 holds **by construction** rather than by measurement;
3. `stop_on never`, so a rung that ends badly cannot delete the nine measurements behind it.

---

## State

### 1. The ten rungs as flown (`fb-gym --mission … --threads 1 --elev const`, tree `8cd3a74`)

| rung | exit | run s | releases | aimErrM | soft killed | R-27/R-73 | AIM-120/9 | friendly:f16 (V,M) | hostile:mig29 (V,M) |
|---|---:|---:|---:|---|---:|---:|---:|---|---|
| 01 headon-noon | 3 | 600.0 | 4/4 | 31.7 / 42.2 / 51.5 / 40.7 | 4/4 | 18 | 0 | (20, 12) | (16, 8) |
| 02 headon-night | 3 | 600.0 | 4/4 | 67.6 / 70.5 / 79.3 / 79.3 | 4/4 | 20 | 0 | (20, 12) | (16, 8) |
| 03 offset-morning | 3 | 600.0 | 4/4 | 74.6 / 87.0 / 74.3 / 83.4 | 4/4 | 10 | 0 | (20, 12) | (16, 8) |
| 04 offset-night | 3 | 600.0 | 4/4 | 31.7 / 42.2 / 51.5 / 40.7 | 4/4 | 5 | 0 | (20, 12) | (16, 8) |
| 05 beam-dawn | 3 | 144.4 | 4/4 | 75.3 / 84.0 / 74.8 / 86.5 | 4/4 | 5 | 2 | (19, 11) | (17, 9) |
| 06 beam-afternoon | 3 | 139.1 | 4/4 | 74.6 / 87.0 / 74.3 / 83.4 | 4/4 | 6 | 2 | (19, 11) | (17, 9) |
| 07 close-dusk | 3 | 140.7 | 4/4 | 31.7 / 42.2 / 51.5 / 40.7 | 4/4 | 12 | 2 | (19, 11) | (17, 9) |
| 08 close-day | 3 | 138.0 | 4/4 | 74.6 / 87.0 / 74.3 / 83.4 | 4/4 | 11 | 3 | (18, 10) | (18, 10) |
| 09 vertical-predawn | 3 | 600.0 | 4/4 | 64.8 / 69.5 / 66.5 / 72.8 | 4/4 | 21 | 0 | (20, 12) | (16, 8) |
| 10 vertical-evening | 3 | 600.0 | 4/4 | 31.7 / 42.2 / 51.5 / 40.7 | 4/4 | 11 | 0 | (20, 12) | (16, 8) |

**Exit 3 (TIMEOUT) on all ten is the arena's normal end and is not a verdict.** Every rung publishes
**32** `mission OBJECTIVE` lines — 16 aircraft × 2 — so property 3 is carried by measurement and not by
declaration, and `M` runs 8…12 of a possible 16 on each side.

### 2. Property 4, measured: 8 of 10 rungs, 16 of 20 graded cells, at zero flips

Instrument: `fb_campaign_arena.py`'s own `kChaosSteps` = ±3.0 / ±2.2 / ±1.4 / ±0.6 m of spawn longitude,
8 samples, applied to the graded side's own aircraft, against that side's own baseline `(V, M)`.
Threshold `kChaosMaxFlips = 0`.

| flips of 8 | rungs |
|---|---|
| **0 on both graded sides** | **01, 02, 03, 04, 06, 07, 08, 09** — eight rungs, sixteen cells |
| 1 on each side | **05 beam-dawn** |
| 2 on each side | **10 vertical-evening** |

**The two failures are published, not repaired**, and each has a sibling that isolates the cause:
`ar-05` and `ar-06` fly the **identical geometry C** and `ar-09` and `ar-10` the **identical geometry E**,
and in both pairs one member is clean at 0 of 8. So the coin sits in the rung's own air, not in the
geometry — and a rung edited until its baseline stops moving would be a rung selected on its own result,
which is the move `E11` §4 refuses.

`E11` §4 asked whether gradable and robust fight each other and measured that they do not. This arena
says the same from the other side: **rungs 06, 07 and 08 lose aircraft on both teams and still flip 0 of
8**, while `ar-10`, which loses nobody at baseline, flips 2.

### 3. Determinism and the campaign

| Check | Requirement | Measured |
|---|---|---|
| Each rung over `--threads 1/2/4` | one telemetry fingerprint | **10/10: one MD5 over all `telemetry*.csv` per rung, identical at 1, 2 and 4**; identical exit code at all three |
| `fb-gym --campaign campaigns/ar-arena.fbc --threads 1` | runs, ten steps | see §State 4 |
| `git status --porcelain mods/f16/src/missions mods/f16/src` | no existing file modified | **ten `??` lines and nothing else** |
| Build | `make -C sim core-lib gym` warning-free | `libfbcore.a` (86 objects), `fb-gym` with 0 Dawn/WebGPU symbols |

### 4. One gradability probe, reported as a probe and not as a gate result

The arena is **not** tuned toward a mover count — `E11` §4's rule — and no lever sweep was run on it in
this round. One probe was run, to show that the cells are not inert:

| | friendly:f16 | hostile:mig29 | AIM-120 off the rail |
|---|---|---|---|
| `ar-01` baseline | (20, 12) | (16, 8) | 0 |
| `ar-01` + `set pilot_emcon_frac 3.0` on the sweep (G5's `emcon-wide`) | **(22, 14)** | **(14, 6)** | **4** |

One lever, both graded cells moved. The mechanism is published in the run: the baseline sweep commands
**radar STANDBY at t ≈ 6 s** under its own emission discipline and never re-radiates, so it never
develops a launch solution; `emcon-wide` keeps it hot. That is the room the gene has on this arena, and
it is reported rather than removed.

### 5. What was measured and thrown away during construction, with its number

Kept here because a measured dead end is knowledge:

| Attempt | Measurement | Why it was dropped |
|---|---|---|
| strike block at 5 000 m | the fixture and wind rungs deliver at **89…108 m** against a 100.2 m soft fail radius; `ar-05` left one aim point standing at **107.978 m** | the ground half became a coin — a property-4 failure manufactured by the author. The block moved to 2 500…2 800 m, where the same rungs deliver at 64…87 m |
| geometry D at 0.75 E | the merge truncates the run at **t = 95.3 s with 0 of 4 bombs away**, ground objectives unreachable | a property-3 failure. D moved to 0.90 E: pass at t = 140 s, 4/4 released |
| geometry C at 0.90 E / +0.45 lat | the two tracks never intersect inside the timeout | property 2 in substance: hostile fighters in the file that never fight are not opposition. C moved to 0.58 E / +0.35 lat |
| MiG `n019_emission off` instead of `illum` | **0 AIM-120 off the rail**, unchanged | the blue sweep's radar standby is **not** caused by the MiGs radiating; the hypothesis was wrong and the setting was left where it was |

---

## Gaps

| # | Thing |
|---|---|
| **AR-1** | **Two of ten rungs do not carry property 4** — `ar-05-beam-dawn` at 1 flip of 8 and `ar-10-vertical-evening` at 2 of 8, on both graded sides. 2 of 8 is doc/doctrine-evolution.md §5's own noise floor, i.e. the level at which no claim may be made about that rung at all. Not repaired here on purpose (§State 2) |
| **AR-2** | **The arena has not been flown against the genome.** No lever sweep, no S1, no S2, no `--emit-informative`. §State 4 is a single probe on one gene on one rung and must not be read as a gate result. The gate run is the next thing, and it belongs to whoever owns doc/doctrine-evolution.md — a round that built an arena and then declared it informative would be grading its own work |
| **AR-3** | **The cells are not in `sim/tools/arena-campaign.txt`.** That file's generating rule ("every (mission, team, module) group of the ten campaigns' 100 committed missions …") is scoped to the campaigns and does not reach these files, so the twenty `ar-*` cells have to be added by whoever runs the gate, together with a rule that admits them without curating them |
| **AR-4** | **`doc/campaigns/INDEX.md` does not mention this file**, deliberately (it declares the directory of ten complete, and this is not an eleventh campaign). A pointer that says *"and one arena that is not a campaign"* would be honest; adding it was out of this round's scope |
| **AR-5** | **The blue sweep never fires at baseline in six of the ten rungs.** It commands radar standby at t ≈ 6 s and never re-radiates. Whether that is the correct emission discipline for a four-ship with a live RWR picture is a `pilot.md` question, not an arena question — but until it is answered, six of the ten baselines have an air half that only red shoots in, and the level-C craft gate is carried by the strikers' deliveries |
| **AR-6** | **G7 (`pilot_attack_ccip_m`) is still structurally dead here.** All four strikers deliver CCRP; `E-17`(c) names a CCIP delivery as one of the two arenas the genome lacks. A CCIP variant was NOT built, because a release from this block would have had to be re-derived and the round had no measurement to justify the geometry |
| **AR-7** | **The arena is 16 aircraft plus 9 ground units per rung and it is not cheap.** [MESS] a rung costs 7…40 s of wall clock at `--threads 1`; the S7 screen alone is 8 runs per graded side, 160 runs for the ten rungs |
