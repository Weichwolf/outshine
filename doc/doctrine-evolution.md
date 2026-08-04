# Doctrine evolution — fitness, genome, archive, arena

**Subject:** step 3 of the owner goal. What the evolutionary tournament **optimises** (the fitness), what
it is **allowed to change** (the genome), what stops the co-evolution from **circling** (the archive),
and what an arena must be before any of it measures anything (the **saturation criterion**).

**Status: BUILT, rounds `E1`, `E2` and `E3` (2026-07-29).** The `## Spec` below is unchanged from the round
that wrote it — it is the contract, and a contract that is edited to match what was built measures
nothing. `E2` cut the genome against the arena: it measured every gene's lever one at a time, found
that `E1`'s total tie was the COMPARISON and not the genome (deviation D6), built the merge profile and
three merge geometries, ran the gate with the genome's own alphabet (E-12) and got a REFUSAL, and
produced the first evolution run whose population does not tie. Its measurements are the second
`## State` block. **`E5` (2026-07-30) is step 5 of the owner goal — evolution over the CAMPAIGN BREADTH — and its
result is a refusal with numbers:** the gate was pointed at all **154 cells of the ten campaigns**
(3,388 runs), the genome grew the two ground decisions the campaigns actually grade (§7), and
**0 cells are informative** — 2 under the loosest reading the gate admits, which is exactly the verdict
`campaigns/w1-red-flag.md` reached on its own ten rungs. No doctrine shift is published; the round's
product is that measurement, the per-gene reach table, and four findings in
§Gaps → "Exploits the evolution found", one of which is an exploit of this file's own level M.
**`E6` (2026-07-30) repairs the two defects `E5` measured against this file's own instrument** and
publishes no doctrine shift either, because it flew no evolution run: the craft level grew a GROUND
currency (§8 E12 — a pair compared by domination, never a sixth summand, so no metre of aim error is
ever priced in shot-geometry points) and the runner now lets every judge finish a run that ENDS
(§8 E13, X-1). Measured: `C` was `GATE` on **32 of the 46** campaign cells that aim a bomb and is on
**0 of 46**, with `(V, M)` unmoved on all 154; the cell where three unrelated levers lifted M from 0 to
8 now reads the same key whether the opponent's MiG departs or not; and the three published tournament
results are re-flown on both instruments and are identical.
`E3` took the merge apart: E-15's *"what the merge decides is a CFIT"* was exactly
right at n = 120 runs (**77 monitor KOs, the MiG-29 in 77 of 77**), the cause was one line of the
airframe layer (`FBFlightControl` bound this jet's own rate damper only on its FLCS path while BFM
commands `Manual`), and closing it took the merge's S1 pass away with it. Its measurements are the
third `## State` block; nothing in the fitness, the genome, the archive or the gate moved.
`## State` carries what the build measured against it, **including the five places the spec did not
carry** (D1–D5) and the one exhibit that did not come out the way the spec predicted (A). Built:
`sim/tools/fb_fitness.py` (the fitness), `sim/tools/fb_arena_check.py` (the gate),
`sim/tools/fb_evolve.py` (the runner and the archive), the `Free`/`Scale` split in
`sim/src/pilot/FBPilotTuning.*`, `mission OBJECTIVE` in `sim/src/core/FBMissionMonitor.*`, the
eight-geometry arena in `sim/tools/fb_tournament.py`, and (`E5`) `sim/tools/fb_campaign_arena.py` +
`fb_campaign_evolve.py` + `fb_campaign_exploit.py` with `tools/arena-campaign.txt` and
`tools/levers-campaign.txt`.

**Why the name, and why it is not `evolution.md`.** Exactly one thing evolves here: **doctrine** — the
decisions a pilot makes with the aircraft he was given. The model, the deck, the weapon and the sensor
are fixed by principle 1 and may not appear in a genome. The filename carries that boundary so that a
file called "evolution" cannot quietly grow a section about evolving an airframe.

**Why it is outside the `sim/src/` mirror.** Like [`duels.md`](duels.md) (a PAIRING),
[`formation.md`](formation.md) (a FLIGHT) and [`air-defence-network.md`](air-defence-network.md) (a NET),
a fitness is not a directory. It cuts through `pilot/` (the genome is `FBPilotTuning`'s key table),
`core/` (the verdict it must read comes from `FBMissionMonitor`), `missions/` (the arena is `.fbm` text)
and `tools/` (which is not in the mirror at all). Putting it in [`pilot.md`](pilot.md) would bury the
measurement half inside a control-law file that is already 1,500 lines. It is the **fifth** deliberate
exception to the mirror rule.

| Source class | What it is |
|---|---|
| **The instrument being reformed** | `sim/tools/fb_tournament.py` (the `Score()` block, the attribution half, `variants-*.txt`) |
| **The measurements the reform rests on** | [`formation.md`](formation.md) §State "The flight tournament" · [`duels.md`](duels.md) §State "Tournament cross-check" · [`modules/air/module.md`](modules/air/module.md) §State B6 · [`weapons.md`](weapons.md) §State "30 mm kinetic path" |
| **The vocabulary the fitness must sit on** | [`missions/verdict.md`](missions/verdict.md) (four verdicts, nine objective kinds) |
| **The genome's home** | [`pilot.md`](pilot.md) §9 (`FBPilotTuning`), §11 (pilot property vs aircraft property), [`formation.md`](formation.md) §§4–6 |
| **The runner evolved over** | [`missions/campaign.md`](missions/campaign.md) (the `.fbc` layer, the fingerprint, the refusal rule) |
| **The boundary** | `CLAUDE.md` principle 1, `sim/assets/MODEL-DELTAS.md`, [`conventions.md`](conventions.md) |

Marking: `[SET]` = a FlightBox setting with its one-sentence reason · `[DERIVED]` = computed from a
named relation · `[MESS]` = measured in this tree, with the file it is measured in · `[TODO]` = named
and not yet measured.

---

## Spec

### 0. The four contracts of this round

| # | Contract | Acceptance / measurement anchor |
|---|---|---|
| **E1** | **The fitness is lexicographic, not weighted.** Result dominates absolutely; craft orders only inside an identical result | §1.3's tuple, computed per unit per run. Acceptance: no craft item, at any value inside its bound, can change the order of two variants whose result keys differ |
| **E2** | **The genome is doctrine and cannot express a model number** | §2. Acceptance: `git status --porcelain sim/assets` is empty after an evolution run and `make -C sim verify-models` is green; the runner prints its own alphabet at start (`evolving N genes of M pilot keys; K keys refused as airframe-owned`) |
| **E3** | **An archive of earlier opponents prevents circling, and the circling is MEASURED** | §3. Acceptance: the champion set's cyclic-triple count `T ≤ 0.05` and the score against the FIXED yardstick is non-decreasing over a five-generation window |
| **E4** | **No evolution run starts on a saturated arena** | §4. Acceptance: the arena check passes (≥ 6 geometries, ≥ 3 informative, modal outcome class ≤ 60 % per informative geometry) and its summary line heads every evolution log |
| **E5** | **The product of a round is an explainable doctrine shift plus the exploit list, never a better number** | §6's report template. A round with an empty "mechanism" section does not publish a shift |
| **E6** | **Every run stays deterministic and every archived score names the arena it was measured on** | one fingerprint over `--threads 1/2/4`; an archive whose arena fingerprint does not match is REFUSED, not re-used ([`missions/campaign.md`](missions/campaign.md) §5's rule, applied verbatim) |

---

### 1. The fitness

#### 1.1 The evidence that the present one rewards the wrong thing

The owner's sentence is *"the present fitness pays per landed salvo and thereby demonstrably rewards
the wrong thing."* The evidence exists, it is in the tree, and one of the three exhibits was **written
down as a finding against its own round** by the file that measured it.

**Exhibit A — the fitness ranks the doctrine that abandons the sort above the one that keeps it.**
[MESS, [`formation.md`](formation.md) §State "The flight tournament", `mirror` geometry, 40 runs,
`--flight 2`]

| variant | fitness | kill / lost |
|---|---|---|
| `f16_solo` (`dl=off` — two singles in formation, no sort, no cover) | **1097.8** | 0 / 0 |
| `f16_net` (`dl=on` — the cooperative sort and the cover rule this tree built and measured) | 977.1 | 0 / 0 |

Identical result. 120.7 points of difference, all of it `hits landed`. The file names the mechanism
itself: *"The fitness pays 150 per burst landed and counts the opposing FLIGHT's hits for each member;
an element that concentrates both jets on one bandit therefore banks more hits than one that splits
across two, and in an arena where nobody dies the concentration wins on points."* The code confirms it
exactly — `FlightView.hits = sum(m.hits for m in members)` and `score()` is called once per member, so
a hit on an opposing flight of n is **paid n times**. The same round measured that the cooperative sort
is the better doctrine by every non-fitness metric it has (93 % target split, 0.962 against 0.750
distinct targets per shooter, `dup && free` = 0). **The fitness contradicts the tree's own measurement
of the thing it is supposed to select for.** That is the whole proof; the rest is corroboration.

**Exhibit B — "outcome dominates" is false today, and the margin is 1.5 points.**
[MESS, [`duels.md`](duels.md) §State "Tournament cross-check", `mirror`, 30 runs]

| variant | fitness | outcome | craft | kill / lost / draw |
|---|---|---|---|---|
| `f16_long` | **603.3** | 460.0 | 143.3 | 4 / 0 / 6 |
| `f16_base` | 601.8 | **550.0** | 51.8 | 4 / 0 / 6 |

Identical records. The two outcome bands decompose exactly [DERIVED, ten runs per variant]: `f16_base`
5500 = 4·1000 + 150·**10 hits**; `f16_long` 4600 = 4000 + 150·**4 hits** — i.e. `f16_long` landed
exactly its four kills and nothing else, `f16_base` landed six further bursts that killed nobody and
was paid **+900** for them. Then 91.5 points of craft overturned 90.0 points of outcome and the field
was ordered by the 1.5-point residue of two quantities, **neither of which measures the result.** The
script's own banner claims this cannot happen (*"the outcome band is larger than everything else put
together, so no amount of good craft can out-score a kill"*). The claim holds for `kill`/`lost` and is
false for the term that sits in the same band.

**Exhibit C — the counter is a count of ticks, and the arena that will expose it does not exist yet.**
`core/FBDamageModel::ApplyKinetic` calls `FBSystemHealth::NoteHit()` **once per gun bundle**, and a
bundle is one tick's worth of rounds ([`weapons.md`](weapons.md) §3.1: *"6,000 rounds/min against a
0.1 s tick is ten rounds per tick"*). The same function's sibling `AddKinetic` exists **precisely** so
that the damage does not depend on that partition — *"judging every bundle on its own would make the
damage a function of the TICK RATE, which is exactly what principle 4 forbids"* — and the hit COUNTER
carries no such protection.

| Quantity | Value | Provenance |
|---|---|---|
| Bundles per second of continuous M61A1 fire | 10 | [DERIVED] 6,000 rd/min ÷ 60 ÷ (0.1 s tick) = 10 rd/tick ⇒ 10 bundles/s |
| Fitness per second of on-target fire | **1,500 points** | [DERIVED] 10 × `W_HIT` 150 = 1.5 kills per second |
| Per briefed squeeze (`BfmGunBurstS` 0.5 s, and the burst length **is** the bus minimum spacing, so fire is continuous while the funnel holds) | 750 points | [DERIVED] |
| The endpoint the tree already flew | *"the FULL drum wipes the avionics … **without** downing the aircraft"* (571 m, `dmg_failed 4016`) | [MESS] [`weapons.md`](weapons.md) §State. GSh-301 at 1,500 rd/min ⇒ 2.5 rd/bundle ⇒ ~60 bundles for 150 rounds, all above `kMinReportedHits` 0.1 (the row measures 0.3–0.46 rd/bundle) ⇒ `dmg_hits` ≈ 60 ⇒ **≈ 9,000 points for a jet that flew on** [DERIVED] |

**Honesty about C:** the bundle→`NoteHit` mapping is source-exact and the arithmetic is off published
rates, but the count itself has not been read off that run's last telemetry line. `[TODO]` — one read
of `dmg_hits` in `mig29-gun`'s telemetry settles it, and it is an acceptance item of the first
evolution round. **[READ 2026-07-29 — the mechanism holds and the number is 40 % high; §State.]** C is also **latent**: the tournaments that produced A and B are BVR and fire no gun,
so C did not cause them. It is the gradient the genome is about to be pointed at the moment a
gun-carrying arena exists, and it is listed here so that it is a prediction and not a post-hoc excuse.

#### 1.2 Why a weighting repeats exactly this error

A weighted sum answers the question *"how many landed bursts is a kill worth?"* — and that question has
no answer, because the two are not the same kind of thing. Whatever exchange rate is chosen is a
**standing offer**, and a search procedure is a machine for finding the cheapest way to accept it. The
present rate is 1 kill = 6.67 hits, which Exhibit C prices at 0.67 s of trigger.

Three properties make the weighted form structurally wrong rather than merely mistuned:

| Property | Consequence |
|---|---|
| The dominance claim is a claim about the **sizes of numbers** | it is therefore falsified by any new term, any new arena, or any count that can grow. Exhibit B falsified it with the terms that were already there |
| The craft band's width is computable and is **half a kill** | [DERIVED from the weights] max craft = 100 (quality) + 80 (support) + 40 (lead) + 40 (defence) + 40 (energy) = **+300**; min = −250 (no shot) − 40 (lead) = **−290**; span **590 ≈ 3.9 hits ≈ 0.49 kills**. A band that wide is not a tie-breaker, it is a lever |
| A sum is **commutative between currencies** | it cannot express "never" — and "never" is the whole content of "result dominates" |

The tree already refused this pattern once, in the place where an invariant actually had to hold:
`core/FBSystemHealth` is not protected by making self-repair expensive, it is protected by making the
mutators private with exactly one friend, so that *"self-healing does not compile"*. **A lexicographic
order is the same move applied to a score**: the exchange rate between levels is infinite, so there is
nothing to trade.

#### 1.3 The order

Per unit, per run. Compared left to right; the first level that differs decides; a level is consulted
only on an exact tie of every level to its left.

```
key(unit, run) = ( V , M , C )          larger is better

V ∈ {0,1,2,3}   the JUDGE's verdict for this unit, read from `UNIT_RESULT`, never recomputed
M ∈ {0..k}      how many of the k objectives this unit DECLARED the judge marked fulfilled
C ∈ ℝ           craft — a bounded weighted sum that can never cross a level
```

**Level V — the four outcomes, and they are the judge's own.** `FBMissionVerdict` has exactly
`{None, Success, Fail, Timeout}` and the physical judge overrides it with `CRASH`/`LOC`
([`missions/verdict.md`](missions/verdict.md); the per-unit string is
`SUCCESS|FAIL|TIMEOUT|CRASH|LOC|NONE`).

| V | `UNIT_RESULT result=` | Reading |
|---|---|---|
| 3 | `SUCCESS` | the declared objectives were met |
| 2 | `TIMEOUT`, or `NONE` **with** declared objectives | nothing was decided — the honest draw |
| 1 | `FAIL` | decisively failed: shot combat-ineffective, or a touchdown off the runway |
| 0 | `CRASH`, `LOC` | the airframe was lost **to nobody** |
| — | `NONE` **without** declared objectives | excluded from the fitness: nothing was asked of this unit |

Two decisions, both `[SET]` and both stated so a reader can re-decide:
- **`CRASH` and `LOC` share rank 0 and are not separated.** The difference between a departure and a
  CFIT is a detail of the physical judge, not of a doctrine, and both are the same verdict about the
  doctrine: it lost the jet without an opponent. It is also the tree's live defect class
  ([`pilot.md`](pilot.md) 2.9, `duel-merge`'s CFIT at t = 232.3).
- **`TIMEOUT` above `FAIL`.** A doctrine that decides nothing is worse than one that wins and better
  than one that loses. There is no third reading.

**Level M — what "the same outcome" means.** Two variants have the SAME OUTCOME iff their `V` is equal
**and** they fulfilled the same NUMBER of their declared objectives. Because a tournament generates both
seats from one template and only the `pilot_*` lines differ, the declared objective SET is identical
across the whole field by construction, so a count is comparable without ordering the nine kinds against
each other — and ordering them **must** be avoided: nothing in the tree says an `identify` is worth more
or less than a `suppress`, and inventing that ladder would be exactly the exchange rate §1.2 refuses.

Level M is what keeps the fitness readable in the stalemate arena the tree actually has: five of five
BVR geometries draw ([`duels.md`](duels.md)), and under V alone the whole field ties at 2. With M, a
unit that met `survive` + `deny release` while its opponent met only `survive` is ahead **for a reason
the mission declared.**

**Level C — craft.** The same items as today minus the two that pay per event, plus one gate:

| Item | Form | Bound | Kept because |
|---|---|---|---|
| shot geometry | `100 · zone · cos(ata)`, plateaued at Rtr | [0, 100] | a launch envelope position is a property of the DECISION and is capped by construction |
| support | `80 · eng_support_f` | [0, 80] | the difference between a launch and a kill; a fraction, so unfarmable |
| shot lead | `40 · tanh((t_foe − t_me)/15)` | [−40, 40] | relative to the opponent in the SAME run — a duel is a relative thing |
| defence | `40 · max(0, 1 − react/8)`, only where a warning existed and was survived | [0, 40] | being shot at is the opponent's decision, so it cannot be farmed |
| energy | `40 · eng_es_min / es_start` | [0, 40] | the state the engagement is left in |
| rounds | `−25` per round fired | ≥ −150 | bounded by the loadout, not by the run length |
| **hits landed** | **REMOVED** | — | it is the defect of §1.1: a per-event payment on a count the simulator partitions |
| **no shot** | **REMOVED, replaced by a gate** | — | see below |

**The engagement gate replaces the anti-running-away term.** A unit that neither fired nor designated
gets `C = −∞` (last within its own outcome class). A gate rather than a −250 price, because a price can
be bought back — the old one could be repaid by `energy` + `defence` + `support` — and a gate cannot.
Engaged := `eng_shot_s ≥ 0 ∨ eng_lock_s ≥ 0` [SET, both are `eng_*` columns that survive the
engagement].

**What level C stops being.** Under a lexicographic order the craft WEIGHTS are no longer load-bearing:
only their signs and their boundedness matter, because no value of C crosses a level. The present file
spends five paragraphs defending the SIZES of six numbers; that defence becomes unnecessary, and the
one property that must be checked is the one that is checkable — every item is bounded, so `C` is
bounded, so `C` can never be a result.

#### 1.4 Aggregating over runs — three designs, one recommended

A lexicographic key cannot be averaged: a mean of ranks is a number in a currency that has no units, and
level M would then never be consulted because two float means are almost never equal.

| | A — mean per level | B — mean per level with a tie band | **C — pairwise domination (RECOMMENDED)** |
|---|---|---|---|
| Mechanism | mean of V, then of M, then of C | as A, with a declared indifference band ε per level | per PAIRING, compare the two units' keys directly; the variant scores 1 / ½ / 0 per run |
| Ties | never happen at level V ⇒ M and C are dead code | happen by declaration ⇒ needs a magic ε per level | happen naturally: ranks are integers |
| Uses the both-seats structure the tool already flies | no | no | **yes** — the two mirrored runs of a pair are exactly one match |
| Feeds the archive (§3) | no | no | **yes** — an archive needs a win/loss vector, which this produces for free |
| Cost | the coarseness: an N-variant field gives at most N−1 matches, so C does real tie-break work | | |

**Recommended: C.** Variant fitness = `Σ over opponents of pair points / (2 · N_opponents)` ∈ [0, 1] — a
normalised win rate, comparable across field sizes exactly as the old mean was meant to be, with no
tolerance constant anywhere. `kOutcomeTol` disappears with the scalar it existed for.

The report still prints the itemised key and the craft breakdown per run, because **the point of the
tournament is WHY** and that has not changed.

---

### 2. The genome — five fields, and a boundary with teeth

#### 2.1 The five

Every one of the five has a named, measured precedent in this tree — three exist today as mission text
and two are named as MISSING.

| # | Gene | Displaces which decision | Band | It is working when … | It is inert when … |
|---|---|---|---|---|---|
| **G1** | `pilot_flight_shape` | the station geometry the wingman holds. Today `FormationSpreadM`/`TrailM`/`StackM` are **airframe hooks and not mission data**, so a mission cannot brief a wedge, a trail or a wall — [`formation.md`](formation.md) F5. The mode selects a `(spread, trail, stack)` triple, each a MULTIPLE of the row's own `FormationSpreadM` | `{spread, wedge, trail, wall}` [SET — the four F5 names] | `flt_sta` median moves and recovers through a turn; the discriminating channel is `flt_free` (contacts nobody is on) at the same `flt_mates` — a shape that buys mutual support lowers it | a four-ship is four abreast (`FormationTrailM` = 0, today's default) |
| **G2** | `pilot_cover_frac` | the rule that keeps one member free. Today a binary that fires whenever a mate is bound ([`formation.md`](formation.md) §6). The gene is the MULTIPLE of the OWN weapon's binding time a member will hold its trigger for | 0 … 3.0 [SET; 0 = the rule off, 1.0 = exactly one binding] | `flt_defer_s` > 0 and `flt_both_s` = 0. [MESS] the F-16 measured 7.8 s of deferral, `pair-cover.fbm` | `flt_defer_s` = 0. **Its gradient is airframe-shaped:** the AIM-120 binds 0.3 s and the R-27R 17.3 s — a factor of **58** [MESS] — so this gene is nearly flat for the F-16 and steep for the MiG, and it can only be evolved in a MIXED tournament |
| **G3** | `pilot_sort` | who takes whom. Exists today as `sort=none\|left\|right\|near\|far` plus the channel bit `dl=on\|off` | the five contracts × the channel | distinct targets per engaged member. [MESS] cooperative **0.962** against contract **0.750**; the violation metric `flt_dup ∧ flt_free > 0` must stay **0** | `flt_src` = 0. **Watch `flt_switch`:** the known instability is 19–35 re-assignments per unit ([`formation.md`](formation.md) F2), so a gene that raises switches without raising the split is riding the jitter, not sorting |
| **G4** | `pilot_energy_frac` | the BFM throttle's standing bias — the one the tree explicitly refused to remove until this gene exists: *"The standing bias the loop leaves is therefore doing an ENERGY job by accident; before this can be corrected, the energy rule it stands in for has to exist"* ([`pilot.md`](pilot.md) §Gaps, rejected: integral action). The gene is the speed the pursuer insists on keeping, as a FRACTION of its own `BfmCornerSpeedKt` | 0.7 … 1.2 [SET — the band runs from below `BfmMinSpeedKt` (300/380 = 0.79) to above the overspeed clamp (1.15)] | the 16-approach sweep's two cells move in OPPOSITE directions: [MESS] the integral fix took the straight defender 0/8 → 8/8 and the turning defender 8/8 → 0/8. A gene that moves both up has found something; a gene that trades one for the other has found the same bias | `eng_es_min/es_start` and `bfm_ctrl_s` unchanged to the digit |
| **G5** | `pilot_emcon_frac` | WHEN the radar goes loud. Today it is briefed mission text — `duel-emcon`'s commit call is a fixed `t = 170` — and never a decision. The gene is the range at which the set is turned on, as a FRACTION of the aircraft's OWN search gate | 0 … 1.5 [SET; **0 = never radiate**, **≥ 1.0 = loud from acquisition**; the two rails ARE the two named policies] | the `fcr_on`/`n019_on` radiating spans move AND the OPPONENT's `eng_detect_s` moves with them — EMCON is measured on the other jet's clock | the spans are the whole run. **Blocked today:** [`duels.md`](duels.md) D3 — the pilot does not read the IRST, so "silent" means "silent and blind" and the band is degenerate. This gene may not be evolved before D3 closes, and §Gaps books it |

#### 2.2 The boundary — structural, not disciplinary

> **A genome field may displace a PILOT constant or a DECISION. It may never SET an aircraft or weapon
> number — only SCALE one.**

The taxonomy is not new: [`pilot.md`](pilot.md) §11 already separates PILOT properties (constants in
`FBPilot.cpp`, because a human is the same in every cockpit) from AIRCRAFT properties (virtual hooks the
module overrides). What is new is that the tuning table must carry the distinction instead of leaving it
to the author of the next key.

**The proposed shape** — two entry kinds in `FBPilotTuning`'s existing fixed table, and two accessors
that cannot be confused for each other:

| Kind | Declaration | Read as | Example |
|---|---|---|---|
| `Free` | `Free(p, lo, hi)` — a pure pilot decision | `Tuned(p, own)` — today's accessor, unchanged | `pilot_react_s`, `pilot_beam_deg`, `pilot_sort` |
| `Scale` | `Scale(p, lo, hi, hook)` — a multiple of a named airframe hook | `Scaled(p, own)` = `own · Or(p, 1.0)` | `pilot_energy_frac` (× `BfmCornerSpeedKt`), `pilot_cover_frac` (× the weapon's own `ttaS`), `pilot_emcon_frac` (× the radar's own gate) |

Four properties follow, and each is a check rather than a rule of conduct:

1. **A `Scale` key cannot express an absolute.** There is no syntax for it. The dimensioned quantity
   never leaves the airframe's own class, which is exactly what `Tuned(p, own)`'s `own` argument was
   for — a fraction makes the same intent unbypassable.
2. **The alphabet is already closed by a built gate.** `FBPilotTuning::Set` rejects an unknown key or an
   out-of-band value and `FBF16Module::ApplySetup` turns that into a mission **FAIL** — a mistyped or
   mutated-out-of-range gene does not silently fly the default, it does not fly. Measured precedent:
   `air-tier-refused.fbm` exits **1** with `SET_INVALID_VALUE` + `SET_REJECTED` and the run never
   starts ([`modules/air/module.md`](modules/air/module.md) §State).
3. **The evolution runner must not be able to write a model file at all.** Today `fb_tournament.py`
   HAS such a path: the attribution half regenerates a row's JSBSim deck in place (`run_attr(deck=…)` /
   `restore_decks()`). That path is legitimate where it lives and must not be reachable from the
   evolutionary one. Structural form: **`fb_evolve.py` is a separate tool** that imports the mission
   generator and the scorer and never the deck writer, and the gate is the one this tree already uses —
   **it prints its own alphabet at start**: `evolving 5 genes of 22 pilot keys; 17 refused as
   airframe-owned; 0 non-pilot keys reachable`, in the shape of `verify-layers`' *"6 registry reader(s)
   inside the perception boundary"*.
4. **The falsification, run every time.** Before and after: `git status --porcelain sim/assets` empty
   and `make -C sim verify-models` green. A run that moved a deck is **void**, not "explained".

**The existing table's exception, named rather than hidden.** `pilot_speed_kt` (150…900) and
`pilot_lock_nm` (1…40) are absolutes today, and `pilot_gun_burst_s`' upper limit is a WEAPON number
(`FBGun::MaxBurstS`). They predate this rule and stay; the rule binds **new** keys. `pilot_speed_kt` is
also the key that already cost the tree a defect of exactly this class — [MESS] `duels.md` M3, a hook
compared in CAS and fed to a TAS command, 40 % below the aircraft's own `BfmMinSpeedKt` — which is the
argument for the rule, not against it.

---

### 3. The archive — against circling

#### 3.1 The failure mode, stated precisely

Co-evolutionary fitness is measured against the CURRENT opponents, so it is a **relative** quantity in a
moving frame. A population in which A beats B, B beats C and C beats A can circle indefinitely while
every generation's measured fitness rises, because each generation really does beat the one it was
measured against. The fitness curve of a circling population and of a learning one are the same curve.

#### 3.2 What is kept

A genome is a **text line** ([`pilot.md`](pilot.md) §9's whole point: *"a variant is a LINE in a mission
file instead of a class"*), so an archive is a text file. Per member:

| Field | Why |
|---|---|
| the variant line (`name key=value …`, incl. `module=`, `dl=`, `sort=`) | the genome, verbatim and re-flyable |
| generation of admission, and the index | the history axis the circling test is computed along |
| the **arena fingerprint** it was measured on | §3.5 — without it a score is not comparable to anything |
| its win vector against the opponents it was scored against | the admission test (§3.4) and the CIAO matrix need it |

Storage cost: bytes. Nothing else is kept — no telemetry, no population, no scores that outlive their
arena.

#### 3.3 How selection uses it

A candidate's fitness (§1.4) is its win rate against **the current population** ∪ **a k-sample of the
archive**.

Sampling is **deterministic** — `conventions.md` forbids randomness in this simulator and an archive
sample is no exception. The rule: index the archive by admission order 0…N−1 and take
`i·(N−1)/(k−1)` for `i = 0…k−1`, i.e. an even stride over the whole history, always including the
**oldest** and the **newest** member. The oldest is the anchor that makes progress measurable at all;
the newest is the current threat.

`k = 8` [SET]. Cost arithmetic, stated because it is the reason k is not larger: at population `P` and
both seats, a generation costs `P(P−1)` runs for the round robin plus `2Pk` for the archive; at
`P = 12, k = 8` that is `132 + 192 = 324` runs per generation — **the archive is 59 % of the budget.**
Wall-clock is the arena's timeout divided by the measured speedup and is `[TODO]`.

#### 3.4 When something enters — two designs

| | A — Hall of Fame | **B — non-dominated admission (RECOMMENDED)** |
|---|---|---|
| Rule | the generation's champion enters, every generation | a candidate enters iff its **win vector** over the sampled opponents is not dominated by any current member's vector on the same opponents |
| Extra runs | none | none — the vectors already exist |
| Growth | +1 per generation, accumulating near-duplicates | only on genuinely new behaviour |
| Against circling | weak: a cycler's champions all enter and the archive becomes the cycle | **strong**: a set of mutually non-dominated strategies is one a cycler must beat ALL of, and beating all of them is by construction not a cycle |

**Recommended: B.** Cap `kArchiveMax = 64` [SET]; on overflow evict the member whose win vector is
nearest (Hamming) to another member's, ties broken by the higher index — **never the oldest**, which is
the history anchor. The eviction rule is stated because it must be deterministic.

#### 3.5 What it costs — and the cost is not the runs

**Every simulator fix invalidates the archive.** An archived score was measured on one arena, one
binary, one elevation source. The tree already owns this rule and its wording:
[`missions/campaign.md`](missions/campaign.md) §5 — *"A fingerprint is comparable only within ONE
environment base … an output tree without the record is REFUSED rather than replayed against a
default."* Applied here: an archive whose arena fingerprint differs from the current arena's is
**refused**, not reused.

The mitigation is cheap and the honesty is the point: the **genomes** survive (they are text and cost
nothing), the **scores** do not. After a reset the first generation re-flies the archive against itself
— `N(N−1)` runs at both seats — and this is a running cost, not an exception, because **finding
simulator defects is the declared product of this work** (§5). A round that fixes an exploit pays for
its own archive reset. Budgeting for that is part of the specification.

#### 3.6 The circling measurement — three instruments

**(a) The fixed yardstick — mandatory, every generation, and the cheapest.** A frozen doctrine set that
never evolves: `tools/variants-bvr.txt`'s six lines, which predate all of this. Every generation's
champion is flown against all six, both seats — **12 runs per generation**.

- learning ⇒ the score against the FIXED set rises and does not fall back;
- circling ⇒ it oscillates with no trend, while the score against the population rises.

Acceptance: **non-decreasing over any five-generation window** [SET; five because a single generation's
score is one sample of an arena the tree has measured to be chaotic — `pilot.md`'s ±3 m spawn probe
flipped the outcome in 2 of 8 samples].

**(b) The champion tournament — the exact statistic.** At the end of a run of generations, fly every
champion against every other champion, both seats, and build the tournament graph `W`. The number of
**cyclic triples** has a closed form (Kendall–Babington Smith):

```
d = C(n,3) − Σ_i C(s_i, 2)          s_i = champion i's number of wins
T = d / C(n,3)                       0 = perfectly transitive, → 1/4 for a coin-toss field
```

Acceptance: **`T ≤ 0.05`** [SET — a twentieth keeps the intransitive part an order of magnitude below
the random-field value of 0.25, and the number is printed so a reader can re-decide]. A transitive
champion set is a learning trajectory; a banded one (i beats i−1, loses to i−3) is a cycle, and `d`
counts its triples exactly.

**(c) The doctrine trajectory — free.** Champion genomes as vectors, each gene normalised by its own
band. Compute `min over j ≤ i−3 of ‖g_i − g_j‖`. A learner's distance to its own past stays bounded
away from zero; a cycler's returns periodically to near zero. No extra runs, and it names the GENE that
is circling — which is what makes it a debugging instrument and not just a detector.

---

### 4. The arena — the blocker, and what "not saturated" means

#### 4.1 The measurement that blocks everything

[MESS, [`modules/air/module.md`](modules/air/module.md) §State B6] The attribution instrument on the
`mig21` row: nine deck perturbations + the control cell + nine doctrine variants = **19 runs**, of which
**18 return the identical outcome, −1450.0**; only `beam-hard` moves it. `band_deck` = 0.0 and
`|control − baseline|` = 0.0, so the tool correctly prints the instrument defect instead of a result.

**And the constant decomposes** [DERIVED, against the present weights; `[TODO]` confirm against the
tool's own printed items, Gaps E-3]:

```
−1450.0 = −1200 (lost)  −250 (no shot)  ± 0 residue
```

Only two items in `Score()` are of that magnitude, and both must be present: the bandit was **shot down
without ever firing a round**, in 18 of 19 runs. The residue is exactly zero, which admits two readings
and both are innocuous — either the craft items cancel (`shot lead` −40 against `energy` +40, i.e. it
died at its entry energy having never manoeuvred, `es_min/es_start` clamped to 1) or neither fires. The
finding does not depend on which: **nothing inside the bandit's own reach changed the outcome.** A
saturated arena is not a small defect of an
attribution test — it is the absence of the signal selection would act on. Deck spread measures
nothing, doctrine spread measures nothing, and an evolutionary run on it would optimise noise.

#### 4.2 The criterion, in three parts

**Per geometry, against a fixed field (the yardstick of §3.6a):**

| # | Criterion | Number | Justification |
|---|---|---|---|
| **S1** | The runs do not all land in the same outcome class | ≥ 2 distinct `(V, M)` classes, and the **modal class ≤ 60 %** of runs | [SET] a binary question is most informative at 50/50; 60 % admits a real asymmetry and excludes the measured 18/19 = **94.7 %** by 35 points |
| **S2** | The geometry separates DOCTRINES, not noise | **≥ 3 of the 9** doctrine levers change the outcome class from the baseline | [SET, and it is a robustness argument] today it is **1 of 9**, so the band rests on a single run — and this tree has measured that a single run is not a measurement (`pilot.md`: perturbing a spawn by 0.8 m flips the outcome in 2 of 8 samples). With 3, the band survives losing any one |
| **S3** | The result is not a measurement of our own model | `band_deck ≤ 0.25 · band_doctrine` **and** `\|control − baseline\| > band_deck` | unchanged, cited: [`modules/air/module.md`](modules/air/module.md) §Spec 11, instrument 2 and the repaired one-sided instrument 3 |

**Per arena (a SET of geometries):**

| # | Criterion | Number | Justification |
|---|---|---|---|
| **S4** | Size | **≥ 6 geometries** | [DERIVED] the duels' own doctrine matrix is 4 geometries × 4 doctrine pairs, and **2 of the 4 draw every cell** (`MiG high` and `50° offset`) — a measured informative rate of **50 %**. To hold ≥ 3 informative geometries at that rate, ≥ 6. The rate is itself measured against one field, so the check is re-run whenever the field changes |
| **S5** | Yield | **≥ 3 informative** (S1 ∧ S2 ∧ S3) | below three, losing one geometry to a fix costs a third of the arena |
| **S6** | Distinctness | no two informative geometries produce the same outcome-class vector over the yardstick | operational and cheap: it measures whether a geometry asks a DIFFERENT question, which is what matters, rather than parameter distance, which does not |

**The axes an arena must span**, each named because the tree MEASURED that it changes the answer:

| Axis | The measurement that says it matters |
|---|---|
| initial aspect (head-on / offset / beam / stern) | [MESS] the 50° offset draws every doctrine cell that head-on decides — a crossing target spends the round's energy on turning (25.5 m / 19.4 m expiries) |
| energy asymmetry (co-alt/co-speed vs 6,000 m + 150 kt) | [MESS] `split` inverts the entire ranking: `f16_base` 1085.1 with 7 kills, every MiG variant negative |
| force ratio (1v1 / 2v2 / 4v4) | [MESS] the cooperative channel is worth an outcome band on `split` (940.9 with a kill vs 770.0 with none) and nothing on `mirror` |
| detection symmetry | [MESS] an 81 s detection lead (52.1 nm vs 26.0) buys **nothing** under equal doctrine and everything under unequal |
| weapon obligation (fire-and-forget vs illuminate-to-impact) | [MESS] 0.3 s against 17.3 s of binding — a factor of 58 |

**The gate.** `tools/fb_arena_check.py` (proposed) prints per geometry: modal-class share, distinct
classes, doctrine movers of 9, `band_deck`/`band_doctrine`, and the pairwise identity graph. **No
evolution run starts on an arena that has not passed, and the check's summary line heads every
evolution log.** This is the precondition for step 5 and it belongs BEFORE the campaigns: a campaign
flown on a saturated geometry produces a verdict that no lever on either side could have changed, and
that is not a campaign result, it is a fixed point.

---

### 5. Trap one — telling a tactic from an exploit

**The expectation, stated up front:** the search will exploit the simulator rather than improve the
doctrine, and the goal wants every such find as a **result**. What makes the results readable is a test
that separates the two; without it every finding is ambiguous.

Four instruments, applied to every champion before anything about it is published. The first two are
screens, the third is the verdict, the fourth is the audit.

| # | Test | Passes when | Costs |
|---|---|---|---|
| **X1** | **Arena invariance** — re-fly the champion against the yardstick on the arena's other informative geometries | the advantage survives on ≥ 2 of them | 2 × 6 × 2 runs. **A screen, not a verdict**: some real tactics are geometry-specific — the MiG's early launch decides only on the flat co-altitude geometry [MESS] |
| **X2** | **Invariance to our declared ignorance** — the existing `band_deck` grid (`CD0` ±10 %, `e` ±10 %, `Ixx` ±10 %, thrust ±5 %) applied to the CHAMPION rather than to a deck row | the outcome class is unchanged on ≥ 7 of the 9 points | 9 runs. A tactic survives our own uncertainty; an advantage riding a model artefact typically vanishes or inverts |
| **X3** | **The mechanism must be nameable in published channels** — a chain of blocks, events and columns, each with its number, that explains the shift | such a chain exists and each link's number is shown | no runs. **The burden of proof is inverted: no chain ⇒ exploit until proven otherwise.** The model of a passing chain is `duels.md`'s early-launch finding — *"a launch puts a missile symbol on the other jet's receiver, and a seeker on one's own aircraft is never negotiable"* — with the 1.5 s margin measured |
| **X4** | **The physical audit** — three named exploit classes, each with a detector that already exists | all three | below |

**X4's three detectors**, and each of them has a measured noise floor in this tree, which is what makes
them usable:

| Class | Detector | Its noise floor |
|---|---|---|
| **A lucky trajectory** dressed as a doctrine | perturb the decisive geometry's spawn longitude in 0.8 m steps over ±3 m, 8 samples; the champion's outcome class must survive | [MESS, `pilot.md`] the yardstick itself flips in **2 of 8** samples in a chaotic geometry — and if it does, **no claim may be made on that geometry at all** |
| **Judge evasion** — an advantage against a boundary rather than an opponent (a body rate just under 60 °/s for just under 3 s; an altitude just above the BFM floor; a `survive` that is only true because the run ended) | re-fly with the timeout extended by 50 % | an advantage that evaporates was an advantage against the CLOCK |
| **A partition exploit** — an advantage that scales with a count the simulator cuts arbitrarily (bundles, ticks, dispenses) | the fitness must contain no such count (§1.3 removes the one that existed) **and** the report lists, per champion, which counts moved. `dmg_hits` moving while `dmg_effective` does not is the signature | Exhibit C is the worked example, and it is the reason this class is listed first among things to look for |

**The verdict.** X1–X4 all pass ⇒ publishable as a doctrine shift. Any of X3 or X4 fails ⇒ filed as an
**exploit finding** in this file's `## Gaps` → "Exploits the evolution found", with the channel it rode
and the file that owns the defect. An exploit finding is a result of the round with the same standing as
a doctrine shift, and it is the one the goal asks for by name.

---

### 6. Trap two — what a round publishes

**Better numbers are not the product.** A fitness curve that rises is compatible with circling (§3.1),
with an exploit (§5) and with a saturated arena that manufactures a ranking out of craft noise (§4.1).
The product is an **explainable doctrine shift**, and the form that keeps it from becoming a story is a
fixed template with mandatory evidence per section.

| § | Section | The evidence it must carry |
|---|---|---|
| 1 | **The shift, in one sentence** | the gene(s) that moved and the DIRECTION. "The F-16 line moved `pilot_emcon_frac` 1.0 → 0.45 and `pilot_cover_frac` 0 → 1.2" |
| 2 | **The genome diff** | per gene: value before, after, and its band. **A gene sitting on its rail is not a finding, it is an unbounded band**, and must be reported as such |
| 3 | **The outcome ledger** | the lexicographic key distribution before/after **against the FIXED yardstick**, per geometry — never against the co-evolving population, which is the Red Queen and measures nothing |
| 4 | **The mechanism** | X3's chain of published channels, with the column and the number at every link. **This is the section that makes it a doctrine shift instead of a number** |
| 5 | **The counter** | what beats it. A doctrine with no known counter has not been tested; the archive is where to look, and the report names the member that comes closest |
| 6 | **The exploit audit** | X1–X4, pass/fail, with numbers |
| 7 | **The cost** | runs, wall time, what was invalidated (archive resets, missions whose telemetry moved) |
| 8 | **Exploits found** | every candidate that failed X3 or X4, with the channel it rode and the file that owns the defect. **This section is the round's product just as much as §1** |

**The binding rule: a round with an empty §4 does not publish §1.**

**What is expressly NOT a finding**, so that it cannot be reported as one:

- a rank change inside level C (craft orders, it does not decide);
- a fitness rise measured only against the co-evolving population;
- a ±1–2 kill change in a 16-approach sweep — [MESS, `pilot.md`] measured chaos: an 0.8 m spawn
  perturbation flips the outcome in 2 of 8 samples. Departure counts and pooled rate statistics are the
  parts of a sweep that carry information;
- anything measured on a geometry that failed S1–S3.

---

### 7. Round `E5` — the arena is the campaigns, and the genome grows a ground half

**Added 2026-07-30, before the round's first run.** §§0–6 are untouched: they are the contract and a
contract edited to match what was built measures nothing. This section adds five contracts of its own,
and every one of them exists because a MEASUREMENT of an earlier round forced it.

| # | Contract | Acceptance / measurement anchor |
|---|---|---|
| **E7** | **The arena is the campaigns' own committed missions.** A cell is `(mission, team, module)`: a hand-authored rung, the side the doctrine is flown by, and the units its key is summed over | the cell list is produced by a STATED RULE and not curated (every group of the 100 missions that flies a FlightBox module and declares an objective), and the committed files are never written: `git status --porcelain sim/missions sim/assets` empty before and after, or the run is VOID |
| **E8** | **The genome may name a GROUND decision, because that is the half the campaigns grade.** [MESS, [`campaigns/w1-red-flag.md`](campaigns/w1-red-flag.md)] *"the air half of a training ladder cannot be graded above two aircraft a side in this tree; the ground half can"* — and the five genes of §2.1 are all air-to-air. **G6** `pilot_attack_bias_s` (the pickle's lead over one's own actuation) and **G7** `pilot_attack_ccip_m` (the cross-error the pilot will accept before pressing) | both are EXISTING `Free` keys of `FBPilotTuning`'s compiled table with no new syntax and no dimensioned aircraft number; §2.2's boundary is unchanged and the runner prints its alphabet out of `fb-gym --pilot-keys` as before |
| **E9** | **Which genes can act is MEASURED before the run, per cell, in the published channel §2.1 names for each gene** — never assumed from the mission text | one table, one channel and one number per gene per cell; a gene with no live channel anywhere in the arena is reported as structurally inert WITH the source line that makes it so |
| **E10** | **The gate is not loosened, and where the lever set is larger than nine it is TIGHTENED.** S1's 60 %, S2's 3 movers, S4's 6 geometries and S5's 3 informative are the same constants | S2 passes only when the movers are ≥ 3 **and** ≥ 3/9 of the levers swept, so a longer lever file cannot buy a pass |
| **E11** | **A campaign cell is not co-evolutionary, and the round must say what that does to §3's three instruments** | the opposing side is committed mission text and cannot answer, so instrument (a) is EXACT rather than a proxy and the Red Queen is excluded by construction; (b) and (c) stay live, because a pairwise order over a multi-cell arena can still be intransitive |

**What E8 is not.** It is not a widening of §2.2's boundary and it may not become one. Both keys are
PILOT properties in [`pilot.md`](pilot.md) §11's sense — one is the pilot's knowledge of his own hands
(`AttackReleaseBiasS`, a module hook the F-16 sets to 0.0 s), the other is the error he is willing to
accept — and neither can express a mass, a drag, a warhead or a range. The two `Scale` genes stay the
only ones that touch an airframe number at all.

**The one thing this round may NOT do, stated in advance because it is the cheap way out.** A campaign
cell has a scripted opponent, so a doctrine can be fitted to ONE file's script. A shift is publishable
only if it survives §5's X1 on cells the champion was not selected on, and the burden of proof stays
inverted: no chain of published channels ⇒ exploit.

---

### 8. Round `E6` — craft learns the ground, and the judge always finishes

**Added 2026-07-30, before the round's first run.** §§0–7 are untouched; this section adds three
contracts, and each of the three exists because `E5` MEASURED the defect it repairs — E-17(a) and X-1.
The lexicographic order of §1.3 is not changed: **level V and level M are byte-identical, and only the
third key grows.**

| # | Contract | Acceptance / measurement anchor |
|---|---|---|
| **E12** | **Craft carries a SECOND currency, and the two are never added.** Every item of §1.3 is air-to-air, so a strike cell's key is `(V, M, GATE)` and a bomb 20 m out and a bomb 2 km out are exactly tied — [MESS, `E5`] on **32 of the 46** cells that aim a bomb. `C` becomes the pair `(air, aim)`, compared by **domination**: better in one and not worse in the other wins, better in one and worse in the other is INCOMPARABLE and ties | two runs with the same `V`, the same `M` and a different `aimErrM` are strictly ordered where they were exactly tied before; and **no craft value crosses a level** — both components stay bounded, so `\|C\|` stays under the 1e3 step |
| **E13** | **A run that ENDS publishes its objective vector. WHEN a run ends is untouched.** `FirstFlightKo` keeps its meaning to the tick — the conservative reading of a wreck that must not keep integrating — but every judge still open when the loop stops is asked before the report | every `sim/missions/*.fbm` keeps its exit code and **every telemetry column byte-identical**; the `events.log` delta is ADDITIVE except where an already-owned rule (the shoot-down that explains the impact) finally applies; deterministic over `--threads 1/2/4` |
| **E14** | **The new post comes out of a channel the judge already writes**, like every other one | `aimErrM` on `stores DELIVERY` (`missions/FBMissionRunner.cpp`, `LogStoreImpact`). **No new telemetry column, no new event, no new roster field** |

**Why a pair and not a sixth summand — the decision, with its reason.** A summand would have priced one
metre of aim error in shot-geometry points, and §1.2's whole argument is that such an exchange rate is a
**standing offer** and a search procedure is a machine for accepting one cheaply. The tree's own answer
to a rate it does not want to name is to make it infinite (a lexicographic level) or to refuse the
comparison (the non-dominated archive of §3.4 B). Ordering the two currencies lexicographically would
have required saying which of them is worth more, and nothing in this tree says that — the same argument
that forbids ranking the nine objective kinds at level M. **Domination says neither**: it orders
everything that is comparable and stays silent on everything that is not, so a doctrine can never buy
aim error with air craft, nor air craft with aim error, in either direction.

**Where the two currencies DO meet, and what it costs.** A unit that both fights and bombs (an escort
that also pickles) can be incomparable to itself one lever later, and an incomparable pair scores half a
point in §1.4's pairwise domination — the same half a point an exact tie scores. That is the price, it
is paid in RESOLUTION and never in order, and it is reported per tournament: the `decided at level`
line counts a C-comparison only when `compare_craft` actually decided.

**The gate grows the same arm.** §1.3's engagement gate is `eng_shot_s ≥ 0 ∨ eng_lock_s ≥ 0`, which no
striker in the ten campaigns satisfies — so without a ground arm the whole ground gradient is
unreachable behind a `GATE`. The arm is a published `stores DELIVERY` of this unit's own store: the
same class of fact as the two air columns (a record that survives the engagement), unfarmable, and the
identical channel the item is computed from, so gate and item cannot disagree.

---

### 9. Round `E7` — S1 and S2 are asked in disjoint alphabets, and that is an instrument defect

**Added 2026-07-30, before the round's first run and before the field it changes was touched.** §§0–8
are untouched. This section adds three contracts, and the measurement that forces them needs **no runs
at all** — it is two files read against each other:

| | the keys its members DIFFER in |
|---|---|
| S1's fixed field (`variants-bvr.txt`, six members) | `pilot_shot_rtr`, `pilot_lock_nm`, `pilot_react_s` |
| the genome (§2.1, `fb_evolve.GENES`) | `pilot_energy_frac`, `pilot_cover_frac`, `sort`/`dl`, `pilot_attack_bias_s`, `pilot_attack_ccip_m` |
| **intersection** | **empty** |

All six members carry `dl=off sort=""`, i.e. the field names the one gene it touches and holds it
**constant**, so no member displaces any gene at all. `informative = S1 ∧ S2` is therefore a conjunction
of two questions about **different things**: S1 asks whether a cell's outcome is spread over six
doctrines the evolution cannot express, S2 whether the baseline moves under levers of the genome. An
empty conjunction is then a property of the INSTRUMENT before it is a property of any cell — and the
tree has already written the symptom down twice without naming the cause (`E4`: *"S1 and S2 pass on
different cells and `informative` is their conjunction"*; `E5` D9, which read the same emptiness as a
ground-versus-air problem and proposed to leave it).

| # | Contract | Acceptance / measurement anchor |
|---|---|---|
| **E15** | **A fixed field is COMMENSURATE with the genome or it does not gate it.** For every gene of §2.1 at least one member of the field must displace that gene from the field's own baseline. Commensurability is CHECKED mechanically out of `fb-gym --pilot-keys` and `GENES`, never asserted in prose | the arena refuses to print an S1 verdict against an incommensurate field and names the genes no member displaces. Today that list is four of five |
| **E16** | **The field is EXTENDED, never rewritten, and the extension may not consult a result.** The six BVR members stay byte-identical; members are added until E15 holds. A member is one lever, and its value is argued from the GENE's meaning and its declared band alone — no cell, no campaign, no run output may appear in the reason for a member | the proof is the COMMIT ORDER: the extended field is committed **before** the first run that reads it, so no number of this arena can have influenced a member. `git log` is the audit, not this paragraph |
| **E17** | **What commensurability costs is booked in advance, not discovered afterwards.** (a) S1 and S2 stop being independent — they were independent only because they were disjoint, which is the defect. What each still adds is stated: S1 is DISTRIBUTIONAL (no outcome class owns the field), S2 is DIFFERENTIAL (the baseline is not in a flat spot), and a cell can pass either without the other. (b) The field becomes a function of the ALPHABET, so growing the genome moves S1 and cross-round S1 numbers are comparable only within an alphabet version | every S1 number published from here carries its field's own line count; `E2`–`E6`'s S1 numbers keep their date and are not restated |

**What this round may NOT do, stated in advance because it is again the cheap way out.** A member added
at a gene's far rail (`pilot_attack_bias_s = 10 s`, which throws a bomb 2.3 km wide) would split almost
every strike cell into two outcome classes and buy S1 passes wholesale. The rails belong to the LEVER
set, whose job is to sweep a band; a member of the fixed field must be a doctrine somebody argues for in
a debrief, which is the rule `variants-bvr.txt` was already written under and which is why its `slowhand`
is 4.0 s and not 60. **A field that passes S1 everywhere has stopped measuring, exactly like one that
passes it nowhere.**

---

## State

**Built, round `E1` (2026-07-29).** The four contracts are implemented and measured; two of the five
genes are live, two are BLOCKED by a named gap in another file, and the arena passes.

| Piece | Status | Anchor |
|---|---|---|
| **Level M's input** — `mission OBJECTIVE unit=… kind=… state=met\|unmet\|violated`, one line per declared objective | **BUILT** at the ONE point every conclusion passes through (`FBMissionMonitor::Conclude`), not at `Finalize` — see the deviation note below | [MESS] 137 missions: **432/432 telemetry files byte-identical**, 77 `events.log` unchanged, 60 gained exactly **136** `OBJECTIVE` lines, 0 lines removed, 0 other lines moved |
| **The lexicographic fitness** `(V, M, C)` + pairwise domination | **BUILT** as `sim/tools/fb_fitness.py`, the one scorer the tournament, the arena check and the evolution runner all import | `hits landed` and `no shot` are gone; the engagement gate replaces the latter |
| **The order scalar** `V·10⁶ + M·10³ + C` | **BUILT** — order-isomorphic because \|C\| < 500, which is what gives the attribution instrument its bands back without a second fitness | `fb_fitness.order_scalar` |
| **The genome's `Free`/`Scale` split** | **BUILT** in `FBPilotTuning`, plus `fb-gym --pilot-keys` so the runner reads its alphabet out of the ONE table | 20 keys, 2 of them `Scale`; two `static_assert`s |
| **G4 `pilot_energy_frac`** (Scale × `BfmCornerSpeedKt`, 0.7…1.2) | **BUILT** — the BFM throttle's insisted-on speed | [MESS] `genome-scale-flown.fbm`: moves the throttle on **2,979 of 3,001** ticks, max \|Δthr\| **0.680** |
| **G2 `pilot_cover_frac`** (Scale × the weapon's own binding time, 0…3) | **BUILT and MEASURED INERT on every arena this round flew** | [MESS] `flt_defer_s` = 0.0 in **132 of 132** unit traces over 12 runs at `xmirror --flight 2`. E-6 predicted it; this is the measurement |
| **G3 `pilot_sort`** (`dl=` + `sort=`) | **exists unchanged** as mission text | [`formation.md`](formation.md) §§4–6 |
| **G1, G5** | **REFUSED, and the runner prints the blocker at start** rather than silently dropping them | E-5 (`formation.md` F5), E-4 (`duels.md` D3) |
| **The arena gate** `tools/fb_arena_check.py` | **BUILT**, and the arena was rebuilt until it passed | see the two tables below |
| **The archive** (`tools/fb_evolve.py`, non-dominated admission, deterministic stride sample, cap 64) | **BUILT**, with all three circling instruments | [MESS] 4 generations × P = 4 at `xmirror --flight 2`: T = 0.0000, yardstick 0.667 flat, archive 16 members |
| The attribution instrument | **consumed unchanged**, now on the order scalar | `sim/tools/fb_tournament.py` |
| `git status --porcelain sim/assets` before and after every evolution run | **BUILT as a check, not a rule** — the runner refuses to start dirty and prints VOID if it ends dirty | both runs: clean/clean, `verify-models` green |

### The two exhibits, re-measured

**Exhibit C — the prediction was RIGHT about the mechanism and 40 % HIGH about the number.**
[MESS, 2026-07-29, `gun-bfm` / `gun-turning` / `mig29-gun`, read off `events.log` + the last telemetry row]

| Claim | Predicted | Measured | Verdict |
|---|---|---|---|
| a bundle is one TICK's rounds, not one squeeze's | 10 rd/tick at 6,000 rd/min | `gun BURST rounds=1` → `5` → `10` at 0.1 s spacing (the spool-up ramp, then the full rate) | **confirmed, source-exact** |
| bundles per second of continuous fire | 10 | 10 — every burst span is a whole number of 0.1 s ticks; the F-16's spans are 0.5 s = `BfmGunBurstS` = 5 bundles per squeeze | **confirmed** |
| `NoteHit()` once per bundle | 1 | `dmg_hits` = `gun HIT` line count exactly (8/8, 24/24, 23/23) | **confirmed** |
| **fitness per second of on-target fire** | **1,500** | `gun-turning` 24 hits / 4.0 s = **900** · `gun-bfm` 9 / 1.3 s = **1,038** · `mig29-gun` 23 / 2.7 s = **1,278** | **the derivation is an UPPER BOUND, not a rate.** `kMinReportedHits` 0.1 filters the bundles that pass wide, so 60–85 % of bundles score |
| the ≈9,000-point endpoint (full drum, jet flies on) | ~60 bundles | not reproduced: `mig29-gun` kills at **23** hits on 67 of 150 rounds | the endpoint stands as a [TODO] on the 571 m case, and the round it was derived from is not in `sim/missions/` |

**The reading:** the defect is real and the fix (§1.3 removes the counter) is right — 900 points per second of trigger is still 0.9 kills per second in the old currency, which is the whole complaint. The specific figure 1,500 is a ceiling nobody reaches and this file now says so.

**Exhibit A — it did NOT turn around, and that is the round's most important finding.**
[MESS, `tools/variants-flight.txt`, both seats, `--flight 2`]

| geometry | old weighted fitness | new order | margin, and what carries it |
|---|---|---|---|
| `mirror` | `f16_solo` **1097.8** > `f16_net` 977.1 | `f16_solo` **1.000** > `f16_net` 0.625 | **0.9 points of `shot lead`** on an exact tie at V = 4 and M = 2 in all 8 runs. Was 120.7 points of `hits landed` |
| `split` | `f16_net` 940.9 > `f16_solo` 770.0 | `f16_net` **V = 4.25, M = 2.25** > `f16_solo` V = 4.00, M = 2.00 | the cooperative doctrine is strictly ahead at BOTH deciding levels; the head-to-head win rate ties 0.750 because each side wins its west seat |

**What that means, stated without softening.** The mechanism §1.1 named is gone: the 120.7-point gap
collapsed by a factor of 134, and `hits landed` no longer exists. But the DIRECTION did not flip on
`mirror`, because level C still orders a field that ties at V and M — and two floats are never equal.
On the geometry that decides anything (`split`) the cooperative doctrine IS ahead where it counts.

**The consequence for the spec is stated in §Gaps as E-11:** §Knowledge 1's claim that the reformed
fitness is *"honestly silent"* in a saturated arena is **false as specified**. §1.4's pairwise
domination consults C on a tie, and C always differs. The tool now says so instead — every tournament
prints `decided at level: V n  M n  C n  exact tie n`, and refuses to let the ranking be read when
V and M decided nothing:

```
decided at level:  V 2   M 0   C 18   (of 20 runs)          mirror,  --flight 2
decided at level:  V 14  M 0   C 6    (of 20 runs)          split,   --flight 2
```

**Exhibit B — resolved.** [MESS, `tools/variants-mixed.txt`, `mirror`, 30 runs, `--timeout 420`]
`f16_base` and `f16_long` now carry the IDENTICAL result (V = 2.40, M = 1.40 both): the 900 points of
bursts that killed nobody are not an outcome any more, so nothing overturns anything. The two are
ordered by craft, which is what craft is for. `latelock` prints `GATE x8` — eight of its ten runs
never engaged, and the gate says so where the old −250 could be repaid.

### The arena, old against new

Both measured with `tools/fb_arena_check.py`, the FIXED yardstick (`variants-bvr.txt`, six lines, both
seats = 60 side-keys per geometry) and the nine declared doctrine levers.

| | geometries | informative | verdict |
|---|---|---|---|
| **old** (`mirror`, `split`) | 2 | 1 | **REFUSED** — S4 2 < 6, S5 1 < 3 |
| **new** (8, spanning aspect / energy / detection / weapon obligation) | 8 | **4** | **PASSED** |

| geometry | distinct classes | modal | movers of 9 | S1 | S2 |
|---|---|---|---|---|---|
| `mirror` | 1 | **100.0 %** | 1 (`react-slow`) | NO | NO |
| `stern` | 1 | 100.0 % | 0 | NO | NO |
| `offset` | 3 | 93.3 % | 1 (`beam-hard`) | NO | NO |
| `xsplit` | 3 | 41.7 % | 2 | ok | NO |
| **`far`** | 4 | **40.0 %** | **4** | ok | ok |
| **`xmirror`** | 3 | **56.7 %** | **5** | ok | ok |
| **`split`** | 3 | **56.7 %** | **3** | ok | ok |
| **`xclose`** | 3 | **60.0 %** | **3** | ok | ok |

`mirror`'s 100 % / 1-of-9 is worse than the 94.7 % §4.1 quotes from the catalogue row, and it is the
geometry every published F-16 doctrine result in this tree was measured on. **What desaturated the
arena was not the geometry, it was the AIRFRAME:** the three best cells are the two mixed ones and the
long approach, and eleven F-16-vs-F-16 candidates were flown and rejected before that was accepted
(`low` 66.7 %/2, `farsplit` 70.0 %/2, `farsplit2` 66.7 %/2, `farhigh` 73.3 %/2, `hardsplit` 90.0 %/2,
`fast` 70.0 %/1, `closesplit` 80.0 %/1, `high` 80.0 %/1, `close` 96.7 %/0, `lowfar` 96.7 %/0,
`farfast` / `farlowsplit` / `beam` 100 %/0). That is §4.2's own weapon-obligation axis arriving at its
consequence: two identical aircraft with the same weapon draw, and no geometry fixes that.

---

## State — round `E2` (2026-07-29): the genome and the arena cut

`E1` closed with E-13: *"the genome and the arena do not intersect, and the first evolution run
measured a total tie — every individual 0.500."* That reading was **half right and the wrong half was
the diagnosis.** Measured per gene, three of the five levers do grip; what did not grip was the
COMPARISON. Both halves are below, in the order the round measured them.

### 1. Per gene: does the lever grip at all?

Each gene against the channel §2.1 names for it, at both of its rails, on ONE mission — no tournament,
no aggregation, nothing that could hide a movement or manufacture one.

| Gene | Where it was measured | Rails | The channel §2.1 names | Verdict |
|---|---|---|---|---|
| **G1** `pilot_flight_shape` | `bvr-intercept` + the `set` line | — | — | **BLOCKED, and it is not even a key.** `set pilot_flight_shape 1` → `module SET_INVALID_VALUE … reason="no such pilot parameter, or out of range"` + `mission SET_REJECTED` at t = 0.0, **exit 1**, the run never starts. Cause unchanged: [`formation.md`](formation.md) F5 |
| **G5** `pilot_emcon_frac` | `bvr-intercept` + the `set` line | — | — | **BLOCKED, identically.** `set pilot_emcon_frac 0.5` → the same two lines, **exit 1**. Cause unchanged: [`duels.md`](duels.md) D3 |
| **G4** `pilot_energy_frac` | `bfm-basic`, `bfm-offset`, and a live 1v1 merge (both seats `set task bfm`) | 0.7 / 1.0 / 1.2 | `bfm_ctrl_s` and `eng_es_min/es_start` "unchanged to the digit" = inert | **GRIPS in the `bfm` phase, and hard.** `bfm-basic` `bfm_ctrl_s` **267.4 / 267.9 / 268.8** s and `bfm_es` **17 397 / 17 392 / 17 378** ft, both monotone in the gene; `bfm-offset` 293.5 / 293.5 / 293.6 and 17 393 / 17 384 / 17 375. Against a LIVE opponent the movement is a different order: head-on merge `bfm_es` **42 690 / 44 976 / 46 360** ft (span 3 670 ft = 8.6 %), `bfm_lock_s` 48.7 / 46.3 / 43.8 s; mixed merge `bfm_es` 9 156 / 25 176 / 13 574 ft and the OPPONENT's `bfm_ctrl_s` 59.3 / 0.0 / 14.7 s |
| **G3** `sort` (`dl=` + `sort=`) | 2v2 F-16 against 2v2 F-16, 420 s | six alleles | the assignment source and the split | **GRIPS, and the allele table was wrong.** `SORT_ASSIGN` lines per element and their `src=`: (off,none) **0**, none · (off,left) **2**, contract · (off,near) **6**, contract · (on,none) **41**, cooperative · (on,left) **41**, cooperative · (on,near) **41**, cooperative. **The three `on` rows are identical to the last digit** — `FBFlightPicture::Assign` ranks the cooperative sort above the briefed contract, so a contract beside a live net is dead text |
| **G2** `pilot_cover_frac` | five tournament geometries, `--flight 2`, `dl=on` on BOTH sides | 0 / 1.0 / 3.0 | `flt_defer_s` > 0 **and** `flt_both_s` = 0 | **GRIPS on `split`, and only on the lower half of its band.** See the table below |

**G2, per geometry — and this is E-6's correction.** The gene holds the trigger only while a mate is
already bound, so it needs a BINDING LONGER THAN THE SPREAD between the two members' firing solutions.
The AIM-120's binding is its time-to-active and that is a function of LAUNCH RANGE, so the geometry
decides whether the gene has anything to act on:

| geometry, `--flight 2` | `flt_both_s` at rail 0 | `flt_defer_s` at 0 / 1.0 / 3.0 | craft C |
|---|---|---|---|
| `mirror` | 0.4 · 1.6 s | 0.0 / 0.0 / 0.0 | 432.9 flat |
| `far` | 1.5 · 1.7 s | 0.0 / 0.0 / 0.0 | 418.6 flat |
| `xclose` | 0.0 · 0.3 s | 0.0 / 0.0 / 0.0 | 454.5 flat |
| `xmirror` | 0.0 · 0.0 s | 0.0 / 0.0 / 0.0 | 362.5 flat |
| **`split`** | **5.6 · 4.6 s** | **0.0 / 6.3 / 6.3** | **525.5 / 525.1 / 525.1** |

E-6 read *"the gene is correct and its arena does not exist"* off `xmirror`, whose east seat is a
MiG-29 — an aircraft with **no datalink at all**, so the element being measured never had a mate's
bound-bit to act on. It exists: it is the geometry with a 12 000 m / 500 kt seat, where the launch is
far enough that the round binds 5–6 s instead of 0.3. **The upper half of the band is still flat** —
at 1.0 the cap already outlasts the mate's binding, so 3.0 buys nothing. A gene with half a live band
is reported as such rather than as a working gene.

### 2. The tie was the COMPARISON, not the genome

[MESS, `split --flight 2`, the population round robin of a four-individual generation, 12 pairings]

| | value |
|---|---|
| west key, craft | C = +505.5 … +526.7 — **four distinct values for four genomes** |
| east key, craft | C = **+69.0 in 12 of 12** |
| cross-seat comparison | west wins **12 of 12** (over the whole run, 132 of 144) |
| the seat is worth | **457 craft points**; the genome **21.2** |

§1.4 design C compares "the two units' keys" **inside one run**, i.e. ACROSS THE SEATS. Where the seat
carries the key, that comparison returns the seat in BOTH mirrored runs, each variant therefore takes
exactly one point of two, and **the whole population scores 0.500 by construction, whatever it
carries.** That is E-13's total tie, exactly and arithmetically.

**What was built:** `fb_fitness.match_points` — a MATCH is the pair of mirrored runs and the comparison
inside it is **seat against the same seat** (A-as-west against B-as-west, A-as-east against B-as-east,
summed and halved). It is the aggregation, not the order: `(V, M, C)`, `compare` and `pair_points` are
untouched, and no craft value crosses a level. Booked as deviation **D6**.

**The A/B, on the SAME telemetry — no re-flight, only the comparison changed:**

```
xfarsplit --flight 2, generation 0   cross-seat  0.500 0.500 0.500 0.500 0.500 0.500
                                     same-seat   0.700 0.400 0.400 0.400 0.700 0.400
xfarsplit --flight 2, generation 2   cross-seat  0.500 × 12
                                     same-seat   0.773 0.417 0.417 0.417 0.417 0.417
                                                 0.417 0.773 0.500 0.500 0.500 0.227
```

The published tournaments do NOT move: `variants-flight` on `mirror --flight 2` is `f16_solo` **1.000**
> `f16_net` 0.625 and on `split` the three F-16 rows tie at **0.750** over `mig_pair` 0.250 /
`mig_solo` 0.000 — the same numbers §State's Exhibit A carries. `variants-mixed` on `mirror` orders
`mig_long` 1.000 > `f16_long` 0.800 > `f16_base` 0.600 > `mig_base` 0.400 > `f16_deep` 0.200 >
`mig_deep` 0.000, with `f16_base`/`f16_long` still on the identical result (V = 2.40, M = 1.40). A
heterogeneous field does not collapse under the cross-seat rule; a homogeneous one collapses totally,
which is why the defect surfaced only in the evolution runner.

### 3. The merge — built, measured, and it does not decide a doctrine

The arena had no geometry that enters close combat, and it could not have one by accident:
**`FBPilot` has exactly ONE transition into `Phase::Bfm` and it is the briefed task at spawn.** The
intercept phase turns around at `InterceptAbortRangeNm` (5 nm) and there is no `Transition(Phase::Bfm)`
anywhere in `InterceptCommands`. So a geometry that asks a merge question has to say so, and
`fb_tournament` now carries a second unit-block **profile** (`merge`, doc/missions/duel-merge.fbm's own
box settings) plus three geometries that use it: `merge`, `xmerge`, `xmergesplit`.

**What they measure** [MESS, 70 merge runs over seven candidate geometries]:

| | |
|---|---|
| gun bursts fired, all 70 runs | **6** |
| `gun HIT` lines | **0** |
| missile launches | **0** |
| `dmg_hits` | **0** |
| what the deciding class `(2,0)` actually is | **the MiG-29 flying into the ground** — 9 of 11 east results `CRASH`, reasons "extreme attitude at ground contact" / "ground penetration" / "structure contact" |

So the merge DECIDES (`xmerge` and `xmergesplit` both pass S1 at a 50.0 % modal share where every BVR
symmetric cell sits at 100 %) and what it decides is [`pilot.md`](pilot.md) 2.9's CFIT defect class, not
a doctrine. And it fails S2 whichever alphabet it is swept with:

| geometry | levers | movers of 9 | S1 | S2 |
|---|---|---|---|---|
| `merge` (F-16 v F-16) | the declared nine | 0 | NO (100 %) | NO |
| `merge` | `levers-merge.txt` | 0 | NO | NO |
| `xmerge` (F-16 v MiG-29) | the declared nine | 0 | ok (50.0 %) | NO |
| `xmerge` | `levers-merge.txt` | **1** (`energy-low`) | ok | NO |
| `xmergesplit` | `levers-merge.txt` | **2** (`energy-low`, `energy-high`) | ok (50.0 %) | NO |

`tools/levers-merge.txt` is the nine points of the phase's OWN alphabet, because the declared nine are
`pilot_shot_rtr` / `pilot_lock_nm` / `pilot_react_s` / `pilot_beam_deg` / `pilot_abort_nm` and **not one
of them is read inside `BfmCommands`** — sweeping them on a merge geometry re-flies the identical
aeroplane nine times. The phase reads exactly five keys and only three carry a command
(`pilot_gun_tol_frac`, `pilot_gun_burst_s`, `pilot_energy_frac`); of those three, **only the energy gene
ever moves an outcome class**, because the gun never scores.

**The consequence, stated without softening: G4 has no publishable geometry.** Its lever grips (§1), it
moves the outcome class on `xmergesplit` (2 of 9), and every geometry it moves fails S2 — so §6's
*"anything measured on a geometry that failed S1–S3 is expressly NOT a finding"* applies to it. G4 stays
**blocked**, and the blocker is now three named things instead of one suspicion: no intercept→merge
transition, no `eng_*` channel in the merge (so level C is `GATE` for both sides and the fitness is
blind to everything the merge moves), and a gun that fires 6 times in 70 runs and hits nothing.

### 4. The arena, `E1` against `E2`

**At `--flight 1` with the declared nine levers — the gate as `E1` ran it. PASSED.**

```
geometry   distinct     modal modal class    movers of 9                          S1 S2 S3
far               4     40.0% (2, 1)              4 shoot-early,shoot-late,…      ok ok n/a
merge             1    100.0% (2, 1)              0 -                             NO NO n/a
mirror            1    100.0% (2, 1)              1 react-slow                    NO NO n/a
offset            3     93.3% (2, 1)              1 beam-hard                     NO NO n/a
split             3     56.7% (2, 1)              3 shoot-early,shoot-late,…      ok ok n/a
stern             1    100.0% (2, 1)              0 -                             NO NO n/a
xclose            3     60.0% (2, 1)              3 shoot-early,react-slow,…      ok ok n/a
xfarsplit         4     40.0% (1, 0)              2 shoot-late,commit-near        ok NO n/a
xmerge            2     50.0% (2, 0)              0 -                             ok NO n/a
xmergesplit       2     50.0% (2, 0)              0 -                             ok NO n/a
xmirror           3     56.7% (2, 1)              5 shoot-early,shoot-late,…      ok ok n/a
xsplit            3     41.7% (3, 2)              2 shoot-late,commit-near        ok NO n/a

S4 12 geometries (>= 6) ok · S5 4 informative (>= 3) ok [far, split, xclose, xmirror]
S6 0 identical pair(s) ok · ARENA: PASSED
```

**At `--flight 2` with the GENOME's own alphabet — the check E4 actually needs (E-12). REFUSED, and the
refusal is the finding.** `tools/levers-genome.txt` is the three live genes at three points each;
`fb_arena_check.py` gained `--flight N`, because two of the three live genes do not exist below a
two-ship and an arena passed at `--flight 1` says nothing about the arena a `--flight 2` run uses.

```
geometry   distinct     modal modal class    movers of 9                          S1 S2 S3
far               2     71.7% (4, 2)              1 sort-net                       NO NO n/a
merge             1    100.0% (4, 2)              0 -                              NO NO n/a
mirror            2     90.0% (4, 2)              0 -                              NO NO n/a
offset            2     86.7% (4, 2)              3 sort-net,sort-left,sort-near   NO ok n/a
split             2     70.0% (4, 2)              0 -                              NO NO n/a
stern             1    100.0% (4, 2)              0 -                              NO NO n/a
xclose            2     63.3% (4, 2)              0 -                              NO NO n/a
xfarsplit         4     48.3% (3, 1)              3 sort-net,sort-left,sort-near   ok ok n/a
xmerge            2     50.0% (4, 0)              0 -                              ok NO n/a
xmergesplit       2     50.0% (4, 0)              1 energy-low                     ok NO n/a
xmirror           2     70.0% (4, 2)              0 -                              NO NO n/a
xsplit            4     60.0% (4, 2)              0 -                              ok NO n/a

S4 12 geometries (>= 6) ok · S5 1 informative (>= 3) NO [xfarsplit] · ARENA: REFUSED
```

| | at `--flight 1`, declared nine | at `--flight 2`, genome alphabet |
|---|---|---|
| informative | **4** | **1** (`xfarsplit`) |
| verdict | PASSED | **REFUSED** — S5 1 < 3 |

Two rows are worth reading beside each other. **A 2v2 is MORE saturated than a 1v1, not less:** at
`--flight 2` eight of the twelve geometries put every run in `(4,2)` — both members TIMEOUT with
`survive` met — where at `--flight 1` the same geometries spread three and four classes. And `offset`,
which the arena keeps as a documented dead cell, is the only other geometry the genome moves at all
(3 of 9, again all three sort alleles) — on a field that sits at an 86.7 % modal share.

`xfarsplit` is the one cell the genome moves: **3 movers of 9, all three of G3's alleles**, over the
classes (3,1)/(4,2)/(6,4), on a field that spreads **4 classes at a 48.3 % modal share**. It asks three
of §4.2's five axes at once — the long run-in, the energy split and the weapon obligation. **Seventeen
candidate geometries were screened for a second one and sixteen returned 0 or 1 mover**
(`deepsplit`, `offsplit`, `xdeepsplit`, `xoffset`, `xoffsplit`, `xstern`, `xfar` 1, `farsplit` 1,
`xfarsplit2` 1, `xfarhigh`, `xfarlow`, `xfaroffset`, `xfarstern`, and the four merge cells), so the
arena's yield **under the alphabet that is actually evolved** is 1 of 12, not 4 of 12.

**Where the temptation to build the arena around a gene was, and what was done instead.** Three places,
all of them declined and all of them recorded here so a reader can check the decision rather than trust
it:

1. **A merge yardstick for S1.** The BVR yardstick cannot distinguish two merge doctrines, so a merge
   geometry is saturated against it. Writing a merge-flavoured FIXED FIELD would have made `xmerge`
   informative on paper. Not done — `--variants` was left at `variants-bvr.txt` for every number above,
   and only S2's lever set was given a phase-appropriate file, which is what E-12 already asked for.
2. **Lowering `kMoversMin` from 3 to 2**, which would have made `xmergesplit` informative and published
   G4. Not done, not proposed: the gate is not loosened, and the movers it would have counted are the
   gene's own two rails.
3. **Selecting the evolution geometry by which ALLELE wins on it.** Not done: `xfarsplit` was selected
   on S1 (a property of the fixed yardstick, which no genome can influence) and on S2's mover COUNT.
   Which allele wins was read afterwards and is reported below.

### 5. The evolution run — the population does not tie

`tools/fb_evolve.py --geometry xfarsplit --flight 2 --generations 4 --population 6`, 4 gens × 6, archive
20 members, 7 m 0 s.

| generation | fitness distribution (sorted) | spread |
|---|---|---|
| 0 | **0.700** 0.700 0.400 0.400 0.400 0.400 | 1.75× |
| 1 | **0.600** 0.600 0.500 0.500 0.400 0.400 | 1.50× |
| 2 | **0.773** 0.773 0.500 0.500 0.500 0.227 | **3.40×** |
| 3 | **0.650** 0.650 0.550 0.550 0.550 0.550 | 1.18× |

Not a mean — the distribution, because a mean of six 0.500s and a mean of {0.773, 0.227, …} are the
same number and only one of them is a measurement.

**And it is decided by the RESULT, not by craft.** Over the four population round robins,
**312 seat comparisons: V 96 · M 0 · C 152 · exact tie 64.** Ninety-six comparisons — 31 % — turn on
the judge's verdict; the outcome classes over all 720 side keys are (6,4) 218 · (2,0) 218 · (3,1) 194 ·
(4,2) 90, i.e. four classes with no modal class above 30 %.

**Which gene moves the outcome, per geometry** — the round's third deliverable, and two of five rows are
a refusal:

| Gene | The geometry it moves the outcome on | Evidence |
|---|---|---|
| **G3** `sort` | **`xfarsplit`** | 3 movers of 9 at `--flight 2`, all three alleles, classes (3,1)→(4,2)/(6,4). The evolution run's ranking is carried by it: every generation's bottom is a `sort=left` or a `dl=on` row and its top is `dl=off` |
| **G2** `pilot_cover_frac` | **none yet — it moves level C on `split`, never V or M** | `flt_defer_s` 0 → 6.3 s and C 525.5 → 525.1, i.e. −0.4 craft points. §6: a rank change inside level C is expressly not a finding. Its band is also half dead (flat above 1.0) |
| **G4** `pilot_energy_frac` | **none — `xmergesplit` is the only one and it fails S2** | §3 above |
| **G1**, **G5** | **none — not keys** | §1 above |

**The champion, and why it is not published as a doctrine shift.** `g0_00`
(`pilot_cover_frac=1.11 pilot_energy_frac=0.7`, `dl=off`, no contract) held the championship in all four
generations, the fixed yardstick was **0.583 flat**, T = 0.0000 and the trajectory distance to a
champion three generations back is 0.0000 — a FIXED POINT, not a cycle, and §6's binding rule applies:
the mechanism section would have to name a chain of published channels, and the one channel that moved
(G3) moved on a geometry whose informative status rests on a single lever family. **No §1 is published
this round.** The product is the two defects above and the arena row.

### Deviations from the spec, found while building it

| # | The spec says | What was built, and why |
|---|---|---|
| **D1** | E-1's line is emitted at `Finalize` | It is emitted in `Conclude`, the one point BOTH conclusion paths pass through. `Finalize` alone would publish nothing for a unit that FAILs in `Tick` (a shoot-down, a `no_fire` violation, a touchdown off the runway) — and those units have an M worth counting inside their own outcome class |
| **D2** | S3 is *"unchanged, cited"* | **S3 is not computable on this arena.** Its instrument perturbs a GENERATED catalogue deck via `gen_air_decks.py`; the arena flies the F-16 and the MiG-29, whose decks are FlightBox's read-only model copies under principle 1 and carry no declared-ignorance band. The check prints `n/a` with the reason and never a 0.0 — a no-op is not a measurement. S3 stays live for a catalogue-row arena, where it was defined |
| **D3** | §Knowledge 1: the reformed fitness is *"honestly silent"* in a saturated arena | False — see Exhibit A above and E-11 |
| **D4** | §2.2 property 3: the runner prints `evolving 5 genes of 22 pilot keys; 17 refused as airframe-owned` | It prints `evolving 3 genes of 20 pilot keys; 18 keys not in the genome; 0 non-pilot keys reachable` plus one line per BLOCKED gene with its blocker. 3 not 5 because G1/G5 are blocked by E-4/E-5; *"refused as airframe-owned"* was not used because it is not true of the other 18 — they are pilot keys that simply are not genes |
| **D5** | §4.2 S2 counts *"3 of the 9 doctrine levers"* | The nine are `pilot_*` intercept keys and are **not the genome**. [MESS] `xmirror` passes S2 with 5 of 9 while all four gene alleles produce the IDENTICAL key in 12 of 12 runs. `fb_arena_check.py --levers FILE` now sweeps whatever alleles are being evolved; the fixed nine remain the default. Booked as E-12 |
| **D6** (`E2`) | §1.4 design C: *"per PAIRING, compare the two units' keys directly"* — i.e. the two SIDES of one run | **The comparison is seat against the SAME seat**, across the two mirrored runs (`fb_fitness.match_points`). The spec's form measures the SEAT wherever the seat carries the key, and then returns the same winner in both mirrored runs, so every variant takes one point of two and the field ties at 0.500 by construction — E-13's total tie, arithmetically. [MESS, `split --flight 2`] west C = +505.5…+526.7, east C = +69.0 in 12 of 12, seat worth 457 craft points against the genome's 21.2. The ORDER is untouched: `(V, M, C)`, `compare` and `pair_points` are the same functions and no craft value crosses a level. The published tournaments do not move (§State 2) |
| **D7** (`E2`) | §4.2 assumes one geometry table, swept at one force ratio | A geometry now carries its **profile** — `bvr` or `merge` — because the phase the pilots fly it in is part of the question and cannot be a flag on the runner: `FBPilot` has exactly one transition into `Phase::Bfm` and it is the briefed task at spawn. `fb_arena_check.py` gained `--flight N` for the same reason: two of the three live genes do not exist below a two-ship, so a gate passed at `--flight 1` says nothing about the arena a `--flight 2` run uses |

---

## State — round `E3` (2026-07-29): the merge was a defect, and now it is a gunfight that cannot kill

`E2` closed with two readings about the merge: E-14 (*"G4 has no publishable geometry"*, three blockers)
and E-15 (*"what the merge geometries decide is a CFIT"*). Both were right, and this round measured what
was underneath them. Nothing in the fitness, the genome, the archive or the gate changed; the arena's
one profile line and one line of `systems/FBFlightControl` did.

### 1. The CFIT was one line of the airframe layer, and it was ONLY ever the MiG

[MESS, `fb_arena_check --geometry merge --geometry xmerge --geometry xmergesplit --levers
levers-merge.txt`, 120 runs per pass]

| | monitor KOs | who | gun bursts | `gun HIT` |
|---|---|---|---|---|
| before | **77** | **MiG-29 in 77 of 77** (38 `ATTITUDE_CONTACT`, 37 `CFIT`, 2 `STRUCTURE_CONTACT`) | 191 | **0** |
| damper on the `Manual` path | **0** | — | 386 | 0 |
| + the merge profile briefs the GUN control position | **0** | — | **2 652** | **897** |

The F-16 was never KO'd in a merge cell, in either seat, before or after. The seat was excluded first:
the MiG dies in the WEST seat too (`xmerge` mirrored: CRASH at t = 351.2), and a MiG-versus-MiG merge
survives at 420 s, so it was neither the seat nor the airframe alone but the airframe under a live
opponent.

**The isolating experiment is one aeroplane and no fight at all** — a MiG-29 on `set task bfm` with the
nearest hostile 100 km away, i.e. `FBPilot`'s cold anchored search and nothing else, 300 s, three start
altitudes; the same mission text one module over for the F-16:

| cold BFM search, 300 s | mean `bfm_gcmd` | mean \|bank\| | p95 \|VS\| | 1 000 / 1 500 / 3 000 m |
|---|---|---|---|---|
| **MiG-29, as it was** | **4.57 g** | **76°** | **183 m/s** | CFIT at **12.7 / 15.1 / 188.6 s** |
| MiG-29, damper at the hand stick | 1.11 g | 24° | 4 m/s | survives all three |
| F-16 | 1.22 g | 40° | 9 m/s | survives all three |

`KqDamp`/`KpDampRoll` are the SAU-451 DAMPER and bound only on `FBFlightControl`'s FLCS path, while BFM
commands `Manual` — the aircraft fought every close engagement with its damper off, the pilot's F-16 g
loop rang against an undamped short period, and the fight ended in the ground.
[`pilot.md`](pilot.md) §5.10a carries the mechanism and the derivation; `CLAUDE.md` principle 1 was not
touched — `sim/assets` is byte-identical and `verify-models` green.

**The temptation, named and declined:** the fastest way to make these numbers pretty was the deck. The
MiG's own `Cmq`/`Cnr` would have damped the short period too, and nobody would have noticed. It is not
a delta with a source, a better mission result is expressly not evidence, and the defect was on our side
of the seam — the same seam the two earlier screws were fixed at.

### 2. What the merge decides now: nothing, and that is the honest reading

| geometry, `levers-merge.txt` | before: distinct / modal / class | after: distinct / modal / class | S1 |
|---|---|---|---|
| `merge` | 1 / 100.0 % / (2,1) | 1 / 100.0 % / (2,1) | NO → NO |
| `xmerge` | 2 / **50.0 %** / (2,0) | 1 / 100.0 % / (2,1) | **ok → NO** |
| `xmergesplit` | 2 / **50.0 %** / (2,0) | 1 / 100.0 % / (2,1) | **ok → NO** |

The two cells that passed S1 passed it **on the CFIT** — the deciding class `(2,0)` was one seat rank-0
for having been lost to nobody. With the defect gone the merge is three saturated cells, `G4` has 0
movers of 9 on all three, and `E2` §4's declined temptations stand unchanged: the gate was not loosened,
`kMoversMin` was not lowered, and no merge-flavoured yardstick was written. The rest of the arena did
not move at all — the full 12-geometry gate at `--flight 1` with the declared nine is identical to
`E2`'s table on every other row (`far` 4/40.0 %, `split` 3/56.7 %, `xclose` 3/60.0 %, `xmirror`
3/56.7 %, `xfarsplit` 4/40.0 % (1,0), `xsplit` 3/41.7 % (3,2)) and still **PASSES with 4 informative** —
the merge cells never counted toward S5, because they always failed S2.

### 3. G4 now has a lever in the merge, and no outcome to move it

The merge profile briefs the GUN control position (`pilot_bfm_ctrl_min_nm 0.15` / `_max_nm 0.40`).
The reason is not the gene: `Phase::Bfm` ends its tick in `BfmGunfire` and has **no missile shot at
all**, so the default 0.5–1.5 nm band is a holding position for a weapon the phase cannot employ, and it
lies outside the EEGS funnel (600–3 000 ft). [MESS, `xmergesplit`] with the missile band the MiG held
**132.4 s of control position at a median 2.64 nm and \|ata\| 0.4°** with `gun_in_funnel` 0 in 4 200
ticks and 0 triggers — a perfect tail chase that can never squeeze. `missions/gun-bfm.fbm`'s header
already carries the rule and the two numbers are its own.

Three-point sweep of `pilot_energy_frac` at 0.7 / 0.95 / 1.2, both seats, after both changes:

| geometry | MiG `bfm_es` (ft) | MiG `bfm_ctrl_s` (s) | rounds fired | hits landed on the F-16 |
|---|---|---|---|---|
| `merge` (F-16 v F-16, east seat) | 35 104 / 40 608 / 48 184 | 0.0 / 0.0 / 0.0 | 0 / 0 / 0 | 0 / 0 / 0 |
| `xmerge` | 21 552 / 38 384 / 26 179 | **145.4 / 0.0 / 0.0** | **150 / 12 / 12** | **10 / 0 / 0** |
| `xmergesplit` | 33 592 / 39 358 / 42 492 | 34.2 / 31.9 / 2.1 | **150 / 150 / 0** | **23 / 23 / 0** |

The gene grips hard and the reading is mechanical: **the LOW rail converts.** Insisting on 0.7× corner
keeps the MiG inside the F-16's turn and empties the drum into it; 1.2× (above the overspeed clamp)
abandons the fight — `ctrl_s` collapses to 2.1 s and nothing is fired.

**And the outcome class does not move, because the gun lands and cannot kill.** [MESS,
`xmergesplit`, energy 0.7] 23 `gun HIT` lines carrying **6.37 rounds of the 150-round drum** at a mean
miss of 3.41 m; the F-16 ends with 3 systems degraded, one failed component and `dmg_effective` **1.00**.
Level V sees `TIMEOUT` on both sides, level M sees `survive` met on both, and the whole sweep sits in
one class. So §6's binding rule applies to G4 for the second round running and **no §1 is published** —
but the blocker is now ONE named thing instead of three, and it is not a defect of this arena: it is
`weapons.md`'s kinetic path and `pilot.md` 2.4.

### 4. What this round did NOT do

- The deck was not touched (`git status --porcelain sim/assets` empty, `verify-models` green).
- The gate was not loosened: `fb_arena_check.py` is unchanged, and the merge cells are reported as
  REFUSED rather than re-classified.
- No geometry was added, removed or re-selected. The three merge cells are `E2`'s.
- 134 of 139 stock missions are byte-identical; the five that move are all MiG-29 and each is justified
  in [`pilot.md`](pilot.md) §5.10a / [`duels.md`](duels.md) D1.

---

## State — round `E4` (2026-07-29): the merge decides, and the gene that moves it moves a CFIT

`E3` closed with E-14: *"the gene's LEVER is now large and monotone in the merge … what is missing is an
OUTCOME for it to move"*, and one named blocker — **the gun lands and cannot kill**, plus `Phase::Bfm`
having no missile shot at all. Both are addressed this round in [`pilot.md`](pilot.md) (§5.8 the trigger's
prediction horizon, §5.11 the WVR missile shot). Nothing in the fitness, the genome, the archive or the
gate changed, and `fb_arena_check.py` is byte-identical.

### 1. The arena, same 3 × (30 + 10) runs, before and after

| geometry | before: distinct / modal / class / movers | after: distinct / modal / class / movers | S1 | S2 |
|---|---|---|---|---|
| `merge` | 1 / 100.0 % / (2,1) / **0** | 1 / 100.0 % / (2,1) / **3** (`energy-low`, `energy-mid`, `energy-high`) | NO → NO | NO → **ok** |
| `xmerge` | 1 / 100.0 % / (2,1) / 0 | **2 / 50.0 % / (3,2)** / 0 | NO → **ok** | NO → NO |
| `xmergesplit` | 1 / 100.0 % / (2,1) / 0 | 1 / 100.0 % / (2,1) / 0 | NO → NO | NO → NO |

The full per-side outcome-class distribution over the yardstick field, 60 samples per cell, which is the
number the modal share is taken in:

| geometry | before | after |
|---|---|---|
| `merge` (F-16 v F-16) | (2,1) 60 (100 %) | (2,1) 60 (100 %) |
| `xmerge` (F-16 v MiG-29) | (2,1) 60 (100 %) | **(3,2) 30 (50 %) + (1,0) 30 (50 %)** |
| `xmergesplit` | (2,1) 60 (100 %) | (2,1) 60 (100 %) |

**`xmerge` is decided in 30 of 30 runs, and every one of them the same way** — 40 `damage KILL` lines in
the arena, all against the east seat, none against the west, and **none of them by the gun** (the line
before every kill is a `DETONATION`, not a `gun HIT`). The F-16 spawns in `acm_hud` and the MiG must
switch its N019 over the bus, so the AIM-9 goes 1.9 s early; and the AIM-9's 9.4 kg warhead kills this
airframe inside 2.32 m where the R-73's 7.4 kg needs 2.08 m. That the MiG can win a close fight at all is
proved on the geometry its round is documented for and not here:
`missions/duel-merge-stern.fbm`, [`duels.md`](duels.md) row 9.

### 2. G4: the gene moves the outcome class, and the mover is a CFIT

**Answer to the question `E3` left open: yes on `merge`, and no, the gene is not free.** All three
`pilot_energy_frac` alleles change the outcome class there, which is S2's first pass on any merge cell.
What they change it BY is the reading that matters, and it is `E-15`'s finding one layer up:

| allele | class | what actually happened |
|---|---|---|
| baseline | (2,1) | both jets exchange AIM-9s at t = 1.9 s, both rounds arrive **2.7–3.4 m** out against a 2.32 m kill radius, both survive blinded, both time out at 420 s |
| `energy-low` 0.7 | (2,0) | same exchange, then the east jet flies 400 s below its own `BfmMinSpeedKt` and meets the ground — `monitor KO ATTITUDE_CONTACT` at t = 412.3 |
| `energy-mid` 0.95 | (2,0) | the same, at t = 417.2 |
| `energy-high` 1.2 | (0,0) | the WEST jet, `monitor KO ATTITUDE_CONTACT` at t = 226.6 |

Three of the arena's 43 monitor KOs are healthy aircraft and all three are these; the other **40 are jets
that had already taken a `damage KILL`** and then fell, which is `core/FBSystemHealth`'s monotonicity
doing what it says. So the honest statement is the one `E-15` taught this file to make: **a geometry
whose class is moved by a CFIT is measuring the CFIT.** G4 is therefore still not published, `kMoversMin`
was not lowered, no merge-flavoured yardstick was written, and the arena is REFUSED exactly as before
(3 geometries, 0 informative — S1 and S2 pass on different cells and `informative` is their conjunction).

### 3. What the gun did, since E-14 named it

The trigger's prediction horizon was the ROUND's time of flight and is now the SQUEEZE's
([`pilot.md`](pilot.md) §5.8). Over the same 120-run merge arena:

| | rounds fired | rounds ON TARGET | hit rate |
|---|---|---|---|
| before | 6,318 | 139.8 | 2.21 % |
| after | 5,850 | **449.1** | **7.68 %** |

**Still 0 gun kills**, and the shortfall is now arithmetic rather than a mystery: a `damage KILL` costs
**17.0** landed 30 mm rounds in one zone and a drum delivers **9.53**. The ladder was not touched and
[`weapons.md`](weapons.md) carries why.

### 4. What this round did NOT do

- The deck was not touched (`git status --porcelain sim/assets` empty, `verify-models` green).
- The gate was not loosened: `fb_arena_check.py` is unchanged and the merge cells are still REFUSED.
- No geometry was added, removed or re-selected. The three merge cells are `E2`'s.
- 130 of 139 stock missions are byte-identical; the nine that move are named in
  [`pilot.md`](pilot.md) State and [`duels.md`](duels.md), and one mission is new.

---

## State — round `E5` (2026-07-30): the arena is the ten campaigns, and it is REFUSED

Step 5 of the owner goal: evolve doctrine **over the campaign breadth**. The instrument
[`campaigns/w1-red-flag.md`](campaigns/w1-red-flag.md) said the gate did not have is built
(`tools/fb_campaign_arena.py`), the genome grew the two GROUND decisions the campaigns actually grade
(§7 E8), and the gate was pointed at **all 154 cells of the ten campaigns**. It refuses them. Nothing
in the fitness, the archive or the gate's numbers changed; `fb_arena_check.py` is byte-identical and
`sim/src/` was not touched at all.

**The one-sentence result: no doctrine shift is published, because no cell of the campaign breadth is
informative — 0 of 154 under the genome's own alphabet, and 2 of 154 under the loosest reading the
gate admits, which is the identical verdict W1 reached on its own ten rungs.**

### 1. Which genes can act, MEASURED per cell rather than read off the mission text (E9)

[MESS, 154 cells × 16 lever points = **2,464 runs**, `--elev const`, each campaign's own clock,
`tools/fb_campaign_arena.py --channels`] "Channel" counts the cells on which the gene moved the
published column §2.1 names for it; "class" counts the cells on which it moved the outcome class.

| Gene | its published channel | channel moves | class moves | why the number is what it is |
|---|---|---:|---:|---|
| **G2** `pilot_cover_frac` | `flt_defer_s`, `flt_cover_s` | **0** | **0** | `flt_defer_s` is 0.0 in **all 2,464 runs on all 154 cells**. E-6's condition — a binding longer than the spread between two members' firing solutions — is met by no campaign geometry: the cooperative element that has a live net (the F-16) carries the AIM-120, whose binding is 0.3 s |
| **G3** `sort` + `dl` | `flt_src`, `flt_assign`, `flt_switch`, `SORT_ASSIGN` | **75** | **33** | the only gene family that moves the breadth, and the half that moves it is the BRIEFED CONTRACT on the **MiG-29** — an aircraft with no datalink, where E2's *"a contract beside a live net is dead text"* does not apply |
| **G4** `pilot_energy_frac` | `bfm_ctrl_s`, `bfm_es` | **12** | **9** | it exists only inside `Phase::Bfm`, and the campaigns declare `set task bfm` in exactly **six** files (`o4-04…09`, `w1-01`) — twelve cells with both seats. The reach is the whole population of BFM cells |
| **G6** `pilot_attack_bias_s` | `ATTACK_RELEASE biasS`, `stores DELIVERY aimErrM` | **34** | **26** | F-16 only. `FBMig29Pilot` overrides the attack pass with its own `ATTACK_CONSENT` path and never reads `AttackBiasS`, so the gene is structurally absent on every O3 striker — E-6's *"the gradient is airframe-shaped"*, a second time and for a different reason |
| **G7** `pilot_attack_ccip_m` | `deliveries`, `aimErrM` | **0** | **0** | `FBPilot.cpp:1368` reads it only when `AtkMode_ == Ccip`, and **not one of the 54 attack missions in the ten campaigns flies CCIP** — they are `ccrp` (42 files), `opt` (12, the MiG's optical director) and `arm` (9). Structurally inert, with the source line and the census |

**The two blocked genes are unchanged and the runner still prints their blockers**: G1 (`formation.md`
F5) and G5 (`duels.md` D3) are not keys at all. The runner's alphabet line now reads *"evolving 5 genes
of 20 pilot keys; 16 keys not in the genome; 0 non-pilot keys reachable"* — and the consequence is
stated rather than hidden: adding G6/G7 moves `fb_evolve.seed_population`'s spread, so **`E2`'s
evolution run is not byte-reproducible against today's genome.** Its measurements stand with their date,
which is the rule this file already applies to an archive whose arena moved.

### 2. The gate over the campaign breadth — REFUSED, and the number is robust across three readings

The cell list is generated by a stated rule and not curated (`tools/arena-campaign.txt`): every
`(mission, team, module)` group of the 100 committed missions that flies a FlightBox module and
declares an objective. **S1 is taken in the FIXED YARDSTICK** (`variants-bvr.txt`, six frozen
doctrines, 924 further runs) — §4.2's own construction, which W1 could not use and had to substitute
the lever population for.

| reading | S2 lever set | S2 threshold | cells passing S2 | of those, S1 ok | informative | verdict |
|---|---|---|---:|---:|---:|---|
| this round's contract (E10) | `levers-campaign.txt`, 15 points | ≥ 3 **and** ≥ 3/9 of 15 = 5 | 0 | — | **0** | REFUSED |
| `E2`'s own published alphabet | `levers-genome.txt`, 9 points | ≥ 3 of 9 | 1 (`o4-06-merge:f16`) | 0 | **0** | REFUSED |
| the loosest the gate admits | `levers-campaign.txt`, 15 points | ≥ 3, ratio waived | 4 | 2 | **2** | REFUSED (S5 2 < 3) |

`S4 154 cells ≥ 6 ok · S5 0 informative ≥ 3 NO · S6 0 identical pair(s) ok · S3 n/a (D2)`

**The distribution of movers, which is the honest form of "the genome and the arena do not intersect":**

| movers of 15 | 0 | 1 | 2 | 3 | 4 |
|---|---:|---:|---:|---:|---:|
| cells | **89** | 46 | 15 | 3 | 1 |

The four cells above two are `o4-06-merge:f16` (3 — all three energy alleles), `w3-09-saturation:f16`
(3), `w4-10-allied-force:f16` (3) and `w3-10-package-q:f16` (4). Twelve cells pass S1; the two that
pass both under the loosest reading are `w3-09-saturation:f16` and `w3-10-package-q:f16` — **both
capstone packages, both above two aircraft a side, and both moved by the GROUND gene and the channel
bit rather than by anything the air half does.** That is W1's sentence measured on nine more
campaigns: *the air half of this tree cannot be graded above two aircraft a side; the ground half can.*

### 3. Why the ground half has no gradient either, and it is the fitness rather than the arena

[MESS, the same 154 baselines] level **C is `GATE` on 74 of 154 cells**, and on **32 of the 46 cells
that actually aim a bomb**. Every craft item of §1.3 is an air-to-air quantity — shot geometry,
uplink support, shot lead, defence, energy, rounds — so a strike cell's key is `(V, M, GATE)` and two
doctrines that both survive and both miss are EXACTLY tied. The graded channel is the objective count
alone, which is binary per aim point.

The consequence is a search property and it was measured before the gate's verdict was in, on a
two-cell probe (`w2-01-dome` + `w1-01-merge`, 94 runs, 8 generations): a `fb_evolve`-style ±step poll
that halves on stagnation **never moved its champion off `pilot_attack_bias_s = −7.2`**, because every
value outside a ±0.08 s window produces the identical class and the step shrinks around wherever the
first champion sat. Replacing the operator with a coordinate-wise GRID poll over the current bracket,
halving the bracket per generation (`fb_campaign_evolve.grid_poll`, booked as **D8**), crossed the
plateau at generation 6 and found **−0.1562 s**. Both probes are reported as what they are — probes of
the SEARCH OPERATOR on an arena that had not passed — and neither publishes anything.

### 4. What the search found anyway: one instrument exploit and two delivery defects

All three are in §Gaps → "Exploits the evolution found" with their channels. The headline of the three,
because it is the one the fitness itself is exposed to:

**A healthy aircraft — on EITHER side — departing controlled flight deletes level M for everybody.**
`FBMissionRunner`'s loop ends at `FirstFlightKo`, and `ExpectedLoss` forgives a K.O. only when the unit
was already combat-ineffective, so a stall/mush of a MiG-29 stops the run before any monitor
`Conclude`s: **zero `mission OBJECTIVE` lines are published** and every unit reads `NONE`. [MESS,
`w4-10-allied-force`] the baseline ends at t = 695.3 of 700 s with `kamig4 LOC "stall/mush"` and the
eight Blue F-16s score `V = 16, M = 0`; three unrelated levers (`net-off`, `bias-early`, **and
`bias-rail`, which throws the bombs 2,794 m wide**) all keep the MiG flying, the run reaches its own
timeout, and the identical eight jets score `V = 18, M = 8`. **17 of 154 cells have a lever that
crosses that boundary; 5 baselines sit on the wrong side of it.**

### 5. The cost, and what it did not touch

| | |
|---|---|
| runs | **2,464** lever + **924** yardstick + 64 bias sweep + 26 exploit-detector + 94 + 350 probe = **3,922** |
| wall clock | 37 min (lever pass, `--jobs 7 --threads 2`) + 16 min (yardstick) on 6 cores |
| determinism | `--threads 1/2/4` at both measurement points: `w1-07-emcon` baseline/`net-off` and `w2-01-dome` at `bias −0.2` — **identical telemetry MD5 and identical events at all three**, 6 + 3 runs |
| `git status --porcelain sim/assets sim/missions` | **empty before and after every run**; `verify-models` green (1 declared delta), `verify-layers` green (*"304 files, 841 internal include(s), 12 layers — no upward include, 3 restricted header(s) respected, 6 registry reader(s) inside the perception boundary, 1 antenna-cue poster(s), 291 file(s) in their layer's namespace (5 C-island file(s) exempt)"*) |
| `sim/src/` | **not touched**. The round is three tools, two data files and this section |
| first tool crash, and the fix | the first full pass was killed at cell 140 of 154 after 65 min and printed nothing. The gate now appends every finished run to its channels CSV and RESUMES from it |

### Deviations from the spec, found while building it

| # | The spec says | What was built, and why |
|---|---|---|
| **D8** (`E5`) | §2's genome is searched by `fb_evolve.mutate`'s ±quarter-band step | The campaign runner polls a coordinate-wise GRID over each gene's current bracket and halves the bracket per generation. `mutate` is unchanged and its default `scale=1.0` is the old step exactly. The reason is measured (§3): with a gene whose declared band is 200× its useful scale, a local ±step poll on a plateau shrinks around its own starting point — 94 runs, 8 generations, champion unmoved — while a bracket that always spans the band reaches 0.078 s of a 20 s band in six |
| **D9** (`E5`) | §4.2 S1 is measured *"against a fixed field (the yardstick of §3.6a)"* | Built exactly so on a campaign cell, which W1 could not do — and the consequence is stated rather than worked around: the frozen field is six BVR intercept doctrines, so it is INERT on every strike cell, and a strike cell can therefore never pass S1. Writing a ground-flavoured yardstick would have made 46 cells informative on paper; it was not done, for the reason `E2` declined the merge yardstick |

---

## State — round `E6` (2026-07-30): the craft level orders on the ground, and X-1 is closed

`E5` closed with two measured defects and both are repaired here. Nothing in level V, level M, the
genome, the archive or the gate's numbers changed: `fb_arena_check.py` is byte-identical, the four gate
constants are untouched, and the two files that moved are `sim/tools/fb_fitness.py` (the craft level)
and **one block** of `sim/src/missions/FBMissionRunner.cpp` (the judge finishing).

### 1. The ground gradient exists, and it did not have to be weighed against the air

[MESS, 154 baselines, `--elev const`, each campaign's own clock, ONE run per cell read by BOTH fitness
modules — the old one out of `git show HEAD`, so the comparison is the reader and never the run]

| | old fitness | new fitness |
|---|---|---|
| cells whose `C` is `GATE` | **74 of 154** | **42 of 154** |
| of the 46 cells that deliver a store | **32 of 46** | **0 of 46** |
| cells whose `(V, M)` differs between the two readers | — | **0 of 154** |

The last row is the conservation statement that matters: the craft level grew and the two levels that
DECIDE did not move on a single cell of the campaign breadth.

**The order, on the cell `E5` used to state the defect** [MESS, `w2-01-dome:f16`, `pilot_attack_bias_s`
at six points, one run each, and **both readers on all six of the SAME kept run trees** — the old key is
read, not derived. `eng_shot_s` and `eng_lock_s` are −1.0 in all six, which is why the old gate closed]:

| lever | `aimErrM` | old key | new key |
|---|---:|---|---|
| `bias -0.1` | 13.29 m | V=3 M=2 **GATE** | V=3 M=2 `(+0.0, +42.9)` |
| baseline (`bias 0`) | 36.38 m | V=2 M=1 **GATE** | V=2 M=1 `(+0.0, +21.6)` |
| `bias -0.05` | 36.38 m | V=2 M=1 **GATE** | V=2 M=1 `(+0.0, +21.6)` |
| `bias +0.1` | 59.51 m | V=2 M=1 **GATE** | V=2 M=1 `(+0.0, +14.4)` |
| `bias +0.2` | 82.64 m | V=2 M=1 **GATE** | V=2 M=1 `(+0.0, +10.8)` |
| `bias 10` (the far rail) | 61 294 m | V=2 M=1 **GATE** | V=2 M=1 `(+0.0, +0.0)` |

Four rows share `(V, M) = (2, 1)` and were **exactly tied** — `GATE` against `GATE`, and the air
component is +0.0 on this cell because the striker never fires and never locks. They are now strictly
ordered, by the aim currency alone, in the order of their aim error. The two runs at 36.38 m stay tied,
which is correct: they are the same delivery.

### 2. X-1 is closed, measured on the cell it was found on

[MESS, `w4-10-allied-force:f16` (the eight Blue F-16s), baseline + the three levers `E5` named, `--elev
const`, both binaries]

| lever | before (the `E5` measurement) | after |
|---|---|---|
| baseline — `kamig4` `LOC "stall/mush"` at t = 695.3 of 700 | **V = 16, M = 0** | **V = 18, M = 8** |
| `net-off` — the MiG lives to the timeout | V = 18, M = 8 | V = 18, M = 8 |
| `bias-early` | V = 18, M = 8 | V = 18, M = 8 |
| `bias-rail` — throws its bombs **2 794 m** wide | V = 18, M = 8 | V = 18, M = 8 |
| movers of 3 on this cell | **3** | **0** |

The doctrine is no longer paid for the opponent's airmanship: the four keys are identical at both
deciding levels, and the run still ends exactly where it ended (`RESULT … result=LOC` on `kamig4`,
t = 695.3, exit 2 — unchanged). What DOES separate the four now is the craft level, and it separates
them in the right direction: `bias-rail`'s aim currency is **+1.4** against the baseline's **+15.9** and
`bias-early`'s **+17.8**. `bias-rail` also carries a HIGHER air component (+333.6 against +312.5), so it
is incomparable to the baseline rather than better than it — which is domination doing exactly the job
it was chosen for.

### 3. Conservation — the three published tournaments do not move, and it is measured, not assumed

Every one re-flown twice: **old binary + old fitness** against **new binary + new fitness**, same
geometry, same variant file, same seed field.

| tournament | published | old instrument | new instrument |
|---|---|---|---|
| `variants-flight`, `mirror --flight 2` | `f16_solo` 1.000 > `f16_net` 0.625 | 1.000 / 0.625 / 0.625 / 0.250 / 0.000 | **identical**, craft `+448.2/+0.0` … `+208.8/+0.0` |
| `variants-flight`, `split --flight 2` | three F-16 rows tie at 0.750 over `mig_pair` 0.250 / `mig_solo` 0.000 | 0.750 ×3 / 0.250 / 0.000, `GATE x4` on both MiG rows | **identical** |
| `variants-mixed`, `mirror --timeout 420` | `mig_long` 1.000 > `f16_long` 0.800 > `f16_base` 0.600 > `mig_base` 0.400 > `f16_deep` 0.200 > `mig_deep` 0.000 | as published, `f16_base`/`f16_long` at V = 2.20, M = 1.20 | **identical** |

V, M, the craft means, the kill/lost/draw records and the `decided at level` line are the same in all
three. The reason is structural rather than lucky: **no store is delivered in any of the 70 runs**, so
the aim component is +0.0 on both sides of every comparison and domination on `(air, 0)` IS the scalar
comparison the old fitness made. `f16_base`/`f16_long` keep V = 2.20 / M = 1.20 against the published
2.40/1.40 — that difference is `E3`/`E4`'s pilot and FLCS work and is present in the OLD instrument too,
which is why the A/B was flown with both.

### 4. What moved in the tree, split by what kind of movement it is

[MESS, all **251** `sim/missions/*.fbm`, `tools/fb_regress.sh`-shaped snapshot, old binary against new]

| | |
|---|---|
| telemetry values moved | **0** — no flight column moves, in any file, at any thread count |
| exit codes moved | **0** (`exit.txt` byte-identical) |
| `events.log` unchanged | **224 of 251** |
| lines added | **80** `mission OBJECTIVE` + **68** `mission RESULT` + **58** `mission UNIT_RESULT` |
| determinism | `--threads 1/2/4`, full snapshot each: **identical**, including exit codes |

The 27 files that moved, by class — the distinction is the point, because only the third class is a
line that says something DIFFERENT rather than something more:

| class | count | files |
|---|---:|---|
| **A — the log gains lines only** (single-actor runs print no `UNIT_RESULT` block, so only the judge's own self-log is new) | 6 | `tank-jettison`, `tank-radius-clean`, `tank-radius-tanks`, `test-cfit-mountain`, `test-gear-up-crash`, `w2-04-loaded` |
| **B — a `UNIT_RESULT` goes from `NONE` "still under way when the run ended" to the judge's own verdict**, plus that unit's objective vector | 18 | `air-eagle-amraam`, `air-eagle-sparrow`, `air-fishbed-guns`, `damage-amraam`, `duel-asym-probe`, `escort-protect-lost`, `gun-bfm`, `gun-turning`, `intercept-lostlock`, `mig29-intercept`, `net-jam-wire`, `o5-06-belt`, `o5-07-jammed`, `objective-covers-none`, `qra-weapons-hold`, `w2-06-escort`, `w2-10-opera`, `w4-10-allied-force` |
| **C — a `UNIT_RESULT` goes from the PHYSICAL judge to the MISSION judge**, because `ShotDownFirst`'s precondition (`Concluded()`) is now met where it never could be: all three units took a `damage KILL … combat ineffective` **before** their `monitor KO`, which is the constellation [`missions/runtime.md`](missions/runtime.md) already gives to the mission judge | 3 | `net-belt-high` (t = 70.1 kill → t = 304.9 ground), `o1-08-belt-netted` (608.2 → 723.9), `o3-10-october-six` (379.9 → 396.5) |

**Class C is a report line and never a verdict**: all three runs keep their `RESULT` line, their
`decisive=` attribution and their exit code. A HEALTHY airframe that departs still reads `LOC`/`CRASH`
(`CombatEffective()` is true, so the rule does not fire) — [MESS] `w4-10`'s `kamig4` before and after.

### 5. The ten campaigns, both criteria, and the fingerprints follow the missions exactly

Every campaign re-verified with `tools/fb_campaign_verify.py` under `--elev const` and its own recorded
clock: criterion 1 = 3 reps × `--threads 1/2/4`, criterion 2 = every step re-run standalone from step
*k−1*'s state file.

| campaign | 9 runs → | steps that moved | replays |
|---|---|---|---|
| `o1-bekaa-1982` | 1 fp, **moved** `629488ce…` | 8 (`o1-08-belt-netted`) | 10/10 MATCH |
| `o2-pvo-intercept` | 1 fp, **held** `f2fbb47e…` | — | 10/10 |
| `o3-yom-kippur-1973` | 1 fp, **moved** `e8e5dff2…` | 10 (`o3-10-october-six`) | 10/10 |
| `o4-gaf-mig29g-dact` | 1 fp, **held** `9c994069…` | — | 10/10 |
| `o5-airfield-defence` | 1 fp, **moved** `8937584b…` | 6, 7 (`o5-06-belt`, `o5-07-jammed`) | 10/10 |
| `w1-red-flag` | 1 fp, **held** `0e32e6a8…` | — | 10/10 |
| `w2-osirak` | 1 fp, **moved** `d951102c…` | 4, 6, 10 | 10/10 |
| `w3-desert-storm` | 1 fp, **held** `bfe4938e…` | — | 10/10 |
| `w4-allied-force` | 1 fp, **moved** `a403d3b7…` | 10 (`w4-10-allied-force`) | 10/10 |
| `w5-baltic-qra` | 1 fp, **held** `786b9794…` | — | 10/10 |
| `viper-attrition` (the `C0` proof) | 1 fp `fdf1da2b…`, **identical on the PRE-round binary** | — | 4/4 |

**99 campaign runs, 11 campaign fingerprints, 0 divergences; 104 standalone replays, 0 divergences.**
Five campaigns held byte for byte and five moved — and **the eight step fingerprints that moved are
exactly the eight campaign missions in §4's list of 27**, no more and no less. Every step exit and every
campaign exit is unchanged.

### 6. The gates

| Gate | Result |
|---|---|
| `make -C sim core-lib gym native wasm` | all four, warning-free |
| `verify-layers` | *"304 files, 841 internal include(s), 12 layers — no upward include, 3 restricted header(s) respected, 6 registry reader(s) inside the perception boundary, 1 antenna-cue poster(s), 291 file(s) in their layer's namespace (5 C-island file(s) exempt)"* |
| `verify-models` | *"4 upstream-backed model path(s) match assets/MODEL-DELTAS.md (1 declared delta(s), 35 FlightBox-own)"* |
| seven harnesses | rc = 0 each |
| `git status --porcelain sim/assets sim/missions` | empty before and after every measurement above |

### 7. What this round did NOT do

- **The 154-cell gate was not re-run in full under the repaired judge.** It is the one measurement this
  round owes and it is booked in E-17 rather than argued away: the X-1 fix moves the outcome CLASS of
  every cell whose baseline or lever crossed the knockout boundary ([MESS, `E5`] 17 cells had such a
  lever, 5 baselines sat on the wrong side), so mover counts can fall (a spurious mover disappears — 3
  → 0 on `w4-10:f16`) and in principle rise (a lever that still ends in a knockout now differs from a
  baseline that no longer does). A partial pass was flown and stopped at **29 of 154 complete cells**;
  their mover distribution is **24 × 0, 4 × 1, 1 × 2 of 15**, i.e. not one cell reaches even `kMoversMin`
  3, let alone E10's ratio of 5. That is consistent with `E5`'s 89/46/15/3/1 and it is NOT a verdict.
- The gate was not loosened: `fb_arena_check.py` is byte-identical, `kModalMax`, `kMoversMin`,
  `kGeometriesMin` and `kInformativeMin` are untouched.
- No genome key was added, removed or re-banded; `fb-gym --pilot-keys` prints the same alphabet.
- The model was not touched: `sim/assets` byte-identical, `verify-models` green.
- **No doctrine shift is published**, because no evolution run was flown. The product is the two repairs
  and their measurements — §6's rule applies to this round as it did to `E5`.

### Deviations from the spec, found while building it

| # | The spec says | What was built, and why |
|---|---|---|
| **D10** (`E6`) | §1.3/§Knowledge 1: `order_scalar` is *"ORDER-ISOMORPHIC to the tuple"* | It is order-isomorphic **at the two levels that decide** and a PROJECTION at the craft level: `v·1e6 + m·1e3 + air + aim`. A pair compared by domination has no isomorphic scalar image — that is what refusing an exchange rate means — so the one instrument that needs a number (the attribution bands, [`modules/air/module.md`](modules/air/module.md) §Spec 11) gets the sum, and the sum is stated as the projection it is. Its cells are all air-to-air, where `aim` is 0 in every run and the number is exactly the one it always was. The ranking never uses it |

---

### 10. Round `E9` — a cell is informative only if it is not a coin

**Added 2026-07-31, after `E8` MEASURED X-5 and before the criterion was built.** §§0–9 are untouched.
One contract, and it exists because the gate certified two cells whose outcome class moves with the
initial condition.

| # | Contract | Acceptance / measurement anchor |
|---|---|---|
| **E18** | **S7 — the outcome class of a cell's BASELINE must survive the same 0.8 m grid X4.1 uses on a champion.** A cell that flips is not informative, whatever S1 and S2 say about it: they measure sensitivity to DOCTRINE and cannot tell it from sensitivity to ANYTHING. The screen runs on the cells that already passed S1 ∧ S2, so it costs 8 runs per candidate and never more | the gate prints S7 per candidate as `flips of 8`, and `informative = S1 ∧ S2 ∧ S3 ∧ S7`. **Applied to `E8`'s three certified cells it must leave one**, i.e. it must REFUSE the arena that `E8` passed — the criterion is written knowing that, and it is written anyway |

**Why the threshold is ZERO flips and not §5's two.** §5's *"2 of 8"* is a floor for reading a
CHAMPION's advantage on a geometry that is already in the arena. S7 decides ADMISSION, and a cell that
is admitted is one every later claim rests on, so it carries the stricter test — the same asymmetry the
tree already applies between a lever (may try a bad idea) and a fixed field (may not).

**What this may NOT become.** S7 may not be run on the champion's genome, only on the cell's own
baseline. A screen that asked "does MY genome survive here" would select the arena on the result, which
is what §7 forbids and what `E2` declined three times.

---

### 11. Round `E13` — the runner selects by a measure §6 forbids publishing

**Added 2026-08-01, after `E12` measured it and before the line was changed.** §§0–10 are untouched. One
contract, and it removes an inconsistency that has been in this file since `E2`.

`fb_campaign_evolve.py` picks its champion from `round_robin(pop, pop + archive)` — the CO-EVOLVING
population. §6's list of what is expressly not a finding names, second: *"a fitness rise measured only
against the co-evolving population"*. A runner that SELECTS by that measure and then offers its champion
as the round's result publishes exactly what §6 forbids, one step removed.

[MESS, `E12`] the inconsistency is not theoretical: `g5_23` wins its round robin at **0.652**, the highest
of its generation, and scores **0.444** against the frozen field, the lowest of all three champions. The
runner followed the round robin and the fixed yardstick fell.

| # | Contract | Acceptance / measurement anchor |
|---|---|---|
| **E19** | **The champion is the genome the FIXED FIELD ranks highest.** The round robin stays where it is needed — §3.4 B's non-domination test builds the archive out of it, and that is a statement about the population, not about the round's result. But the CHAMPION, the one object a round would publish, is selected by the frozen text no genome can influence | **checked BEFORE building, and it buys nothing:** the yardstick-best champion of `E12` scores 0.611 on generations 0–4 and the change therefore cannot manufacture a rise where there was none. A repair that would have produced a result is a repair to distrust; this one is proposed because it does not |

**What this is not.** It is not a loosening and not a new criterion — §3.6a already declares the fixed
field the valid measure and §6 already forbids the other one. The runner simply did not follow its own
file. And the selection cannot become a back door: the field is committed text (E16), extended only by
rule and only before the run that reads it.

---

### 12. Round `E20` — a gene is graded only where it is REACHABLE, and reachability is measured

**Added 2026-08-04, after `E19` measured E-29 and before anything was repaired.** §§0–11 are untouched.
Two contracts. The first is what `E19` left owed; the second is the rule that stops the same error being
made again in a different gene.

| # | Contract | Acceptance / measurement anchor |
|---|---|---|
| **E20** | **A gene's silence on a module is a MEASUREMENT with two possible readings, and the round that finds it must say WHICH.** Either the module cannot express the gene for a reason in ITS OWN sources — then the asymmetry is correct, is booked with the source, and the arena may not grade that side on it — or the seam is missing, and then it is built from that module's own numbers. A third reading, "the gene is inert", is forbidden: it is what X-15 lets a runner believe | the reading is taken at the BIT level (SHA-256 over the side's whole telemetry), never at the outcome class — a class-level zero cannot tell inert from invisible-to-a-sum (X-13). Every gene of §2/§7 carries one of the two verdicts with its own measurement |
| **E21** | **A repair to a module may not move the other module, and the proof is byte-identity rather than an argument.** The seam that lets a second airframe express a gene is a no-op on the first by construction (a virtual whose DEFAULT is the code it replaced), and that is checked and not claimed | `tools/fb_regress.sh` over all missions: every mission WITHOUT the repaired module is byte-identical. And the gene's own rail reproduces the pre-repair binary on the repaired module too — the capability is added, nothing is removed |

**Why the second contract is here.** `E19` measured that eight of nine genes were bit-still on the
MiG-29 and read it as one fact. It is not one fact: `E20` measures **three different causes** behind it —
a missing seam (G5), a sourced airframe asymmetry (G1, G2, G6, G7) and a RIG that never enters the phase
the gene lives in (G4, which the MiG expresses perfectly well where a rig lets it). A round that repairs
"the genome" without separating the three would build a seam for a gene that never needed one.

---

## State — round `E7` (2026-07-31): the debt is paid, and S1 was measuring the wrong genome

Two things were owed and both are delivered: the 154-cell gate re-run in FULL after the X-1 fix, and
S1's fixed field made commensurate with the genome (§9, E15–E17). **Nothing in `sim/src/` was
touched.** 4,158 runs: 2,464 lever (154 cells × 16), 924 against the historical six-member field as a
CONTROL, 770 for the five members that make it commensurate. `--elev const`, each campaign's own clock,
`git status --porcelain sim/missions sim/assets` empty before and after.

**The one-sentence result: the arena is REFUSED again, but for the first time the two criteria AGREE —
S1's thirteen passes under the old field were passes on doctrine the evolution cannot express, and a
field that CAN express it passes nowhere, on all 154 cells, arithmetically.**

### 1. The debt, paid — and it refutes the prediction `E6` committed with it

[MESS, 154 complete cells × 15 levers + baseline]

| movers of 15 | 0 | 1 | 2 | 3 | 4 |
|---|---:|---:|---:|---:|---:|
| `E5`, before the X-1 fix | 89 | 46 | 15 | 3 | 1 |
| `E7`, after it | 89 | 46 | **16** | **2** | 1 |

`E6`'s commit message predicted the fix "moves the class of every cell whose baseline or lever crossed
the knockout boundary, in both directions", on the 17 cells `E5` had counted. **Measured, it moves the
mover distribution by two cells.** The reason is that a mover count is a DIFFERENCE: X-1 shifted the
baseline and its levers across the boundary TOGETHER on nearly every affected cell, so the class moved
and the difference did not. `w4-10-allied-force:f16` — the cell `E6` measured the exploit on — leaves
the group entirely, 3 movers → 0, exactly as measured there. The prediction was wrong in its magnitude,
and the measurement is the correction.

**Which lever family moves an outcome class, over the whole breadth** (`tools/fb_arena_movers.py`, which
reads published channels and flies nothing, so it cannot select anything):

| family | cells it moves | the levers in it |
|---|---:|---|
| **G6** `pilot_attack_bias_s` | 28 | `bias-rail` 26, `bias-late` 11, `bias-early` 6 |
| **G3** `sort` + `dl` | 30 | `sort-near` 17, `net-off` 12, `sort-left` 4 |
| **G4** `pilot_energy_frac` | 9 | `energy-low` 6, `energy-mid` 3, `energy-high` 3 |
| **G2** `pilot_cover_frac` | **0** | — |
| **G7** `pilot_attack_ccip_m` | **0** | — |

### 2. S1 under a commensurate field — 13 → 0, and the mechanism is arithmetic, not a coincidence

| field | cells with ≥ 2 outcome classes | **S1 passes** |
|---|---:|---:|
| `variants-bvr.txt`, 6 members, incommensurate (the control) | 52 | **13** |
| `variants-arena.txt`, 11 members, commensurate | **61** | **0** |

The commensurate field splits MORE cells and passes S1 on none. That is not a paradox and not a
regression — it is the repair working, and the thirteen are traceable one by one:

| the 13 that passed | distinct classes, before → after | modal share, before → after |
|---|---|---|
| all thirteen | **unchanged** (2→2, 3→3, 4→4) | 3/6 = 50.0 % → **8/11 = 72.7 %**, or 2/6 = 33.3 % → **7/11 = 63.6 %** |

**Not one of the five new members produced a new outcome class on any of the thirteen.** All five landed
in the modal class every time, and the new share is exactly `(old modal + 5) / 11`. **A member that is
inert on a cell is a vote for the status quo**, and five of them raise the modal share by construction.

The bound is general rather than a property of these thirteen [MESS, all 154 cells, the six BVR members'
own classes]: the class the BASELINE sits in holds **≥ 2 of the 6 members on every one of the 154 cells**
(2 members on 5 cells, 3 on 9, 4 on 17, 5 on 21, all 6 on 102). With an inert genome the modal class is
therefore at least `(2 + 5)/11 = 63.6 %`, above S1's 60 % everywhere. **On 0 of 154 cells could S1 be
passed by a genome that cannot act**, and on only **9 of 154** does even one of the five members move a
class at all.

So S1 now asks exactly the question it was meant to ask — *can the doctrine under evolution move this
cell?* — and its answer over the ten campaigns is no. The thirteen passes of the six-member field were
**false positives, every one**: those cells were spread by `pilot_shot_rtr`, `pilot_lock_nm` and
`pilot_react_s`, none of which the genome contains. `E4`'s unexplained observation — *"S1 and S2 pass on
different cells"* — is dissolved rather than mitigated: they passed on different cells because they were
asked in different alphabets.

### 3. What no field can fix, and it is the binding constraint

**S2 does not read the field at all** (`fb_campaign_arena.py` counts movers over `levers`, never over
`yard`), so the extension could not have changed the verdict whatever it contained. Under this round's
contract (E10: ≥ 3 movers AND ≥ 3/9 of the 15 swept = 5) **0 of 154 cells pass S2**; the best cell in the
entire campaign breadth is `w3-10-package-q:f16` with **4**.

And the denominator is worse than it looks: **5 of the 15 levers are structurally dead on all 154
cells** — G2's three and G7's two, each measured at 0 movers above. Even the most generous honest
accounting — strike the dead levers and recompute the ratio over the 10 that can act (3/9 × 10 → ≥ 4) —
leaves **exactly one** cell passing S2, against S5's 3. The campaign breadth cannot grade this genome,
and that statement is now robust across four readings rather than three.

### 4. The two arenas that are missing, named with their census

Both are E-17(c), and this round makes one of them cheap enough to date:

| gene | what it needs | what the tree has |
|---|---|---|
| **G7** `pilot_attack_ccip_m` | one rung that delivers in **CCIP** | the mode and a rig exist (`missions/attack-ccip.fbm`); of the 100 campaign missions **not one** declares it — 102 `ccrp`, 32 `opt`, 20 `arm`. **No C++ is needed** |
| **G2** `pilot_cover_frac` | an element whose two members' firing solutions are spread wider than the weapon's own binding | the netted element carries the AIM-120 (0.3 s binding); `flt_defer_s` is 0.0 in all 2,464 runs |

The CCIP gap is a **realism** gap of the campaigns before it is a gate gap — CCIP is the F-16's standard
visual delivery and 54 attack missions fly without it — which is what makes building it legitimate under
§7's rule that a round may not build the arena around a gene.

### 5. Both arenas are refused, and the cause is the GENOME rather than either arena

The campaign breadth is not the only arena, so the round asked the other one too [MESS,
`fb_arena_check.py --flight 1 --levers levers-genome.txt`, the generated geometries]:

| arena | informative | against |
|---|---:|---|
| the ten campaigns, 154 cells | **0** | S5's 3 |
| the generated geometries, 12 | **1** (`merge`, and its single mover is `energy-low`) | S5's 3, and against the **4** E-12 recorded |

The generated arena lost three informative geometries since E-12 was written, and the tree already owns
the reason: `E-15` closed the FLCS-damper defect and `E-18`/X-1 closed the judge's, and *"a geometry
whose informativeness comes from one side dying of a bug is a measurement of the bug"*. Repairing this
simulator removed the signal the gate was reading.

With both arenas refused the diagnosis cannot be a property of either. It is the genome, and the census
is exact — the five growths the owner goal names, measured rather than assumed:

| the goal's name | gene | state, measured |
|---|---|---|
| **Verband** | G1 `pilot_flight_shape` | **not a key at all.** `set pilot_flight_shape 1` → `module SET_INVALID_VALUE` + `mission SET_REJECTED` at t = 0.0, **exit 1** before the first tick. Blocked by [`formation.md`](formation.md) F5: `FormationSpreadM/TrailM/StackM` are airframe hooks, not mission data |
| **Deckung** | G2 `pilot_cover_frac` | live key, **0 movers on 154 cells**. `flt_defer_s` is 0.0 in all 2,464 runs — the netted element carries the AIM-120 and its binding is 0.3 s |
| **Sortierung** | G3 `sort` + `dl` | **live, 30 cells.** The one gene that moves the breadth |
| **Energieregel** | G4 `pilot_energy_frac` | live, **9 cells** — it exists only inside `Phase::Bfm`, which six campaign files declare |
| **EMCON-Timing** | G5 `pilot_emcon_frac` | **not a key at all**, same rejection and exit. Blocked by [`duels.md`](duels.md) D3: `pilot/FBPilot`'s picture is built from the Radar block alone, so "silent" means "silent and blind" and the band is degenerate at one rail |

**Two of the five are not reachable, one is inert for want of a weapon binding, one lives only in BFM,
and exactly one acts broadly.** That is the complete explanation for four rounds without a publishable
doctrine shift, and it is an engineering backlog rather than a gate problem. Ordered by what it unblocks:

| # | build | unblocks | why it is not "building the arena around a gene" |
|---|---|---|---|
| 1 | **F5** — a flight can be BRIEFED a shape (mission vocabulary → pilot) | G1, the goal's *Verband* | a four-ship that cannot be given anything but combat spread is a formation defect on its own terms; `FormationTrailM` defaults to 0, so a four-ship flies four abreast |
| 2 | **D3** — the pilot's picture reads the IRST block that `sensors/FBIrstSystem` already publishes | G5, the goal's *EMCON-Timing* | the MiG-29's one genuinely passive sensor is today consumed only by a missile seeker. This is a sensor defect, and fixing it makes the pilot see LESS artificially, not more |
| 3 | a rung that delivers in **CCIP** | G7 | §4 above. **Stated with its own limit:** the daylight cells it would convert have 2 movers today and would reach 4, so this alone does NOT open S2's 5 — and the one cell that would (`w3-10-package-q:f16`, 4 movers, two families) flies at 00:00 and may not be converted for that reason |
| 4 | a long-binding round on a netted element | G2 | §4 above |

### 6. The cost, and what this round did not touch

| | |
|---|---|
| runs | **4,158** (2,464 lever + 924 control + 770 commensurate) |
| `sim/src/` | **not touched.** The round is one doc section, one data file, two refusals in a tool, one read-only reporter |
| the gate's constants | `kModalMax` 0.60, `kMoversMin` 3, `kMoverFrac` 3/9, `kGeometriesMin` 6, `kInformativeMin` 3 — **all unchanged, none loosened** |
| the evolution | **not run.** §6 forbids publishing anything measured on a cell that failed S1–S3, and 0 cells passed. `tools/arena-informative.txt` is written BY the gate and contains zero cells, which is the honest form of that sentence |

### Deviations from the spec, found while building it

| # | The spec says | What was built, and why |
|---|---|---|
| **D11** (`E7`) | E17(b): growing the genome moves S1, so S1 numbers are comparable only within an alphabet version | **The booking was too narrow, and the measurement says so.** Field SIZE moves S1 as well, and mechanically: S1's threshold is a SHARE, so adding *k* members that are inert on a cell raises its modal share to `(m + k)/(n + k)`. Extending a field therefore TIGHTENS S1 silently. It does not violate E10 (which forbids loosening) and it is the correct direction — an inert member must not certify a cell — but it was not foreseen, and the constant 0.60 is calibrated against a field of six |

---

## State — round `E8` (2026-07-31): the arena PASSES, the evolution RUNS, and X4 refuses the result

> **RETRACTED IN PART, `E9`, same day, by my own check.** §1's mover counts and therefore §1's
> *"3 informative"* are **CONTAMINATED and are not measurements**: the run resumed its baseline and its
> 15 contract levers from a channels file recorded BEFORE F5 changed `FormationTrailM`, and flew only
> the 6 shape levers under the new binary. So the six were compared against an OLD-simulator baseline,
> and every one of them looks like a mover on exactly the cells the default change moves — the
> four-ships. Caught by a contradiction I could not explain away: S7 read `w3-09-saturation` as flipping
> **8 of 8** while the standalone audit had read the same cell as **0 of 8**, and the cause was the
> comparison base, not the cell (`(11, 5)` cached vs `(10, 4)` re-flown). §§2–5 rest on §1 and are
> retracted with it; what stands independently is F5 itself (its own regression is binary-consistent,
> §[`formation.md`](formation.md) F5) and the X4/X-5 measurements, which never touched the cache. The
> corrected numbers are in §State `E9`. **The structural fix is in the tool, not in a habit:** a channels
> file now records the SHA-256 of the simulator that wrote it and refuses to be resumed under another.

`E-20` said the blocker was the genome and named F5 as the first item. F5 is built
([`formation.md`](formation.md) §Spec `F5`), G1 is a key, and everything downstream of that follows in
one chain — including the refusal at the end, which is the round's sharpest finding.

**The one-sentence result: unblocking ONE gene took the campaign breadth from 0 informative cells to 3
and let the first evolution in this line run to completion — and then X4 disqualified two of those
three cells as CHAOTIC, so no doctrine shift is published and the gate is shown to be blind to the
difference between "sensitive to doctrine" and "sensitive to anything".**

### 1. G1 moves the breadth, and the gate opens

[MESS, 154 cells × 21 levers, `levers-campaign-f5.txt` = the 15 contract levers + 6 shape levers]

| movers of 21 | 0 | 1 | 2 | 3 | 4 | 5 | **7** | **8** |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| cells | 84 | 44 | 15 | 4 | 2 | 1 | **3** | **1** |

S2's threshold at 21 levers is `3/9 × 21 = 7`, and **four cells reach it** — the first S2 passes in this
file's history. G1 alone moves the outcome class on **13 cells**: `shape-flat`/`shape-stacked` 9 each,
`shape-tight`/`shape-wide` 8 each, `shape-abreast`/`shape-trail` 3 each (trail acts only from a
four-ship, `aftM = element × trail` with `element = (position−1)/2`).

With the field extended to 14 members (E16: the six BVR lines still byte-identical, three added for the
three new genes), the gate reads:

```
S4 154 cells >= 6 ok · S5 3 informative >= 3 ok · S6 0 identical pair(s) ok · S3 n/a
ARENA: PASSED   (154 cells, 3 informative)
```

The three are `o3-10-october-six`, `w1-09-lfe-four` and `w3-09-saturation` — **all three the MiG-29
seat**. `w3-09-saturation:f16` passes S2 with 7 movers and fails S1, so this round can evolve one
airframe's doctrine, not both, and that is stated rather than glossed.

### 2. The evolution, run on the gate's own output

`fb_campaign_evolve.py --cells tools/arena-informative.txt --generations 6`, **723 runs**, population
40, 8 live genes (only G5 still blocked). The cell list was WRITTEN by the gate (`--emit-informative`),
never by hand.

| | |
|---|---|
| champion | `g0_s2` — the generation-0 grid point plus `sort=near`, and it never changed |
| fitness against the co-evolving population | 0.667 vs the seed's 0.496 — **expressly not a finding** (§6) |
| the fixed yardstick, per generation | 0.556 · 0.556 · 0.556 · 0.556 · 0.556 · 0.556 — **flat** |
| circling instruments | (b) T = 0.0000, 0 cyclic triples of 0; (c) 0.0000 — the reading `E-16` says is ambiguous, and with a flat yardstick it is a **fixed point** |
| saturation | **no** — at generation 5 level V decided **612** comparisons, C 299, exact ties 4126 |

**Six generations of grid poll over seven numeric genes moved nothing.** The only gene that decided was
the sort allele. Against the yardstick's baseline the champion is better on two cells and worse on one:

| cell | champion | yardstick baseline | |
|---|---|---|---|
| `o3-10-october-six` | V=29 M=6 | V=33 M=6 | worse |
| `w1-09-lfe-four` | V=6 M=2, C +395.6 | V=6 M=2, C +270.4 | better (craft only) |
| `w3-09-saturation` | V=11 M=5 | V=10 M=4 | better on both deciding levels |

### 3. X3 passes — the mechanism is a chain of published channels

[MESS, the arena's own channel CSV, baseline against `sort-near`]

| cell | `flt_src` | `flt_assign` | `SORT_ASSIGN` | `flt_switch` |
|---|---|---|---|---|
| `o3-10-october-six` | **0 → 2** | **0 → 4** | **0 → 20** | 0 → 7 |
| `w1-09-lfe-four` | 2 → 2 | 5 → 4 | 6 → 11 | 2 → 4 |
| `w3-09-saturation` | 2 → 2 | 13 → 16 | 32 → 31 | **12 → 9** |

On `o3-10` the flight has **no assignment source at all** without the briefed contract — the MiG-29 has
no cooperative terminal, so `E2`'s *"a contract beside a live net is dead text"* is inverted: the
contract is the only text there is. On `w3-09` the range contract is also more STABLE than what the rung
briefed, three re-sorts fewer over the engagement.

### 4. X4 fails, and the failure is a property of the CELLS rather than of the champion

| detector | `o3-10-october-six` | `w1-09-lfe-four` | `w3-09-saturation` |
|---|---|---|---|
| **X4.1** spawn longitude ±3 m, 8 samples, with the champion | **3 of 8 flip** | **3 of 8 flip** | 0 of 8 |
| the same, with a near-empty genome | **3 of 8** | **8 of 8** | 0 of 8 |
| **X4.2** timeout × 1.5 | held | held | held |

§5's noise floor is *"the yardstick itself flips in 2 of 8 samples in a chaotic geometry — and if it
does, **no claim may be made on that geometry at all**"*. Two of the three certified cells are over it,
and the second measurement settles what it belongs to: **`w1-09-lfe-four` flips on all eight
perturbations under a genome that sets one unrelated gene.** The chaos is the cell's.

With two geometries disqualified, X1 (*"the advantage survives on ≥ 2 informative geometries"*) has one
left and cannot be satisfied. **§6's rule is binding and is applied: no §1 is published.** The champion
stands as a candidate with its mechanism, not as a doctrine shift.

### 5. What that says about the gate, and it is the round's product

S1 asks whether a cell's outcome is spread over a field of doctrines. S2 asks whether the baseline moves
under the genome's levers. **Neither can tell a lever from a coin.** A cell that flips on 0.8 m of spawn
produces distinct classes and plenty of movers for a reason that has nothing to do with doctrine — and
the measurement shows this is not hypothetical: `w1-09-lfe-four` carried **the most movers of the entire
campaign breadth (8 of 21)** and flips on **8 of 8** perturbations.

So S2's mover count is contaminated by chaos exactly where it is largest, and the gate has no criterion
for it although the detector has existed all along as a post-hoc audit. That is booked as **E-21** with
the number, and the screen it asks for is cheap: 8 runs per cell, the same 0.8 m grid.

### 6. The cost

| | |
|---|---|
| runs | 924 (G1's levers over 154 cells) + 462 (the field's three new members) + **723** (the evolution) + 51 (X4 on two genomes) = **2,160** |
| `sim/src/` | four files, all of them F5's: two lines of enum, three table rows, one hook default, one station computation |
| `git status --porcelain sim/missions sim/assets` | empty before and after every run, and the audit tool checks it too |

---

## State — round `E9` (2026-07-31): the corrected number, and why growing the genome cannot open this gate

`E8`'s headline was contaminated by a stale resume index and is retracted above. This is the same
measurement flown clean: **3,388 lever runs, every one under simulator `4b10f951`**, the channels file
now stamped with that simulator's SHA-256 and refusing to be resumed under another.

**The one-sentence result: G1 does act, the gate still refuses, and the reason is arithmetic — S2's
threshold is a RATIO, so a gene that brings `k` levers raises the bar by `k/3` and only helps if it
supplies more than `k/3` movers on ONE cell.**

### 1. The clean numbers, against the retracted ones

| | `E8`, contaminated | `E9`, clean |
|---|---:|---:|
| best cell, movers of 21 | 8 | **6** |
| cells over S2's threshold of 7 | 4 | **0** |
| informative | 3 | **0** |
| verdict | PASSED | **REFUSED** |

| movers of 21 | 0 | 1 | 2 | 3 | 4 | 5 | 6 |
|---|---:|---:|---:|---:|---:|---:|---:|
| cells | 84 | 46 | 16 | 4 | 2 | 1 | **1** |

G1's real reach, per lever: `shape-wide` 5 cells, `shape-flat` 5, `shape-stacked` 5, `shape-tight` 4,
`shape-abreast` 1, `shape-trail` 1 — against the contaminated run's 9/9/8/8/3/3. The S7 chaos screen
never ran, because it screens S1∧S2 candidates and there were none.

### 2. The law this exposes, and it is exact

| lever set | S2's threshold `3/9 × n` | best cell in the whole breadth | deficit |
|---|---:|---|---:|
| 15, before F5 | 5 | `w3-10-package-q:f16` with 4 | **1** |
| 21, with G1 | 7 | `w3-09-saturation:f16` with 6 | **1** |

**The deficit is unchanged.** G1 supplied three movers on the best cell and the bar rose by two, so the
arena came one mover closer in absolute terms and stood exactly still in the terms the gate reads. This
is E10 working as designed — *"a longer lever file may not buy a pass"* — and the design has a
consequence nobody wrote down: **adding a gene helps only where its own per-cell reach beats `k/3`.**
A gene that moves many cells a little (G1: 13 cells, one or two levers each) cannot open this gate;
only a gene that moves ONE cell in three or more of its own levers can.

That is a design criterion for the next gene rather than a complaint about the gate, and it is
falsifiable: G5's EMCON levers must move ≥ 3 of their own on a single cell, or they will move the bar
and not the verdict.

### 3. What the best cell now is, and it changed seat

`w3-09-saturation:f16` — 6 movers from **three families at once** (`net-off`; `bias-late`, `bias-rail`;
`shape-abreast`, `shape-trail`, `shape-wide`), baseline `(20, 12)`. Under the 15-lever set the best cell
was a different one and carried two families. The breadth's most gradable cell is now an F-16 seat, and
the three MiG-29 cells `E8` certified were an artefact.

### 4. The cost, and what the round leaves standing

| | |
|---|---|
| runs | 3,388 lever (clean) + the 108 inherited rows re-flown, field pass **0** — S2 held nowhere, and the fixed field now flies only where it can still change a verdict (2,156 runs saved, no number changed) |
| `sim/src/` | not touched |
| what stands from `E8` | F5 itself (its regression built both binaries and compared them — no cache), the X4 measurements, and X-5 |
| what is retracted | `E8` §§1–5 |

---

## State — round `E10` (2026-07-31): every repair this tree makes removes doctrine signal from the gate

[`duels.md`](duels.md) D3a is built — `CanPressOn` asks for a PICTURE instead of a transmitter — and the
154-cell gate is re-flown under the new simulator (3,388 runs, fresh index, the stale one refused by the
guard). The arena is REFUSED again, and the way it is refused is this round's finding.

**The one-sentence result: fixing a real defect took the EMCON rung from five movers to ZERO, and that
is the fourth independent time a repair has removed apparent doctrine sensitivity — the gate has been
reading defects.**

### 1. What D3a did to the arena

| movers of 21 | 0 | 1 | 2 | 3 | 4 | 5 | 6 |
|---|---:|---:|---:|---:|---:|---:|---:|
| `E9`, before D3a | 84 | 46 | 16 | 4 | 2 | **1** | 1 |
| `E10`, after | 85 | 44 | 18 | 4 | 2 | **0** | 1 |

The cell that left is `w1-07-emcon:f16` — **5 movers → 0**, baseline `(3, 1)` → `(4, 2)`. All five were
the defect: the levers were flipping whether the jet ABORTED, not how it fought. With the abort gone the
baseline reaches the better outcome by itself and nothing moves it. Best cell in the breadth is unchanged
(`w3-09-saturation:f16`, 6 of 21 against a threshold of 7, deficit **1**).

### 2. The pattern, and it is now four independent instances

| the repair | what it removed from the gate |
|---|---|
| **E-15** — the FLCS rate damper on the hand stick | `xmerge`/`xmergesplit` fell from 2 outcome classes at a 50.0 % modal share to **1 class at 100 %**; they lost the S1 pass with the defect that carried it |
| **X-1** — the judge that never concluded | `w4-10-allied-force:f16` **3 movers → 0**; three unrelated levers had been flipping whether the objective vector was published at all |
| **E-12 → `E9`** — the generated arena, after both of the above | **4 informative → 1** of 12 |
| **D3a** — `CanPressOn` demanding a radiating radar | `w1-07-emcon:f16` **5 movers → 0** |

`E-15` stated the rule for one geometry: *"a geometry whose informativeness comes from one side dying of
a bug is a measurement of the bug."* Four instances later it is not an anecdote about one geometry but a
property of this arena: **the campaign breadth's apparent doctrine sensitivity has been substantially
defect-driven, and every repair reduces it.** That is why four rounds of gate work have made the
refusals firmer rather than softer, and it predicts the next one: further repairs will lower the mover
counts again.

**The consequence, stated without an escape hatch.** This is not an argument for stopping the repairs —
each one made the simulator more correct, and `w1-07-emcon` now SUCCEEDS where it used to fail twice
over. It is an argument that a gate built on "does the outcome class move" measures a mixture of
doctrine and defect, and that the mixture has been mostly defect. What is left after the repairs is the
real signal, and today it is one mover short of S2 on one cell of 154.

### 3. The cost

| | |
|---|---|
| runs | 3,388 lever (fresh index under the new simulator) + 502 regression (251 missions × 2 builds) |
| `sim/src/` | one file, one expression — `pilot/FBPilot.cpp`'s `CanPressOn` |
| behaviour | **4 of 251 missions moved, 0 exit codes**; `w1-07-emcon` loses both its `FAIL`s. Determinism over `--threads 1/2/4` on a moved mission: identical MD5 |
| my own acceptance | **falsified and kept**: D3a's spec demanded a no-op and measured four movers |

---

## State — round `E11` (2026-07-31): the genome is complete, S2 falls for the first time, and S7 holds

G5 is built ([`duels.md`](duels.md) D3c). **No gene of the owner goal's list is blocked any more** — nine
live genes, zero blockers, where two rounds ago two of five were not keys at all. The 154-cell gate is
re-flown with 24 levers under the new simulator (3,850 runs, fresh index).

**The one-sentence result: `E-22`'s criterion predicted correctly, S2 fell for the first time in this
file's history — and S7, written two rounds ago before it knew any result, refuses the cell anyway.**

### 1. The first S2 pass, and it is not marginal

| | |
|---|---|
| cell | `w3-09-saturation:f16` |
| movers | **11 of 24** against S2's threshold of `3/9 × 24 = 8` |
| the families that move it | **four at once** — G3 (`net-off`), G6 (`bias-late`, `bias-rail`), G1 (all six shape levers), G5 (`emcon-tight`, `emcon-wide`) |
| S1 | **ok** — 3 distinct outcome classes at a 53.3 % modal share |
| S7 | **NO** — 1 of 8 |

`E-22` had fixed the acceptance for G5 in advance: *"its levers must move ≥ 3 of their own on ONE cell"*.
Measured before the sweep on 12 probe runs, EMCON moved **2 of 3** on this cell — and the sweep then took
the cell from 6 movers of 21 to 11 of 24. That is the first quantitative prediction in this file that was
stated before the run and came true.

**Why 6 → 11 and not 6 → 8.** G5's default moved the cell's own baseline from `(20, 12)` to `(16, 10)`,
and in that regime the four shape levers that were inert before are not. The gene bought its own two and
unlocked three more, which is a property of the cell rather than a general rule and is reported as one.

### 2. S7 refuses it, and sharpening the MEASUREMENT confirms the refusal

The threshold is `kChaosMaxFlips = 0` and it is not touched: it was `[SET]` in §10 with the argument that
admission carries a stricter test than §5's 2-of-8 floor for reading a champion, and it was written with
the explicit expectation that it must refuse `E8`'s arena. Retuning it now, knowing it would open the
gate, is the move this round refused three times.

What IS legitimate is to sharpen the instrument rather than the criterion — with 8 samples a single flip
has a wide interval. Re-flown on a **0.25 m grid, 24 samples**:

| grid | flips | share |
|---|---|---|
| 0.8 m, 8 samples (S7's own) | 1 | 12.5 % |
| **0.25 m, 24 samples** | **3** | **12.5 %** |

Identical. The cell is genuinely a coin one time in eight, the single flip was not a sampling artefact,
and S7 is right about it.

### 3. Where that leaves the arena, stated as a number rather than a mood

The gate no longer refuses because the genome cannot act — it acts on four families at once on this cell.
It refuses because **the campaign breadth has no rung that is both gradable and robust**. That is a
statement about the missions, and it is now specified: what is needed is ≥ 3 rungs with ≥ 8 movers of 24
AND **0 flips of 24** on their own baseline. Neither number is negotiable and both are measured.

| movers of 24 | 0 | 1 | 2 | 3 | 4 | 5 | 6 | 11 |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| cells | 88 | 38 | 21 | 3 | 1 | 1 | 1 | **1** |

### 4. Gradable and robust are NOT in tension — measured, and it changes the target

The obvious worry after §2 is that the two properties fight each other: that a cell only becomes
sensitive to doctrine by sitting on a knife edge. [MESS, the twelve cells with the most movers, each
flown over S7's own 0.8 m grid] **they do not.**

| cell | movers of 24 | flips of 8 |
|---|---:|---:|
| `w3-09-saturation:f16` | 11 | 1 |
| `o5-08-night-one:f16` | 6 | 2 |
| **`o5-09-night-two:f16`** | **5** | **0** |
| **`w3-10-package-q:f16`** | **4** | **0** |
| `o4-06-merge:f16`, `w1-09-lfe-four:f16`, `w3-06-bingo:f16` | 3 | 0 |
| five more | 2 | 0 |

**Ten of the twelve are clean.** Only the two most gradable cells are chaotic, and even that is not a
law — `o5-09-night-two:f16` carries five movers at zero flips. So the target is not "fix the chaos"; it
is **raise the mover count on cells that are already robust**, and the two best candidates fail for
DIFFERENT and structural reasons:

| cell | what it has | what it is missing, and why |
|---|---|---|
| `o5-09-night-two:f16` (5 movers: `net-off`, 2 × shape, 2 × emcon) | a net, four `datalink on`, **flight positions max 2** | the trail levers cannot act at all: `aftM = element × trail` with `element = (position−1)/2`, and a PAIR has no second element. Two of G1's six levers are structurally unreachable on it |
| `w3-10-package-q:f16` (4 movers: 2 × sort, 2 × bias) | flight positions 4, a net, sixteen `datalink on` | neither shape NOR emcon moves it, and that is not explained by the mission text — it is the next thing to measure rather than the next thing to assume |

**What this may not become.** Editing a committed rung BECAUSE it would raise a mover count is selecting
the arena on the result, which §7 forbids and this file has declined four times. A pair becoming a
four-ship is a doctrinal decision about that campaign, and the gate is the check on it, never the reason
for it.

### 5. What the two robust candidates actually publish — and one of them carries a mechanism

[MESS, `build/arena-g5-channels.csv`, the cells' own columns]

| cell | lever | `flt_mates` | `flt_src` | `flt_assign` | `releases` | `deliveries` | `(V, M)` |
|---|---|---:|---:|---:|---:|---:|---|
| `w3-10-package-q:f16` | baseline | 3 | **0** | **0** | 8 | 7 | (40, 24) |
| | every shape and emcon lever | 3 | **0** | **0** | 8 | 7 | (40, 24) |
| `o5-09-night-two:f16` | baseline | 1 | 1 | 3 | **0** | **0** | (7, 1) |
| | `emcon-tight` | 1 | 1 | 3 | **2** | **2** | **(8, 4)** |

**`w3-10-package-q` is a four-ship whose sort never engages**: three mates and `flt_src = 0`,
`flt_assign = 0` on the baseline and on every shape and emcon lever alike. Whether that is a defect of
the flight logic on a four-ship or a property of a geometry with nothing to divide is the next thing to
MEASURE, and this file will not guess it.

**`o5-09-night-two` carries a chain.** Emission discipline takes the cell from zero stores delivered to
two, and level M from 1 to 4 — the jet that does not radiate is not warned about, survives to its release
point, and pickles. Every link is a published column. **And the cell is robust: 0 flips of 8.** This is
the closest thing to a doctrine shift this file has produced, and §6 still forbids publishing it — the
arena has one such cell and needs three. It is recorded here as a measurement, not as a §1.

### 6. Gradability RISES with the size of the graded side — and my own hypothesis was wrong

`w3-10-package-q:f16` grades sixteen F-16s in four flights, twelve of them `task attack` and four
`task intercept`, so its `flt_src = 0` is the STRIKERS' number and not a broken sort. From that I
proposed that a sixteen-unit sum DILUTES a doctrine effect below the granularity of the outcome class.
**Measured over all 154 cells, that is false, and the truth runs the other way** [MESS, cell size =
units of the graded side in the committed file, against movers of 24]:

| units on the graded side | cells | mean movers | max |
|---:|---:|---:|---:|
| 1 | 41 | 0.44 | 3 |
| 2 | 66 | 0.45 | 2 |
| 4 | 30 | 1.37 | 6 |
| 8 | 6 | **2.00** | **11** |
| 16 | 1 | **4.00** | 4 |

**107 of the 154 cells are a single aircraft or a pair, and they average 0.44 movers.** The arena's mass
sits where doctrine cannot be measured at all: a lone jet has no formation to shape, no mate to sort
against, nobody to be silent behind. Gradability is a property of the SIDE's size, and this file spent
four rounds looking for it in the genome and in the gate.

This is the sharpest form of the target in §3: the three rungs the gate needs are not "any three rungs",
they are three of **four aircraft or more** that are also robust. There are 38 such cells in the breadth
and exactly one reaches eight movers today — and that one is the chaotic one.

### 7. How far the goal actually is, as a distance rather than a direction

[MESS, all 154 cells, movers of 24 against the graded side's size, with S7 flown on the top twelve]

| | |
|---|---|
| cells with ≥ 4 units | **44** |
| of those, ≥ 8 movers (S2's threshold at 24 levers) | **1** — `w3-09-saturation:f16` at 11, and it flips 1 of 8 |
| ≥ 6 movers | **2** — the second is `o5-08-night-one:f16` at 6, which flips **2** of 8 |
| the best ROBUST cell | `o5-09-night-two:f16` — **5 movers, 0 flips**, three short |

**The two most gradable cells in the campaign breadth are both chaotic, and the two robust ones are both
short.** S5 wants three informative; the breadth supplies zero, and the nearest miss needs three more
movers on a cell that already carries the strongest published chain in this file (§5).

The three lever counts this file has now flown say the same thing from the other side:

| levers | S2's threshold | best cell | cells passing S2 |
|---:|---:|---:|---:|
| 15 | 5 | 4 | 0 |
| 21 | 7 | 6 | 0 |
| 24 | 8 | **11** | **1** |

Growing the genome DID eventually produce a pass — the first in this file's history — but at a rate of
roughly one cell per two genes, against a bar that rises with every gene added (`E-22`). **There is no
gene left to add**: the owner goal names five and all five are live. What is left is the arena itself,
and §6 says where: 107 of 154 cells are one aircraft or two.

### 8. The small cells are NOT a defect — the campaigns are ladders, and that is the real limit

§6 said the arena's mass "sits where doctrine cannot be measured at all". That is true of the
measurement and WRONG about the cause, and the correction is one command [MESS, friendly F-16/MiG-29
count per rung]:

| campaign | rungs of 1–2 aircraft | rungs of ≥ 4 | its top rung |
|---|---:|---:|---|
| `w1` Red Flag | 6 | 3 | 6 (`graduation`) |
| `w3` Desert Storm | 2 | 8 | **16** (`package-q`) |
| `o5` airfield defence | 6 | 4 | 6 (`batajnica`) |

**Every campaign is a LADDER**: it starts at one or two aircraft and climbs to its own top. The 107
small cells are the lower rungs, and they are correct for what a ladder is — `w1-01-merge` is one jet
against one because that is the first thing a syllabus teaches. Calling them misplaced was wrong.

The real limit is narrower and it is about the SET rather than any campaign: ten ladders produce ten
tops, and **only two rungs in the whole breadth put eight or more aircraft on the graded side**
(`w3-09-saturation` at 8, `w3-10-package-q` at 16). Gradability rises with side size (§6), S5 wants
three informative cells, and the breadth offers two rungs of the size where doctrine is measurable at
all — one of which is chaotic.

**So the decision the gate is waiting on is a campaign-design decision, and it belongs to the owner**:
whether the set of ten ladders should carry more tops. It cannot be taken from a mover count without
selecting the arena on the result, and this file has declined that five times.

### 9. Opposition raises the CEILING, not the average — and that closes the diagnosis

`w2-10-opera:f16` puts **ten** aircraft on the graded side and carries **2** movers. Its campaign says
why in its own contract: *"eight of the ten missions have no air opposition at all — the subject is
reach, not combat."* Split the 44 large cells on whether the committed file puts a hostile FIGHTER in
them [MESS]:

| large cells (≥ 4 own aircraft) | cells | mean movers | **max** |
|---|---:|---:|---:|
| opposed | 36 | 1.47 | **11** |
| unopposed | 8 | 1.50 | **2** |

**The means are the same and the ceilings are not.** Opposition does not make a cell gradable on
average; it is what makes a HIGH mover count possible at all — eight unopposed cells never exceed two,
because an air doctrine has nobody to be a doctrine against.

So the requirement, in its final and complete form, is three conditions and none of them is about the
genome or the gate:

| a cell can be informative only if it is | today |
|---|---|
| **large** — ≥ 4 aircraft on the graded side (§6) | 44 of 154 |
| **opposed** — a hostile fighter in the file (§9) | 36 of those 44 |
| **robust** — 0 flips of its own baseline (§2, `E-21`) | of the top twelve by movers, ten are; the two most gradable are not |
| and then reach S2's 8 movers of 24 | **1 of 36**, and it is one of the two chaotic ones |

That is the whole diagnosis, measured end to end, and every one of the four rows was found this round
rather than assumed. The gate is not waiting on a gene — the genome is complete. It is waiting on the
campaign set to contain three rungs that are large, opposed and robust at once, and the set contains
one.

### 10. Thirteen cells are invisible to level M, and every one of them is inert

The breadth's largest symmetric engagement is `o1-10-mole-cricket` — eight against eight — and it carries
`(16, 0)` on BOTH sides with **zero movers**. That is not a defect: the file says so itself, *"NO
AIRCRAFT IN THIS SORTIE DECLARES `objective survive`, AND NO FIGHTER FLIES IN IT AT ALL"*, and its
reading rule names five things to read — `campaign CARRY` lines, `site LAUNCH`, `net LOST`, the per-jet
`eng_*` debrief, the campaign's ATTRITION line. **The fitness reads none of them.**

[MESS, all 154 cells] **13 cells carry `M = 0` on every one of the 24 levers, and not one of them has a
single mover.** A rung whose product is an attrition arc rather than an objective count is structurally
invisible to an outcome class of `(V, M)` — and being invisible, it can never be informative, whatever
its size or opposition.

This is the fifth and last row of the diagnosis, and it is the one that cannot be fixed by building
missions: it is a statement about what the FITNESS reads. Widening level M to read a carry line or an
attrition arc is not proposed here — §1.2's argument against exchange rates applies to it, and a round
that widened the fitness after measuring which cells it cannot see would be selecting the instrument on
the result. It is booked, with its number, as `E-24`.

### 11. The cost

| | |
|---|---|
| runs | 3,850 lever + 15 field + 32 chaos + 502 regression + 12 probe + 108 scan = **4,519**, plus **1,094** campaign-layer runs (99 campaign runs × 10 steps + 104 standalone replays) |
| `sim/src/` | four files: two tuning entries, two pilot hooks, one decision block, one switch branch, and the F-16's two overrides |
| behaviour | **20 of 251 missions moved, one exit code**; eight missions lose every `FAIL`. Determinism over `--threads 1/2/4`: identical MD5 |
| the round's declared DEBT | the reading rules of those 20 missions are **not individually audited**. The tree requires per-mission justification and it is owed |

---

## State — round `E12` (2026-08-01): the arena PASSES, the evolution RUNS, and the fixed yardstick FALLS

The first passing arena in this file's history, the first evolution run on one, and a refusal that comes
from the instrument built for exactly this case. **`sim/src/` was not touched.**

### 1. The arena passes, and no constant was moved to get there

`E11` measured that a cell needs four properties at once and that the campaign breadth holds **one** such
cell in 154. The ten campaigns are LADDERS and stay untouched; a declared MEASUREMENT ARENA was built
beside them, thirty rungs, by that rule and nothing else.

```
S4 size        : 60 cells        (>= 6) ok
S5 yield       : 3 informative   (>= 3) ok   [ar-08-close-day, ar-15-ratio-one-two, ar-27-close-blue-high]
S6 distinctness: 0 identical pair(s)   ok
ARENA: PASSED   (60 cells, 3 informative)
```

All three carry 8–9 movers of 24, ≥ 3 outcome classes at ≤ 60 % modal, and **0 flips of 8**. **S7 did its
job on a fourth**: `ar-26-beam-three-two:f16` passed S1 AND S2 and was refused at **2 of 8** — the exact
false certification S7 was written for, and it was written before it knew any result.

**Not one constant was moved.** S1's 60 %, S2's 3/9, S4's six, S5's three, S6, S7's zero flips — all as
written. The yield the rule produces was measured first (≈ 5 % of cells) and the rung count follows from
it: thirty rungs, sixty cells, three informative.

### 2. The evolution runs, and level M decides in every generation

`fb_campaign_evolve.py`, **813 runs**, 6 generations, population 45, **9 genes and no blocker** — the full
list the owner goal names.

| generation | 0 | 1 | 2 | 3 | 4 | 5 |
|---|---:|---:|---:|---:|---:|---:|
| decided at level **M** | 308 | 245 | 239 | 239 | 245 | **246** |
| decided at level V | 1222 | 1412 | 1414 | 1412 | 1184 | 1126 |

**Level M — the level that counts fulfilled objectives — decides 239 to 308 comparisons every
generation.** In `E8` it decided **zero**. The fitness is finally grading the thing it was built to grade.

### 3. And the refusal, which is the round's product

| instrument (§3.6) | measured | acceptance |
|---|---|---|
| (a) fixed yardstick per generation | 0.611 · 0.611 · 0.611 · 0.611 · 0.611 · **0.444** | non-decreasing — **NO** |
| (b) cyclic triples | **T = 1.0000** (1 of 1) | ≤ 0.05 — **NO** |
| (c) doctrine trajectory | 0.0000 · 0.0312 · 0.0156 | moves |

Three distinct champions in six generations, and they differ in **exactly one gene** — the pickle lead
`pilot_attack_bias_s`, 0 → −0.625 → −0.3125, every other value identical:

| champion | `bias_s` | fixed yardstick |
|---|---:|---:|
| `g0_s2` | 0 | 0.611 |
| `g4_21` | −0.625 | 0.611 |
| `g5_23` | −0.3125 | **0.444** |

**The co-evolving fitness rose while the uninfluenceable measure fell** — `g5_23` wins its round robin at
0.652 and scores 0.444 against the frozen field, below both of its predecessors. That is the textbook
signature §6 names: *"a fitness rise measured only against the co-evolving population"* is not a finding.
**No §1 is published.**

**The honest weakness of (b), stated rather than leaned on:** with three distinct champions there is
exactly ONE evaluable triple, so `T = 1.0000` rests on n = 1 and is not by itself evidence of circling.
(a) is the load-bearing refusal: the fixed field is frozen text no genome can influence, and it fell.

**And the trap this round did not walk into:** stopping at generation 4 would have shown a flat 0.611
across the whole window and passed (a). Choosing the window after seeing the curve is the same move as
retuning a threshold after seeing the result, and it was declined here as it was declined eight times
before.

### 4. The cost

| | |
|---|---|
| runs | 1,500 lever + field + chaos screen (gate) + **813** (evolution) |
| `sim/src/` | **not touched** |
| the campaigns | **not touched** — thirty new `ar-*` rungs beside them, declared as an arena |
| what is published | **nothing**. §6's precondition was met for the first time, and §3.6's instruments refused the result |

---

## State — round `E13` (2026-08-01): THE FIRST PUBLISHED DOCTRINE SHIFT

`E12` passed the gate and its own instruments refused the champion, because the runner selected by the
co-evolving round robin — a measure §6 forbids publishing. E19 made the selection follow the fixed field.
Re-run on the identical arena, identical genome, identical 813 runs: all three circling instruments pass,
the fixed yardstick RISES, and the four exploit tests hold. This is §6's template, filled.

### §1 — The shift, in one sentence

**A netted four-ship halves its vertical stack: `pilot_flight_stack_frac` 1.5 → 0.75.**

### §2 — The genome diff, with bands

| gene | seed | champion | band | on a rail? |
|---|---:|---:|---|---|
| **`pilot_flight_stack_frac`** | 1.5 | **0.75** | 0 … 3 | no |
| `pilot_attack_bias_s` | 0 | −0.625 | −10 … 10 | no |
| `pilot_flight_trail_frac` | 1.5 | 1.125 | 0 … 3 | no |
| `pilot_flight_spread_frac` | 1.625 | 1.5391 | 0.25 … 3 | no |
| `dl` | — | on | allele | — |
| the other four | unchanged | unchanged | | |

No gene sits on a rail, so none of them is an unbounded band reported as a finding.

### §3 — The outcome ledger, against the FIXED yardstick

| generation | 0 | 1 | 2 | 3 | 4 | 5 |
|---|---:|---:|---:|---:|---:|---:|
| fixed yardstick | 0.722 | **0.889** | 0.889 | 0.889 | 0.889 | 0.889 |

| cell | champion | yardstick baseline | |
|---|---|---|---|
| `ar-08-close-day:f16` | V=19 M=11 | V=18 M=10 | better on both deciding levels |
| `ar-15-ratio-one-two:f16` | V=20 M=13 | V=19 M=11 | better on both |
| `ar-27-close-blue-high:f16` | V=20 M=16 | V=20 M=15 | better on M |

### §4 — The mechanism, as a chain of published channels

[MESS, the three cells, four genomes: the seed, the seed + `dl=on`, that + the halved stack, and the
champion. One run each, the arena's own channel CSV.]

| cell | `flt_switch` seed → half-stack | `(V, M)` |
|---|---|---|
| `ar-08-close-day` | **83 → 77** | (18,10) → (19,11) |
| `ar-15-ratio-one-two` | **66 → 39** | (20,13) → (20,**12**) |
| `ar-27-close-blue-high` | **11 → 7** | (20,15) → (20,16) |

**The chain:** `pilot_flight_stack_frac` scales `FormationStackM` in `FBPilot::FormationStation`
(`altM = lead.AltM + k · stackM`) → the flight's members sit closer in altitude → **`flt_switch`, the
sort's re-assignment count, falls on all three cells** → on two of the three that converts into a higher
objective count.

**And the datalink bit is NOT the mechanism**: `dl=on` alone leaves every channel and both deciding
levels identical to the seed on all three cells. The shift is the stack.

**The honest limit, stated with the finding:** on `ar-15-ratio-one-two` the halved stack alone COSTS one
objective (M 13 → 12) while raising `eng_shots` 1 → 6 and `flt_assign` 7 → 31; the champion's other two
genes recover it to M = 13. The channel moves in one direction on all three cells; **the outcome does
not**, and "less churn is better" is therefore not published as a law.

### §5 — The counter

The archive holds **15** members. The one that comes closest is `g5_23`, the champion `E12` produced under
the old selection: same family, `pilot_attack_bias_s = −0.3125` instead of −0.625, and it scores **0.444**
against the same fixed field where this champion scores 0.889. The counter to this doctrine is therefore
its own neighbour in one gene — which is the honest way of saying the pickle lead is sharp and the stack
is not.

### §6 — The exploit audit

| test | result |
|---|---|
| **X1** arena invariance — advantage survives on ≥ 2 informative geometries | **PASS.** Better on all three; the claim rests on `ar-15` and `ar-27`, which flip **0 of 8** |
| **X2** invariance to declared ignorance | **n/a**, the same reason S3 is: both airframes are FlightBox's read-only model copies, so there is no declared-ignorance band to perturb |
| **X3** mechanism nameable in published channels | **PASS** — §4, with the column and the number at every link |
| **X4** the physical audit | **PASS.** Spawn ±3 m, 8 samples: `ar-15` 0/8, `ar-27` 0/8, `ar-08` 1/8 (§5's floor is 2/8). Timeout × 1.5: all three held. Partition class: the fitness contains no such count (§1.3) |

**Two disclosures that belong to the audit, not to a footnote.** (a) `ar-08` reads 1 of 8 on the coarse
grid and **4 of 24 = 16.7 %** on the 0.25 m grid — below §5's 25 % floor, but closer to it than the coarse
reading suggests, which is why the claim is carried by the two cells that flip zero. (b) `fb_champion_audit.py`
applied S7's ADMISSION threshold (0 flips) to a champion audit until today; §10/E18 wrote the distinction
down in advance, and the tool now uses §5's floor with that quote in its source.

### §7 — The cost

| | |
|---|---|
| runs | 813 (evolution) + 12 (mechanism) + 27 (X4) + 24 (the fine grid on `ar-08`) |
| `sim/src/` | **not touched** |
| the campaigns | **not touched** |
| what moved | one line of `tools/fb_campaign_evolve.py` (E19) and one threshold in `tools/fb_champion_audit.py` |

### §7a — THE VALIDATION, AND IT REFUTES THE TRANSFER

§1 was earned under §5's tests, and §5's X1 asks only that the advantage survive on *"the arena's other
informative geometries"* — of which there are three. So the shift was carried to the set it was NOT
selected on: **the 154 campaign cells**, which played no part in choosing it.

[MESS, 154 cells × 2 genomes, `--elev const`, one run each, `fb_fitness.pair_points` per cell]

| champion against its own seed, over the campaign breadth | |
|---|---:|
| better | **17** |
| **worse** | **38** |
| equal or incomparable | 99 |
| `flt_switch` falls / rises / unchanged | 6 / 9 / 139 |

**The shift does not transfer, and on the campaigns it is net harmful.** The mechanism does not either:
the sort churn falls on 6 cells and RISES on 9, against the arena's three-of-three.

**What this qualifies, and what it does not.** §1 stands as published — it was earned under the tests
this file declares, on an arena that passed every criterion, with a mechanism named in published
channels. What it is NOT is a doctrine for this tree: it is a doctrine for **close-in four-ship geometries
of the kind the arena is built from**, and the owner goal's own words are the right verdict on it —
*"die Evolution beider Piloten-KIs über die Kampagnenbreite, nicht über einzelne Geometrien."* By that
standard this shift fails, and the failure is the more valuable half of the round.

**And it names an instrument weakness that no earlier round could see.** X1 as written — *"survives on
≥ 2 of the arena's other informative geometries"* — is nearly empty when the arena holds three: two of
three is a two-sample test. A champion can pass X1 and be net negative on 154 independent cells, and this
round measured exactly that. Booked as **E-25**.

### §7b — CORRECTED: the shift that transfers is `g1_61`, and the damage is what the search added AFTER it

§7a measured the FINAL champion on the breadth and found it net harmful. That was the right test on the
wrong genome. Every champion of the run was then carried to the same 154 cells [MESS, 924 runs]:

| champion | what it adds | arena yardstick | breadth better / worse | net |
|---|---|---:|---|---:|
| `g0_s3` | `dl=on` | 0.722 | 0 / 0 | **+0** |
| **`g1_61`** | **`stack` 1.5 → 0.75** | **0.889** | 14 / 15 | **−1** |
| `g2_51` | — | 0.889 | 14 / 15 | −1 |
| `g4_21` | `bias_s` → −0.625 | 0.889 | 18 / 37 | **−19** |
| `g5_40` | `trail` → 1.125 | 0.889 | 17 / 38 | **−21** |

**The published shift's core gene is breadth-NEUTRAL.** Halving the vertical stack costs one cell of 154
— fourteen better against fifteen worse — while taking the arena's fixed yardstick from 0.722 to its
maximum 0.889. `dl=on` alone is exactly neutral on all 154, which confirms on the breadth what the arena
already showed: the datalink bit is not the mechanism.

**The damage is entirely what the search added afterwards.** From generation 1 the arena's yardstick is
FLAT at 0.889 — the arena cannot see any further improvement — and the search kept moving anyway, adding
`bias_s = −0.625` (breadth −19) and then `trail = 1.125` (breadth −21). **A flat yardstick is not a
plateau to walk across; it is the arena saying it has nothing left to tell you, and the walk was paid for
on 154 cells the arena never looked at.**

**§1 is therefore corrected to name `g1_61`**: `pilot_flight_stack_frac` 1.5 → 0.75, `dl=on`, everything
else at the seed. It rises on the arena, it is neutral on the breadth, and its mechanism (§4) is the
channel that carried it. The three later genes of §2 are withdrawn from the claim and stand here with
their measurement, which is this file's rule for a rejected approach.

**And the selection rule this yields, stated as a consequence rather than a new criterion:** among
champions tied on the fixed yardstick, the one to publish is the one that costs least on the set it was
not selected on. `g1_61` and `g5_40` are indistinguishable on the arena (0.889 both) and 20 cells apart
on the breadth.

### §7c — SELECTION ON THE BREADTH: the gene's own axis, swept over all 154 cells

§7b showed the core gene is breadth-neutral. The obvious next question is whether ANY value of it is
breadth-POSITIVE, and it is answerable without fishing: the axis is the one §4's mechanism already names,
and it is swept end to end rather than searched. [MESS, 154 cells × 7 genomes = 1,078 runs, everything
but `pilot_flight_stack_frac` held at the seed, `dl=on`]

| `stack_frac` | better | worse | unchanged | net | one-sided binomial |
|---:|---:|---:|---:|---:|---|
| 0 | 11 | **21** | 122 | −10 | **p ≈ 0.025 for WORSE** |
| 0.25 | 13 | 18 | 123 | −5 | — |
| 0.5 | 12 | 18 | 124 | −6 | — |
| **0.75** (published) | 14 | 15 | 125 | **−1** | p = 0.644, i.e. nothing |
| 1.0 | 9 | 18 | 127 | −9 | — |
| **2.0** | **17** | 10 | 127 | **+7** | p = 0.124 — **not significant** (19 of 27 would be needed) |

**Two statements, and only one of them is established.**

1. **Collapsing the stack is measurably harmful on the campaign breadth.** `stack = 0` — every member of
   the flight at the lead's altitude — scores 11 better against 21 worse, and the reverse test gives
   **p ≈ 0.025**. That is a finding.
2. **Widening it looks better and is NOT established.** `stack = 2.0` is the only value net-positive on
   the breadth (+7), and at 17 of 27 responsive cells it misses significance (p = 0.124). It is reported
   as a direction worth another round, not as a doctrine.

**And the headline is the disagreement.** The arena selected 0.75 — halve the stack — and reached its
yardstick maximum there. The breadth's own best value is **2.0, the opposite direction**, and the arena's
choice is exactly neutral on it. The two measurement sets do not merely differ in strength; **they point
opposite ways on the same gene.** §7a called the shift arena-specific; §7c says what the breadth would
have chosen instead.

**What this does NOT license.** Republishing §1 with `stack = 2.0` would be selecting on a p = 0.124
result, which is the same move as retuning a threshold — declined. The value stands here with its
measurement, which is this file's rule for a candidate that did not reach its bar.

### §8 — Exploits found

**None new.** X-1 through X-5 stand as they are. The round's own near-miss is disclosed in §6(a) rather
than filed as an exploit: `ar-08` is noisier than the coarse grid showed, and the claim does not rest on it.

---

## State — round `E14` (2026-08-03): a doctrine shift over the CAMPAIGN BREADTH, p = 0.0005

`E13` published a shift on the arena and its own validation refuted the transfer. This round asks the
question the other way round: **not "what does the arena select", but "what moves the 154 campaign
cells".** The axes are not searched — they are the four gene families `E11` MEASURED as broadly active,
swept end to end, every other gene at the seed.

### §1 — The shift, in one sentence

**The pilot pickles one decision tick early: `pilot_attack_bias_s` 0 → −0.2 s.**

### §2 — The genome diff

| gene | seed | shifted | band | on a rail? |
|---|---:|---:|---|---|
| `pilot_attack_bias_s` | 0 | **−0.2** | −10 … 10 | no |
| the other eight | unchanged | unchanged | | |

### §3 — The outcome ledger over the breadth, and the mirror confirms it

[MESS, 154 cells × 9 genomes = 1,386 runs, `--elev const`, `fb_fitness.pair_points` per cell]

| genome | better | worse | net | one-sided binomial |
|---|---:|---:|---:|---|
| **`bias-early` −0.2 s** | **25** | 9 | **+16** | **p = 0.005** |
| `bias-late` +0.2 s | 1 | **32** | −31 | the same axis, the other way |
| `stack-wide` 2.0 | 17 | 10 | +7 | p = 0.124 |
| `sort-near` | 17 | 13 | +4 | p = 0.292 |
| `net-on` | 0 | 0 | ±0 | inert on all 154 |
| `energy-high`, `sort-left`, `trail-wide` | 3/4/2 | 1/2/2 | +2/+2/±0 | — |

**One axis, two directions, both speak.** A tick early is 25 : 9; a tick late is 1 : 32. That symmetry is
what distinguishes a real gradient from a lucky sample, and no other gene family comes near it.

**And after the chaos screen it gets STRONGER.** All **34** cells the shift moves were put through §5's
0.8 m spawn grid — **in both directions, blind to which way they went**. Six are chaotic (≥ 2 of 8 flips)
and are excluded because §5 forbids a claim on them at all:

| | better | worse | net | p |
|---|---:|---:|---:|---|
| all 34 responsive cells | 25 | 9 | +16 | 0.005 |
| **the 28 that are not coins** | **23** | **5** | **+18** | **0.0005** |

The screen removed more bad cells than good ones. That is the direction a screen is supposed to move a
real effect, and the opposite of what it does to a lucky one.

### §4 — The mechanism, and this tree measured it before it was a doctrine

The chain is **already published in this file** as X-2, found by an earlier round on eight strike cells
over four campaigns, two stores and four altitudes:

`ATTACK_RELEASE biasS` → `stores DELIVERY predErrM = 52.57 m = gs × 0.227 s` → `aimLongM 36.34` of
`aimErrM 36.38` (99.9 % along-track) → `damage rangeM 33.66` against a Mk-84's 17.7 m fail radius.

**The minimum of `aimErrM(bias)` sits at −0.20 ± 0.05 s on every one of those eight cells — a constant
TIME, not a constant distance, which is what says it is a LATENCY** and not a geometry. The pilot leads
the cue by the bus latency plus his own decision tick; what remains uncancelled is ~0.2 s, and at a
loaded F-16's 231 m/s that is 46 m of track.

### §5 — The counter

Its own mirror. `+0.2 s` is 1 better against 32 worse on the same 154 cells — the sharpest counter in
this file, and it is the same gene at the same magnitude with the sign flipped.

### §6 — The exploit audit

| test | result |
|---|---|
| **X1** arena invariance | **PASS** — of four cells checked in detail, three flip 0 of 8 and all three are better; across the breadth the claim rests on 28 screened cells |
| **X2** declared ignorance | **n/a**, as S3: both airframes are read-only model copies |
| **X3** mechanism in published channels | **PASS** — §4, and it was measured independently before this round |
| **X4** the physical audit | **PASS on the screened set.** 34 responsive cells put through the 0.8 m grid; 6 chaotic excluded by §5's floor; timeout × 1.5 held on every cell checked. Partition class: the fitness contains no such count |

### §7 — What this is, stated exactly

**It is not a tactic. It is a defect of our own default, and the whole campaign breadth says so.**
X-2 already filed it as *"a defect of the default"* — `FBF16Pilot::AttackReleaseBiasS()` returns **0.0 s**
— and this round is that verdict measured on 154 cells instead of eight. The right repair is therefore
not to publish a doctrine but to **give the airframe its own number**, derived from the latency it
already knows, and the reason it has not been done in this round is the tree's own rule: a model number
needs a source, and *"ein besseres Missionsergebnis ist ausdrücklich KEIN Beleg"*. What is measured here
is the SIZE of the defect and its sign; what is owed is a derivation.

**The debt is discharged in the round that follows, and it splits E14's number in half.** Traced through
the release chain, the uncompensated term is `FBStoresSystem::kSeparationDelayS` — the acknowledgement is
not the separation; the store enters a queue and `missions/FBOrdnance` drains it one sim tick later.
[MESS] `attack-ccrp`, read t = 71.4 → `CMD_ACK` t = 72.0 → `stores SEPARATION` t = **72.1**: the chain
costs 0.7 s and the pilot held 0.6 s. Adding the third term to `leadS` is a derivation with a source, not
a tuning, and it moves the true delivery from **+38.8 m to +16.0 m long** — a gain of 22.8 m against a
predicted 23.2 m. **So −0.1 s of E14's −0.2 s was this defect and is now structurally gone from every
airframe.** The other −0.1 s was never a doctrine either: it is a bias averaging out half of X-3's
release-clock quantum, whose floor this round's residual (16.0 m = 0.069 s, inside one decision tick)
lands squarely inside. Publishing −0.2 s would have frozen one defect and one partition artefact into a
single tuning key on every aircraft in every geometry. **E14's refusal to publish was correct, and the
measurement that made it refusable is what located the defect.** Details and the second finding it
exposed (`C28`, the release log's own 0.2 s lag) in [`pilot.md`](pilot.md) §4.2 and
[`air-to-ground.md`](air-to-ground.md) Gaps.

### §8 — Exploits found

None new. X-1 … X-5 stand. This round's result is the promotion of X-2 from a local measurement to a
breadth-wide one, with a significance test and a chaos screen it did not have before.

---

## State — round `E16` (2026-08-03): the three measuring rigs cannot carry a shift, and the reason is arithmetic

The round label continues the journal's `E15`; §9/§10's rows `E15`–`E19` are CONTRACT labels, not rounds,
and the collision is theirs rather than this section's.

`E15`'s three new rigs (`sat-01`/`sat-02`/`sat-03`, commit `85c1a74`) were the first cells in this tree
to reach S5's yield. This round asks the only question that matters about them: **can a doctrine shift
be published on them?** The answer is no, it is decidable before a run, and the runs then say something
the arithmetic did not — the one candidate that survives every screen is a DEFECT of the emission gate,
and it is the third latch this tree has found dressed as a doctrine.

### §0 — The ceiling, derived before the first run

§6 publishes a shift as a sign test over PAIRED CELLS (`E14`: 154 cells, 25 : 9, p = 0.005). With `n`
cells the smallest attainable one-sided p is the unanimous case `2^-n`:

| n cells | best attainable p |
|---:|---:|
| 1 | 0.500 |
| 2 | 0.250 |
| **3** | **0.125** |
| 4 | 0.0625 |
| **5** | **0.031** |

**The arena has three cells, so no result on it can reach p ≤ 0.05 — not with a better genome, not with
a longer lever file, not with more runs.** And §5's chaos screen admits exactly one of the three
(`sat-02`, 0 of 8; `sat-01` 1 of 8, `sat-03` 4 of 8), so the admissible `n` is **1** and the ceiling is
**p = 0.5**. The gap to a publishable shift is **four more chaos-clean graduable cells**, and that is the
round's actionable number.

Everything below is therefore a MEASUREMENT and not a claim. It is worth the runs because the direction
data, the clock test and the mechanism are what the next rig must be built against.

### §1 — The shift, in one sentence

**There is none.** The best candidate over the three cells is `pilot_emcon_frac` ≥ 1.35 ("stop going
silent on a mate's report") at **2 better : 1 worse, p = 0.5**; after X4.2 removes `sat-03` it is
**1 : 1**. §6's binding rule is honoured in the other direction than usual: §4 is full, §3 is empty, and
it is §3 that decides.

### §2 — The genome diff of the candidate

| gene | seed | candidate | band | on a rail? |
|---|---:|---:|---|---|
| `pilot_emcon_frac` | 1.0 | **1.35** | 0 … 3 | no — and it is not a point either (§6a) |
| the other eight | unchanged | unchanged | | |

### §3 — The outcome ledger, and the seed wins it

[MESS, 3 cells × (1 baseline + 24 levers) = 75 runs, `--elev const`, `fb_fitness.compare` against each
cell's OWN baseline. `bias-rail` excluded — `tools/levers-campaign-g5.txt` calls it "no delivery at all".]

The lever set reproduces `85c1a74`'s committed table exactly under a freshly linked binary
(`b040e6ef30061351` vs the recorded `429ab24f122c8053`): 9 / 9 / 10 outcome classes, modal
60.0 % / 52.0 % / 52.0 %, movers 10 / 12 / 12.

| lever | `sat-01` | `sat-02` | `sat-03` | better : worse |
|---|:--:|:--:|:--:|---:|
| `emcon-wide` | − | **+** | **+** | **2 : 1** |
| `shape-stacked` | − | **+** | **+** | **2 : 1** |
| `shape-flat` | · | − | + | 1 : 1 |
| `net-off` | − | **+** | − | 1 : 2 |
| `shape-abreast` | − | − | + | 1 : 2 |
| `shape-tight` | − | − | + | 1 : 2 |
| `shape-wide` | − | − | + | 1 : 2 |
| `shape-trail` | · | − | − | 0 : 2 |
| `bias-early` | − | − | − | 0 : 3 |
| `bias-late` | − | − | − | 0 : 3 |
| `ccip-tight` | − | − | − | 0 : 3 |
| | | | | **9 : 22** |

**The seed genome dominates its own lever set.** Pooled over the 31 moved (cell, lever) pairs it is
9 better against 22 worse; the pairs are not independent (11 levers over 3 cells), so the pooled
`P(X ≤ 9 | n = 31)` = 0.015 is quoted as a description and NOT as a significance test — §6 counts cells.

**Half the lever file is the identity map.** 12 of the 24 levers are bit-identical to the baseline in all
28 published channels — including `durationS` and the energy integral `bfm_es` — on all three cells:
`cover-off/one/three`, `energy-low/mid/high`, `net-on`, `sort-left`, `sort-near`, `ccip-open`,
`emcon-tight`, `emcon-mid`. Filed as `E-26`.

**The counter-probe, per axis** (§6's requirement that a one-directional axis is suspect):

| axis | below the seed | above the seed | verdict |
|---|---|---|---|
| `pilot_attack_bias_s` | `bias-early` −0.1 s: 0 : 3 | `bias-late` +0.1 s: 0 : 3 | **symmetric-worse — the seed sits on the optimum.** After `E15` removed `kSeparationDelayS` from the residual this is exactly what `E14`'s corrected reading predicts, measured on cells `E14` never saw |
| `pilot_flight_spread_frac` | `shape-tight` 0.35: 1 : 2 | `shape-wide` 2.5: 1 : 2 | symmetric-worse |
| `pilot_flight_trail_frac` | `shape-abreast` 0: 1 : 2 | `shape-trail` 2.0: 0 : 2 | symmetric-worse |
| `pilot_flight_stack_frac` | `shape-flat` 0: 1 : 1 | `shape-stacked` 3.0: 2 : 1 | speaks both ways — and §6a dissolves it |
| `pilot_emcon_frac` | `emcon-tight` 0.1, `emcon-mid` 0.4: **INERT** | `emcon-wide` 3.0: 2 : 1 | **one-directional, and §6b gives the mechanism** |
| `datalink` | `net-on` = the missions' own briefing: INERT | `net-off`: 1 : 2 | the mirror IS the baseline |

### §3a — The clock test, and it voids two of the three cells

`E15`'s refutation of `w3-09-saturation` made this mandatory: a mover that shifts the truncation instant
moves the UHR, not the result. `FBMissionSim::Conclude` says it in its own comment — *"a K.O. always ENDS
the run but only DECIDES it when it was nobody's declared objective"* — so the first WRECK to reach the
ground stops the clock for every unit, including one whose loss is somebody's declared objective.

| direction | run got | reading |
|---|---|---|
| better | shorter | **conservative** — the clock worked against the mover |
| better | longer | **confounded** |
| worse | longer | conservative |
| worse | shorter | confounded |
| either | unchanged | clean |

Applied to the 31 pairs, and then checked against §5's own judge-evasion detector at `timeout × 1.5`:

| cell | baseline | what the clock does | `timeout × 1.5` on the BASELINE | on the CANDIDATE | S7 |
|---|---|---|---|---|---|
| `sat-01` | 520.0 s | 7 of 10 movers leave it exactly, 3 shorten it | **(32, 23) → (30, 22) MOVED** | (26, 19) held | 1 of 8 |
| `sat-02` | 520.0 s | 3 better movers all SHORTEN it (conservative); 8 of 9 worse movers shorten it (confounded) | (14, 8) held | (14, 10) held | **0 of 8** |
| `sat-03` | 301.7 s (truncated) | **6 of 6 better movers EXTEND it to the full 520 s** | (24, 20) held | **(26, 24) → (23, 22) MOVED** | 4 of 8 |

`sat-03`'s entire better column is the clock, and two independent instruments say so: the duration diff
(+218.3 s on five of the six) and X4.2. `sat-01`'s BASELINE is itself timeout-dependent — its (32, 23) is
"the run stopped before it got worse" — so its reference point is a function of the clock. **`sat-02` is
the only cell of the three that is clean on the spawn grid AND on the clock, on baseline and candidate
alike.** Applying the judge-evasion detector to a cell's BASELINE rather than to a champion is this
round's extension of §5 and is stated as such.

### §4 — The mechanism: the emission gate LATCHES on a report nothing can refresh

Every link is a published channel with its number, all on `sat-02`'s baseline, `telemetry.csv` of `pb1`:

| # | link | number |
|---|---|---|
| 1 | `FBPilot.cpp:1523` `EmconSilent_ = other && nearestM > radiateM`, `radiateM = pilot_emcon_frac × FBF16Pilot::EmconRadiateNm()` | seed = 1.0 × 40 nm = **74.1 km** |
| 2 | `fcr_on` | 1 from t = 0.1 s to t = 57.4 s, then **0 for the remaining 462.5 s** — 574 of 5 200 ticks = **11.0 %** |
| 3 | the latch instant: `fcr_contacts` 0 → 4 and `flt_src` 0 → 1, `flt_assign` 0 → 3 | t = **56.0 s** (≈ 104 km, own first detection) |
| 4 | one tick later `fcr_on` → 0, `fcr_contacts` → 0, **`flt_src` → 0 and stays 0 for 462 s** | t = **57.5 s**. The picture layer counts ZERO sources while the emission gate keeps reading one |
| 5 | the report's range, bracketed by the gene itself: silent at `f = 1.30` (52.0 nm = 96.3 km), radiating at `f = 1.35` (54.0 nm = 100.0 km) — **the same bracket on all three rigs** | the reported point sits in **(96.3, 100.0] km and does not leave it for 462 s** while the geometry closes at 478 m/s, i.e. **220 km of closure against < 3.7 km of report movement**. That is not a track, it is a frozen number |
| 6 | `fcr_lock` | **0 of 5 200 ticks** |
| 7 | `sms LAUNCH_SOLUTION` in the whole run | 18 lines, **0 of them blue**; `eng_shots` = 0 on all six F-16 |
| 8 | `mission OBJECTIVE … kill unit pmi*` | **0 of 8 met**; (V, M) = (14, 8) |

**Break the latch either way and the same thing happens.** Two different genes, one phenotype, one key:

| | `fcr_on` | `fcr_contacts` > 0 | `fcr_lock` | named kills | key |
|---|---:|---:|---:|---:|---|
| baseline | 574 / 5 200 = 11.0 % | 15 ticks | **0** | 0 of 8 | (14, 8) |
| `pilot_emcon_frac ≥ 1.35` | 2 655 / 2 655 = **100 %** | 1 792 | 464 | **2 of 8** | (14, 10) |
| `dl = off` | 2 840 / 2 840 = **100 %** | 1 710 | 543 | **2 of 8** | (14, 10) |

`net-off` removes the reporter (`other` = false); `emcon ≥ 1.35` raises the gate above every reported
range. Neither is a tactic — **both are ways of not reading a 462-second-old report.**

### §5 — The counter, and it is real

The same phenotype on `sat-01` gets the flight lead killed: `emcon ≥ 1.35` ⇒ `monitor KO unit=bl1
reason=CFIT` at **t = 133.5 s**, the run ends there, objectives **27 met of 36 → 23 of 36**, key
(32, 23) → (26, 19). A jet that radiates without pause is a target, and the chain is `duels.md` D3's
published one. **The axis has a genuine trade-off in both directions** — which is exactly why the latch
must be repaired rather than tuned around: a defect that sits on a real trade-off is invisible to a
sweep that only reads the sign.

### §6 — The exploit audit

| test | result |
|---|---|
| **X1** arena invariance | **n/a as a verdict** — `E-25` already says X1 is a two-sample test on a three-cell arena. What is measured instead: the phenotype reproduces through a SECOND, independent gene (`dl=off`) to the same key on the same cell |
| **X2** declared ignorance | **n/a**, as S3: both airframes are read-only model copies |
| **X3** mechanism in published channels | **PASS** — §4, eight links, every one with its number |
| **X4.1** lucky trajectory | **PASS** — champion flips 0 of 8 (`sat-01`), **0 of 8** (`sat-02`), 1 of 8 (`sat-03`), all under §5's floor of 2 |
| **X4.2** judge evasion | **FAIL on `sat-03`** — (26, 24) → (23, 22) at `timeout × 1.5`. Held on `sat-01` and `sat-02` |
| **X4.3** partition | the fitness contains no such count; `deliveries`/`releases` unmoved by the candidate on `sat-02` |
| **verdict** | X4 FAILED ⇒ §5 files this as an **exploit finding**, not a doctrine shift. It is `X-6` below |

**(a) The best-looking gene dissolves under a finer grid.** `pilot_flight_stack_frac` was 2 : 1 on the
coarse three alleles. Swept on 10 points [MESS, 3 × 10 runs] its sign along the ordered grid is, on
`sat-02`: 0 → −, 0.25 → +, 0.5 → +, 0.75 → +, 1.0 → seed, 1.25 → +, 1.5 → −, 2.0 → −, 2.5 → −, 3.0 → +.
**Four sign changes**, and the tooth positions do not agree between cells — `sat-01` is better at
{1.25, 1.5, 2.5} and worse at {0, 0.25, 0.5, 0.75, 2.0, 3.0}, `sat-03` is the near-inverse of that in the
lower half. The gene ACTS (9 of 9 non-seed grid points move the class, while the 0.8 m spawn grid moves
none) but its DIRECTION is not a function of its value. A three-allele lever file cannot see this, and
`shape-stacked`'s 2 : 1 is one tooth of a comb.

**(b) The emcon gene is a three-valued STEP, not a gradient.** 12 grid points over 0 … 3 produce three
phenotypes: always-silent (`f = 0`, distinct on `sat-01` only), the seed's behaviour (0 < `f` ≤ 1.30,
**bit-identical to the baseline in all 28 channels**), and always-radiating (`f` ≥ 1.35, bit-identical to
each other). Bisected to (1.30, 1.35] on all three rigs. So `emcon-tight` and `emcon-mid` are not two
points of a gradient — they are the identity map, and the G5 family in `levers-campaign-g5.txt` samples
one phenotype twice and the other once.

### §7 — The cost

| | |
|---|---|
| runs | **233** — 75 sweep + 24 S7 + 69 fine grid + 18 bisect + 33 X4 + 6 baseline-timeout + 8 kept diagnostic |
| wall | ≈ 45 min at `--jobs 6 --threads 2`, 6 cores |
| what moved in the tree | **nothing.** `sim/src`, `sim/vendor`, `sim/assets/aircraft` and every committed mission are untouched; the arena's own `tree_clean()` passed before and after every sweep |
| what was invalidated | every `build/*-channels.csv` resume index — the freshly linked `fb-gym` hashes to `b040e6ef30061351` and `gym_identity` refuses them all. The link hash is cosmetic: the 3-cell table reproduces `85c1a74`'s numbers exactly |
| artefacts | `sim/build/e15/` — `sat-channels.csv`, `fine-channels.csv`, `bisect-channels.csv`, `sat-arena.log`, the two readers |

### §8 — Exploits found

Three new, `X-6` … `X-8`, all filed below. `X-6` is the round's product.

---

## State — round `E17` (2026-08-04): the guillotine is out of the verdict, and the arena has two cells
The round label continues the journal's `E16`; §9/§10's rows `E15`–`E19` are CONTRACT labels, not
rounds, and the collision is theirs rather than this section's.

`E-27` names the blocker as a number: §6 publishes over paired cells, the smallest attainable one-sided
p is `2^-n`, and S7 admitted **one** of the three `sat-*` rigs. This round attacks the ROOT the builder
of those rigs named — *"an end-referenced objective on a run whose end is an EVENT is chaos-sensitive by
construction"* — measures whether that is actually true, repairs it in the judge, and then builds cells
against the repair. **The arena goes from one chaos-clean gradable cell to two.** The ceiling moves from
p = 0.500 to p = 0.250, and the honest headline is that three cells are still missing.

### §0 — Everything committed about the three rigs was re-measured first, and it had moved

`X-6`'s repair (commit `d8c1a26`) changed the pilot's emission behaviour, and its own note says the
`sat-*` baselines "ha[ve] to be re-flown before the rig is used again". Re-flown, 3 cells × 25 levers +
24 chaos runs, `--elev const`, simulator `a763d63ebf97c921`:

| cell | distinct | modal | movers | S1 | S2 | **S7** |
|---|---:|---:|---:|:--:|:--:|:--:|
| `sat-01-belt-channels` | 10 | 52.0 % | 12 | ok | ok | **1 of 8** |
| `sat-02-picture-split` | 7 | 60.0 % | 10 | ok | ok | **0 of 8** |
| `sat-03-escort-shield` | 10 | 44.0 % | 14 | ok | ok | **5 of 8** (was 4) |

`E-27`'s count survives the re-measurement: **n = 1**, ceiling p = 0.500.

### §1 — The root, and the claim is TRUE but INCOMPLETE

[MESS, `sat-03`, 9 runs on §5's own 0.8 m grid, per-objective diff against `dx = 0`] the five flips
decompose exactly:

| what moved | in how many of the 5 flips | class |
|---|---:|---|
| `ee1` V TIMEOUT → FAIL **and** its `survive` met → violated | **5 of 5** | the FIGHT — a 0.8 m spawn shift decides whether the flight lead is shot down |
| `ee3` + `ee4` `avoid zone sa6belt exposure 200` unmet → **met** | 4 of 5 | the CLOCK |
| `ee4` `kill unit emib3` unmet → met | 1 of 5 | the FIGHT |
| `ec1` `kill unit etgte` met → unmet | 1 of 5 | the RELEASE LATTICE (below) |

**The clock half is proved by a number that cannot be argued with.** `ee3`'s belt dwell at t = 317.9 s is
**175.2 s in the 520 s baseline and 175.2 s in the run that stopped at 317.9 s** — the same trajectory to
the tenth of a second. The budget is 200 s. One reads `unmet` (the run went on to 305.5 s of dwell), the
other `met`. Over the grid the same objective takes four different answers from four different truncation
instants (520.0 → 305.5 s dwell, 379.1 → 237.0, 338.7 → 195.6, 317.9 → 175.2) while its own chaos
amplitude at a FIXED instant is **1.0 s** (174.8…175.8). A 0.6 % disturbance is amplified into a bit.

**And the incompleteness matters as much:** every flip also contains a genuine combat coin. A verdict
rule cannot make that land the same way twice, so the repair below is necessary and not sufficient — the
rest is the mission author's geometry.

### §2 — The repair: `until <s>`, one rule, and it is in the judge

[`missions/verdict.md`](missions/verdict.md), "The seventh thing in the vocabulary". *An objective that
declares `until <s>` has its state FROZEN at that sim time.* Not a kind, not a per-kind predicate: the
same `StateOf` the judge already computes, read once at an instant the MISSION named instead of one the
run's events chose. `FBObjectiveCovers` is untouched (covering is a property of the declaration), the
judge gets no new source, and a span at or past the `timeout` is a parse error.

| gate | result |
|---|---|
| conservation | **3 337 / 3 337** telemetry files byte-identical (SHA-256) and **287 / 287** `events.log` identical over the whole pre-round tree, against the pre-round binary |
| the repair does what it claims | `sat-03` windowed and otherwise untouched: S7 **5 of 8 → 1 of 8** |
| the frozen verdict is PUBLISHED, not asserted | `sat-04`, 25 lever runs: **864 of 1 000** `mission OBJECTIVE` states bit-identical to their own `mission WINDOW_CLOSED` state, **0 mismatches**; the other 136 belong to units that concluded before their span closed |

### §3 — `sat-04-vul-window`: the cell the repair buys

`sat-01`'s geometry with the two measured chaos channels closed and nothing else touched — a controlled
experiment rather than a new question, so that the repair stays attributable.

| | |
|---|---|
| baseline | (27, 18), duration 224.6 s |
| S1 | 9 distinct classes, modal **52.0 %** — ok |
| S2 | **12 movers of 24** (11 excluding the broken `bias-rail`) — ok |
| **S7** | **0 of 8** |
| the clock, checked | the duration over the lever set spans **133.5…520.0 s** and the class does not follow it: `emcon-tight` (520.0 s) and `emcon-mid` (514.7 s) give the same (28, 20); `shape-abreast` (520.0 s) and `shape-trail` (225.8 s) give the same (28, 19). X-7's confound is structurally absent, because everything graded was frozen at t = 220.1 |

Its two new devices, both measured: aim points at the MIDPOINT of their own release lattice (spans 22.9 /
5.9 / 6.0 / 22.9 m), and `zone lane` — a declared cylinder whose radius (3 000 m) is [SET] BETWEEN the
formation's own lateral station steps (1 852 m and 3 704 m), which is what turns a shape allele into a
step. Its lane dwells separate by 5…20 s against a 1.0 s chaos amplitude.

### §4 — Two laws this round measured, and both bound every future rig

**(a) G6 is structurally excluded from any S7-clean cell that grades a delivery against a lethal
radius.** [MESS] the release is a DECISION TICK, so the impact is quantised at 22.9 m of track; the whole
`pilot_attack_bias_s` lever is ±0.1 s = **23 m**. A lever of size L can cross a threshold with a margin
M on both sides only if L > 2M, and 23 > 44 is false. The bias gene and the chaos move the same quantity
by the same amount — that is `sat-01`'s 1-of-8 flip and `sat-03`'s `ec1` bit, one mechanism twice.

**(b) `protect` and `survive` are gradable only AFTER the exchange, and the exchange is the chaos.**
Corollary, measured rather than reasoned: `sat-05`, `sat-03` windowed at 220 s, collapses from 14 movers
and a 44.0 % modal share to **4 movers and 84.0 %** — S1 and S2 both REFUSED. **Ten of `sat-03`'s
fourteen movers were movers of the truncation instant**, which is `X-7` confirmed by removing the clock
instead of by reading durations. The file was not committed; the measurement is the product.

### §5 — `sat-06-qra-window`: refused by S1, and the pair of numbers is the finding

A different question (an identification pass, `identify`, against UNARMED An-26 contacts so that nobody
can be shot at all) built entirely on the repaired mechanics. It reaches S2 (9 movers) and S7 (**0 of
8**) and fails S1 at 64.0 %. The reason is exact, and it is the same cell measured twice with one aim
point moved 11 m:

| aim point | S1 | movers | S7 |
|---|---|---:|:--:|
| ON the lattice edge | ok, modal 60.0 % | 10 | **1 of 8** |
| at the lattice MIDPOINT | **NO**, modal 64.0 % | 9 | **0 of 8** |

**The lever that separated the tenth class WAS the coin.** On a cell whose finest lever is the release
lattice, S1 and S7 are the same measurement with opposite signs. The file is committed in the S7-clean
form, with a header that forbids reading anything off it, because a criterion is not something one buys
back. `zone corridor` was added to buy the missing mover out of the station geometry and did not buy it
(the QRA breaks formation before it reaches the cylinder) — kept, because a device that does nothing is
worth measuring too.

### §6 — Where the arena stands, as the gate's own output

`tools/fb_campaign_arena.py --cells build/e17/cells-e17.txt --levers tools/levers-campaign-g5.txt
--chaos-screen`, artefact `sim/build/e17/sat-arena.log` + `sat-arena-channels.csv`:

```
cell                       module  distinct   modal modal cls   movers  S1 S2
sat-01-belt-channels       f16           10  52.0% (30, 23)        12   ok ok
sat-02-picture-split       f16            7  60.0% (14, 10)        10   ok ok
sat-03-escort-shield       f16           10  44.0% (26, 21)        14   ok ok
sat-04-vul-window          f16            9  52.0% (27, 18)        12   ok ok
sat-06-qra-window          f16            7  64.0% (14, 14)         9   NO ok

S7 CHAOS SCREEN
   sat-01-belt-channels       f16     1 of 8 flipped   NO
   sat-02-picture-split       f16     0 of 8 flipped   ok
   sat-03-escort-shield       f16     5 of 8 flipped   NO
   sat-04-vul-window          f16     0 of 8 flipped   ok

S5 yield       : 2 informative          (>= 3) NO   [sat-02-picture-split, sat-04-vul-window]
S6 distinctness: 0 identical pair(s)                  ok
ARENA: REFUSED   (5 cells, 2 informative)
```

`sat-06` never reaches the chaos screen: it is skipped because S1 refused it, and its 0-of-8 is the
separate probe in §5.

#### The number the next round needs

| | cells | ceiling `2^-n` |
|---|---:|---:|
| before | `sat-02` | 0.500 |
| **after** | `sat-02`, `sat-04` | **0.250** |
| needed for p ≤ 0.05 | 5 | 0.031 |

**Three cells short.** And this round says what they may NOT be built on: not `protect`/`survive` after
an exchange (§4b), not a delivery on a lethal radius (§4a), not a dwell budget read at the end of a run
(§1). What is left, and all three are measured to work: a declared cylinder sized to the formation's own
station arithmetic, a geometric identification box far from its own hold threshold, and a release GATE
(`ccip-tight` refuses the release outright, which is a structural move and not a lattice one).

### §7 — Corrections owed to committed files

- **`E16` §6(b) is REFUTED.** It measured `emcon-tight`/`emcon-mid` as the identity map and read
  `pilot_emcon_frac` as a three-valued STEP. On `sat-04`, post-`X-6`, **all three emcon alleles move the
  outcome class** (28,20 / 28,20 / 24,21 against a (27,18) baseline). The step was a measurement of the
  latch, exactly as `X-6`'s own note predicted it might be.
- **The committed `sat-01`/`sat-02`/`sat-03` header tables are pre-`X-6`** and their numbers are void;
  §0 above is the re-measurement. The files are left untouched so that `E16`'s numbers stay reproducible
  against the binary that produced them.

### §8 — The cost

| | |
|---|---|
| runs | ≈ **1 400** — 99 (three rigs re-flown with S7) + 4 × 34 (sat-04 iterations) + 3 × 34 (sat-05/06) + 45 (S7 probes) + 2 × 288 (conservation) + 2 × 288 (determinism) |
| what moved in `sim/src` | `core/FBObjective.h`, `core/FBMissionMonitor.{h,cpp}`, `core/FBMissionFile.cpp` — one field, one latch, one call, one predicate |
| what moved elsewhere | two new rigs (`sat-04`, `sat-06`), one negative fixture, and one word in `tools/fb_campaign_arena.py`'s hygiene guard (`git status -uno`: an UNTRACKED file in `sim/missions` cannot have MOVED a committed mission, and the guard's docstring never meant it) |
| `sim/vendor`, `sim/assets/aircraft` | untouched; `verify-models` green before and after |

---

## State — round `E18` (2026-08-04): the arena PASSES with five chaos-clean cells, and the sweep over them finds no admissible shift

`E17` left the count at **two** chaos-clean gradable cells and a ceiling of `p = 0.250`, with three
constraints on what the missing three could rest on. This round builds them, the gate accepts them,
and the sweep that the ceiling was wanted for comes back **negative** — which is the round's product
just as much as the cells are.

### §0 — The structural device the three new cells are built on: THE DRY ENGAGEMENT

`E17` §4b said `protect`/`survive` are gradable only after the exchange and the exchange is the chaos.
The escape is not to remove the exchange but to remove its LETHALITY: both sides carry
`brief_master_arm sim`, the hardware refuses every launch, and sorting, locking, cranking, the RWR
answer and the emission discipline all run unchanged. [MESS, `sat-07`] **63 `sms RELEASE_REJECTED
reason=hardware_precedence`, 0 `damage KILL`, 0 `monitor KO`** in the baseline.

**The consequence is a measured number, not an argument.** The chaos amplitude of a graded dwell over
the same ±3 m spawn grid:

| rig | graded quantity | chaos amplitude |
|---|---|---:|
| `sat-03` (wet) | `ee3`'s belt dwell at a fixed instant | 1.0 s |
| `sat-04` (wet, windowed) | lane dwell | 1.0 s |
| **`sat-07`/`sat-09` (dry)** | cylinder dwell | **0.10 … 0.30 s** |
| **`sat-08` (dry)** | planar min-range to a named contact | **1.45 … 27.7 m** |
| a `task formation` transit, no opposition | member's own track | **0.1 m / 0.025 s** |

The discrete event — a missile arriving — is the amplifier. Remove it and a 3 m perturbation stays a
3 m perturbation.

### §1 — The three cells, each on one of `E17` §6's load-bearing devices

| cell | primary graded quantity | narrowest margin | S1 | S2 | S7 |
|---|---|---|:--:|:--:|:--:|
| `sat-07-dry-merge` | a declared cylinder, threshold ladder in measured gaps | **3.20 s against 0.30 s = 11×** | ok, 60.0 % | ok, 10 | **0 of 8** |
| `sat-08-ident-qra` | a geometric `identify` box, 42 rungs | **56.0 m against 5.58 m = 10×** | ok, 60.0 % | ok, 10 | **0 of 8** |
| `sat-09-gate-strike` | the release GATE (`ccip-tight`: 4 releases → 0) | **21.3 m against a 45 m radius**; cylinders 1.45 s against 0.10 s = 15× | ok, 52.0 % | ok, 12 | **0 of 8** |

Two design rules came out of building them and both are instrument findings rather than tactics:

**(a) The outcome class is a SUM, so a lever that changes WHICH objective is met without changing HOW
MANY is invisible to it.** Every rig here therefore grades ONE continuous quantity through a MONOTONE
RUNG LADDER — several `exposure`/`range` thresholds on the same zone or pair — which turns that
quantity into a count the class can see. [MESS] `sat-08`'s `qa4 → an3` ladder alone resolves **nine
distinct alleles**, one per rung step, from 1 706 m (`emcon-tight`) to 13 385 m (`shape-tight`).

**(b) Every rung is placed at the midpoint of a MEASURED gap and emitted only where the lever gap is
≥ 10× the MEASURED chaos span of that same quantity** — derived by tool from the cell's own 25-lever
sweep and its own 9-sample chaos grid, not by hand. The rule is what refused `sat-08`'s obvious axis:
the same eight identify pairs graded by DWELL have a spectrum whose widest usable gap is 1.1 s against
a 0.20 s chaos amplitude (**5×**), while the same pairs graded by MIN-RANGE reach 10…3 180×. The
natural-looking axis was the coin.

### §2 — `E17` §4a satisfied rather than evaded, and the correction that took two attempts

The four rigs that deliver a Mk 82 place their aim points at the **fixed point of their own release
lattice**. That is NOT the midpoint of the printed impacts, and the first attempt got it wrong twice
over:

1. `stores IMPACT … lat=/lon=` is printed with `%g`, i.e. **6 significant digits = 9.3 m of
   quantisation** on a longitude near 44°. A lattice derived from it is coarse by a factor two. The
   full-resolution channel is `damage DAMAGE … bodyFwdM=/bodyRightM=`, the impact in the target's own
   body frame.
2. The pilot's CCIP solution is computed against the **designated target**, so moving the target moves
   the impact with a gain below 1. A one-shot placement does not centre it; it has to be ITERATED.

Iterated to convergence (2 iterations, 3 runs each): [MESS] the baseline impact lands **0.0 m** from
the target and the two bias rails at **±23.7 m** against the Mk 82's 45 m fail radius — **21.3 m of
margin on both sides**, and `bias-early`/`bias-late` are INERT on all three new cells. Before the
correction `bias-early` was a mover on all three with a **3.0 m** margin (48.05 m against 45 m), i.e.
exactly the coin §4a names, and the S7 screen did NOT catch it — it passed 0 of 8 anyway. **A criterion
that a defect passes is not a criterion for that defect**, and the margin, not the screen, is what
excluded it.

### §3 — The gate, as its own output

`tools/fb_campaign_arena.py --cells tools/cells-sat.txt --levers tools/levers-campaign-g5.txt
--chaos-screen`, artefact `sim/build/e18/arena.log` + `e18-channels.csv`:

```
cell                       module  distinct   modal modal cls   movers  S1 S2      S7
sat-01-belt-channels       f16           10  52.0% (30, 23)        12   ok ok   1 of 8  NO
sat-02-picture-split       f16            7  60.0% (14, 10)        10   ok ok   0 of 8  ok
sat-03-escort-shield       f16           10  44.0% (26, 21)        14   ok ok   5 of 8  NO
sat-04-vul-window          f16            9  52.0% (27, 18)        12   ok ok   0 of 8  ok
sat-06-qra-window          f16            7  64.0% (14, 14)         9   NO ok      —
sat-07-dry-merge           f16            9  60.0% (17, 17)        10   ok ok   0 of 8  ok
sat-08-ident-qra           f16           10  60.0% (14, 27)        10   ok ok   0 of 8  ok
sat-09-gate-strike         f16            9  52.0% (22, 23)        12   ok ok   0 of 8  ok

S4 size: 8 (>= 6) ok   S5 yield: 5 informative (>= 3) ok   S6: 0 identical pair(s) ok
ARENA: PASSED   (8 cells, 5 informative)
```

| | cells | ceiling `2^-n` |
|---|---:|---:|
| `E17` | 2 | 0.250 |
| **`E18`** | **5** | **0.031** |

**Two of the five sit at 60.0 % exactly, which is the S1 bound with no slack, and that is stated rather
than smoothed.** It is not a coincidence and it is not tunable: `tools/levers-campaign-g5.txt` contains
**14 levers that are the identity map on any F-16 side** (`cover` ×3 and `energy` ×3 by airframe and
task, `ccip-open`, `net-on`, `sort-left`, `sort-near` beside a live net, plus `bias-early`/`bias-late`
once §2's correction is applied and `emcon-wide`/`shape-trail` where they are the seed). Baseline plus
14 is **60.0 % of a 25-run population**, so no cell built against this file can go below the bound
without an eleventh mover. `E-26` books the denominator problem; this is its other face.

### §4 — The sweep over the five, and it is NEGATIVE

The instrument the ceiling was wanted for, run: each of the 24 levers as a one-gene displacement from
the seed, compared against the seed on each of the five cells by the published order, one-sided sign
test over the non-tied cells. Two readings, because they disagree and only one of them may be
published.

**At the OUTCOME CLASS (V, then M) — the level `§6` permits:**

| lever | cells (02/04/07/08/09) | W | L | p | counter-probe |
|---|---|---:|---:|---:|---|
| `bias-rail` (`pilot_attack_bias_s` = 10) | `-----` | 0 | 5 | **0.031 worse** | none — it IS the rail |
| `ccip-tight` (`pilot_attack_ccip_m` = 1) | `-----` | 0 | 5 | **0.031 worse** | `ccip-open` W0 L0 T5 |
| `emcon-mid` (`pilot_emcon_frac` 1.0 → 0.4) | `-++++` | 4 | 1 | 0.188 better | `emcon-tight` W3 L2, `emcon-wide` W1 L2 |
| every other lever | — | ≤ 3 | ≤ 4 | ≥ 0.188 | — |

**No lever is better on 5 of 5. The two that reach the ceiling are both WORSE, and both sit on a RAIL
of their own band** — `§6` §2: *"A gene sitting on its rail is not a finding, it is an unbounded
band."* `pilot_attack_ccip_m` = 1 is the LOWER RAIL of its declared 1…2 000 band and `bias_s` = 10 the
upper rail of −10…10. Neither is publishable and neither is interesting: refusing every release, or
throwing the bomb 2.3 km long, loses the kill bits by definition.

**The strongest positive is `emcon-mid` at 4 of 5, p = 0.188, and it FAILS on its own counter-probe
rather than on its p-value.** The advantage is not monotone in the gene: 1.0 → 0.4 wins 4 of 5, but
1.0 → 0.1 wins 3 of 5 and 1.0 → 3.0 wins 1 of 5. A direction that reverses on both sides of the point
that won is a point, not a direction.

**The other reading, and it is the round's sharpest warning.** In the FULL published order — V, then M,
then C by domination — three levers reach p = 0.031 and one of them is *not* on a rail: `bias-late`
(`bias_s` = +0.1) is worse on **5 of 5**. It is entirely a level-C effect: at the class level it is
`-....`, W0 L1 T4. Its whole "significance" is `aim` quality, the 23.7 m the aim point moves, and `§6`
lists *"a rank change inside level C"* first among the things that are expressly not a finding. **A
round that read the wrong column would have published a doctrine shift at p = 0.031 this afternoon.**

### §5 — The mechanism of the near-miss, named because it is worth naming

`emcon-mid`'s four wins carry one chain across all four, in published channels ([MESS],
`e18-channels.csv`):

| cell | `flt_switch` | `sort_assign` | `flt_assign` | class |
|---|---|---|---|---|
| `sat-04` | 110 → **29** | 316 → **69** | 12 → **22** | (27,18) → (28,20) |
| `sat-07` | 41 → **10** | 113 → **28** | 25 → **31** | (17,17) → (17,18) |
| `sat-08` | — | 20 → 17 | 10 → **17** | (14,27) → (14,34) |
| `sat-09` | 69 → **11** | 111 → **28** | 26 → 22 | (22,23) → (24,26) |

A quieter radar produces fewer own echoes, the flight stops re-sorting every tick, and the assignment
STICKS. That is X3-shaped. It is reported here and **not** as §1 of a shift report, because `§6`'s
binding rule works the other way round: a nameable mechanism does not buy a p-value.

### §6 — The X4 audit of the candidate, which PASSES and does not help

`tools/fb_champion_audit.py --cells build/e18/cells-informative.txt --genome pilot_emcon_frac=0.4`:

| detector | result |
|---|---|
| **X4a** a lucky trajectory — the CANDIDATE's own genome over the 0.8 m grid, 8 samples | **0 of 8 flipped on all five cells** (base classes (14,9), (28,20), (17,18), (14,34), (24,26)) |
| **X4b** judge evasion — every cell at `timeout` × 1.5 | **held on all five**, class unchanged |
| **X4c** a partition exploit — which counts moved | §5's table; `dmg_hits` does not appear, `dmg_effective` does not move, `eng_shots` moves on one cell (1 → 3) with `eng_shot_s` |
| **X1** arena invariance | it IS the sweep above: 4 of the 5 informative cells, which passes X1's "≥ 2 of the others" as a screen and fails the sign test as a verdict |
| **X2** | n/a, as always on this arena — both airframes are FlightBox's read-only model copies (principle 1), which carry no declared-ignorance band |

The audit is clean and the result is still not publishable. **That is the point of having the audit
separate from the significance test**: robustness and significance are different questions, and this
round is the first in the tree where a candidate passes the first and fails the second.

### §7 — What the round exploited, and every item is an instrument defect

`§6` §8 wants this section to weigh as much as §1. This round ran no evolution, so the list is what the
BUILDER hit — but every one of them is a lever a search would have found first:

| # | The hole | The channel it rides | Who owns it |
|---|---|---|---|
| **X-9** | **`stores IMPACT lat=/lon=` is `%g`-quantised to 9.3 m.** A lattice derived from the log is coarse by a factor two, and the aim-point placement built on it left a 3.0 m margin where it claimed 21.7 m | `events.log`, `stores IMPACT` | [`weapons.md`](weapons.md) — the full-resolution channel `damage DAMAGE … bodyFwdM/bodyRightM` exists beside it |
| **X-10** | **The CCIP impact follows the DESIGNATED TARGET with a gain below 1.** Any procedure that "puts the target where the bomb lands" is a fixed-point iteration and silently wrong as a one-shot | `pilot ATTACK_RELEASE`, `stores DELIVERY` | [`pilot.md`](pilot.md) |
| **X-13** | **The outcome class is a SUM, so an instrument can be blind to a lever that moves everything it measures.** Four of this round's drafts had 10 live levers and 2 outcome classes | `fb_fitness.side_key` | this file |
| **X-11** | **A level-C-only effect reaches p = 0.031 and looks exactly like a doctrine shift** (`bias-late`, §4) | `C_aim` in the channels file | this file — `§6` already forbids it, nothing enforces it |
| **X-12** | **S7 passes a cell whose bias lever has a 3 m margin.** The chaos screen samples 8 points of ±3 m of SPAWN; it does not see a threshold the GENOME sits 3 m from | `fb_campaign_arena.chaos_flips` | this file |
| **X-14** | **`emcon-wide` is the identity map wherever the seed already radiates continuously** — with a live net and no threat the far rail IS the seed. Same class as `E16` §6(b), now on `sat-08` | `flt_src`, `sort_assign` | [`duels.md`](duels.md) D3 |
| **X-15** | **A genome value applied to a module that cannot express the gene is ACCEPTED, silently** | [MESS, `E19`] `set pilot_emcon_frac 0.4` on a MiG-29 unit returns `true` — `FBMig29Module::Set` forwards every `pilot_*` key whole and `FBPilot::ApplyTuning` validates against `FBPilotTuning`'s table, which is the PILOT's and knows nothing about the module's hooks. The spliced mission flies and is **bit-identical** to the unspliced one over the whole telemetry. In a co-evolution one entire side of the search then reports success while doing nothing | no channel at all, which IS the defect: a refused `set` publishes `SET_REJECTED`, an unreachable one publishes nothing | [`pilot.md`](pilot.md) §9 — the alphabet is read from the binary so a genome cannot drift, and it still cannot say which MODULE carries which gene | **passes as a mechanism**, and it is the enabling defect for `E-29` |
| **X-16** | **Two of the genome's gene families are SINGLE-MODULE genes, and four rounds of doctrine were graded on them** | [MESS, `E19`] **G5:** `SilentRadarModeOrdinal()`/`EmconRadiateNm()` default to −1/0.0 in `FBPilot.h:310–311` and the whole EMCON block is gated on both being positive (`FBPilot.cpp:1540`); the only overrides in the tree are `FBF16Pilot.h:71–72`. **G1:** `FBFlightPicture::BuildMembers` adds SELF and returns unless `state.Datalink.H.Readable()`, and the MiG-29 has no datalink block — `flt_mates` max over the run is **3 on every F-16 and 0 on every MiG-29** although each declares `flight ia 1…4` and reports `flt_pos` 1…4. With no mates there is no lead member, `FormationCommands` never runs and `FormationStation` has no call site: `shape-tight`/`shape-wide`/`shape-stacked` leave the MiG separations at **1394 / 3537 / 4113 m**, the baseline's numbers to the metre | `flt_mates`, `flt_src`; and the absent EMCON block | [`duels.md`](duels.md) D3c, [`formation.md`](formation.md) F5 — and this file, whose §2.2 `static_assert` checks that a gene is DIMENSIONLESS and never that it is REACHABLE | **passes as a mechanism**; it bounds every doctrine claim this file has made about G1 and G5 to one airframe |
| **X-19** | **The genome's own TEXT LINE omits a bit the splice writes, so an archived genome is NOT re-flyable** | [MESS, `E19`] `fb_evolve.Genome.line()` emits `dl=` only when it differs from `"off"`, but `"off"` is TRUTHY, so `arena.splice_mission` injects `set datalink off` **and drops the mission's own `set datalink on`**. Every blue genome of the co-evolution therefore flies with the net off while printing `b0_seed pilot_emcon_frac=1.5`. §3.2 asks of the archive exactly one property — *"the genome, verbatim and re-flyable"* — and it does not hold: the same hole is in every archive `fb_campaign_evolve.py` has ever written | `archive-*.txt`; `Genome.line()` against `splice_mission` | this file §3.2 and [`pilot.md`](pilot.md) §9 — a genome is a LINE, so the line has to be the whole genome | **FAILED as a record.** Not repaired in the round that measured it; the `E19` population is internally consistent (every member carries the same hidden bit) so its comparisons stand, and its ABSOLUTE reading against the committed blue does not |
| **X-20** | **An F-16 that ends an EMCON spell re-acquires INSTANTLY — the scan raster is not resynchronised when a non-radiating MODE starts radiating again** | [MESS, `E20`, `sat-02-picture-split`, `pb2`] the jet goes quiet at t = 65.6 holding 8 firm contacts (dropped, `radar standby`) and comes back at t = 96.8 with **8 firm contacts in the SAME 0.1 s tick**, after 31.2 s of silence. Mechanism: `FBRadarSystem::Run` advances `NextScanS_` only while radiating, so the catch-up guard replays up to **64 frames in one tick**, each against the CURRENT geometry, and `kHitsToFirm` = 2 is met immediately. **The MiG-29 in the same run does not do it** — `pmia*`/`pmib*` return from DUMMY with **0 contacts** and build up normally, because `FBMig29Radar::SetEmission(Illum)` calls `ResyncScan()`, a line added with its own measurement (*"contact at t = 27.9 instead of the documented 2 × 3.0 s"*). One airframe carries the fix, the other does not, and the difference is which AXIS its EMCON runs on | `fcr_on` against `fcr_contacts`, one tick apart | [`pilot.md`](pilot.md) §7.6b — the same invariant class: an emission state must cost what leaving it costs | **passes as a mechanism, NOT REPAIRED HERE.** `SetPowered` already owns the pattern (`if (on && !Powered_) Resync_ = true`); the fix is the same condition on the ACTIVE-volume edge, and it moves every F-16 EMCON mission — a second behaviour change with its own regression, which is why it is booked instead of bundled |
| **X-17** | **S7 is measured against ONE opponent and reported as a property of the cell** | [MESS, `E19`, 200 runs] the same ±3 m spawn grid on which `E18` certified all five cells **0 of 8** gives, per (cell, red allele): `sat-04`/`red-near` **3 of 8**, `sat-08`/`red-right` **3 of 8**, `sat-08`/`red-none` **3 of 8**, `sat-04`/`red-none` 1 of 8 — **4 of 25 pairs dirty, and every dirty one carries a non-committed opponent** | `fb_campaign_arena.chaos_flips`, which splices exactly one side | this file — the same shape as `X-12`: a screen for one failure mode read as a screen for another | **FAILED as a criterion** for any use outside the committed opponent. The four pairs were dropped from `E19` §5 rather than argued about |

### §8 — What is NOT claimed

- **The five cells are not fully independent.** All three new ones contain a dry air element, because
  `pilot_emcon_frac` is inert without an engagement (`EmconSilent_` needs `other` — a mate's
  `Engaging` report or a net report) and no cell reaches ten movers without the emcon family. A sign
  test over five cells that share one mechanism overstates its own `n`. The honest reading of the
  0.031 ceiling is **"attainable", not "earned"**, and §4's negative result does not depend on it.
- **No doctrine shift is published**, so `§6`'s template is not filled in.
- The gene bands, the fitness, the genome and `kModalMax`/`kMoversMin`/`kChaosMaxFlips` were **not
  touched** in the round whose verdict they decide.

### §9 — The cost

| | |
|---|---|
| runs | ≈ **1 500** — 8 draft sweeps × 25, 6 chaos grids × 9, 3 re-ladder passes × 34, 4 arena passes over 1–8 cells, 12 centring runs, 45 audit runs |
| what moved in `sim/src` | **nothing** |
| what moved elsewhere | three new rigs (`sat-07`, `sat-08`, `sat-09`), `tools/cells-sat.txt` |
| `sim/vendor`, `sim/assets/aircraft` | untouched |

---

## State — round `E19` (2026-08-04): the first CO-EVOLUTION, and the opponent can carry one gene of nine

`E13`…`E18` each flew ONE side against a FIXED opponent. This round runs the other thing §3 specifies —
both sides carrying a genome — and the first measurement it made **voids the premise it was started
with**: `tools/fb_campaign_evolve.py` was never a co-evolution and could not be made into one by
switching on its `--archive`, because its own docstring is exact — *"the opponent is committed mission
text; it cannot answer."* The archive it keeps is an archive of one population measured against a world
that never moves. A second tool was needed and is built: **`sim/tools/fb_campaign_coevolve.py`**, one run
producing BOTH side keys, plus `tools/duels-sat.txt` (the five `E18` cells with both sides declared).

**The round's product is a negative with a mechanism**, and the mechanism is the reason every earlier
round's near-miss was a near-miss.

### §0 — The instrument, cross-checked against the committed measurement before anything else was believed

Two-sided splice, both keys off one telemetry set. First output, before any lever:

| cell | blue key, `E19` two-sided rig | blue class, `E18` §3 table |
|---|---|---|
| `sat-02` | V=14 M=10 C=+758.9/+42.5 | (14, 10) |
| `sat-04` | V=27 M=18 C=+320.7/+86.2 | (27, 18) |
| `sat-07` | V=17 M=17 C=+132.2/+36.9 | (17, 17) |
| `sat-08` | V=14 M=27 C=+135.4/+36.9 | (14, 27) |
| `sat-09` | V=22 M=23 C=+36.1/+80.9 | (22, 23) |

**5 of 5 identical.** The new reader reproduces the committed arena exactly; everything below is measured
on an instrument that was checked against a number it did not produce.

### §1 — The red genome, measured at the TRAJECTORY level: one gene of nine reaches the MiG-29

The class-level probe (`build/e19/probe-red.log`, 125 runs — 24 levers × 5 cells, red carries the lever,
blue is the committed text) says **1 of 24 levers moves anything.** A class-level zero cannot tell
"inert" from "invisible to a sum" (X-13), so it was re-asked at the only level that can: **SHA-256 over
the side's whole telemetry**, 75 runs over `sat-02`, `sat-04`, `sat-07`.

| red lever family | bit-level result on all three cells |
|---|---|
| `cover-*` (G2), `energy-*` (G4), `net-*` + `sort-left` (G3), `bias-*` (G6), `ccip-*` (G7), `shape-*` (G1), `emcon-*` (G5) — **23 levers** | **bit-identical to the baseline**, red trajectory AND blue trajectory |
| `sort-near` (G3) | MOVED / MOVED on all three |

Widened to the whole pilot alphabet (52 genomes × 2 cells, 104 runs), the split is exact and it is not
a coincidence of the lever file:

| | keys | which |
|---|---:|---|
| move the MiG's trajectory | **12 of 24** | `abort_nm`, `action_s`, `beam_deg`, `chaff_s`, `crank_deg`, `defend_hold_s`, `lock_nm`, `react_s`, `shot_ata_deg`, `shot_rtr`, `shot_spacing_s`, `speed_kt` — **every one of them `Free`, and not one of them in the genome** |
| bit-silent on the MiG | **12 of 24** | includes **8 of the genome's 9 genes**: `energy_frac`, `cover_frac`, `attack_bias_s`, `attack_ccip_m`, `flight_spread/trail/stack_frac`, `emcon_frac` |
| the sort contract | 4 non-seed alleles | `near`, `right`, `far`, `none` all move both sides; `left` **is** the committed brief on all five rigs and is the identity map (X-14's class) |

**The genome is F-16-shaped.** It was assembled gene by gene from F-16 measurements, and on the only
other fighter in the tree it consists of eight silent keys plus a sort contract.

### §2 — Why, in source lines and in a published channel — two independent module boundaries

**G5 (emission) is structurally an F-16-only gene.** `FBPilot.h:310–311` defaults
`SilentRadarModeOrdinal()` to −1 and `EmconRadiateNm()` to 0.0, and the whole EMCON block is gated on
`SilentRadarModeOrdinal() >= 0 && EmconRadiateNm() > 0.0` (`FBPilot.cpp:1540`). Overrides exist in
**exactly one file in the tree** — `FBF16Pilot.h:71–72` (0 and 40.0). `FBMig29Pilot` and `FBAirPilot`
override neither. The header says so itself: *"ein Modul, das nichts überschreibt, verhält sich Bit für
Bit wie vor `duels.md` D3c."* The code is not wrong; what was never written down is that **the gate, the
fitness and four rounds of doctrine have been grading a gene only one of the three flying modules can
carry.**

**G1 (formation shape) dies one layer lower, and the channel says it.** [MESS, `sat-07` and `sat-02`]
`flt_mates` — the MAX over the whole run — is **3 on every F-16** and **0 on every MiG-29**, although
every MiG declares `flight ia 1…4` and reports `flt_pos` 1…4. `FBFlightPicture::BuildMembers` adds SELF
and then `if (!dl.H.Readable()) return;` — the MiG-29 module has no `datalink` key and no datalink block
at all, so `MemberCount_` never exceeds 1. `FormationCommands` needs `searching && Declared && !IsLead`
**and a non-self lead member**; with no mates there is no lead, `FormationStation` is never called, and
the three shape genes have **no call site**. Measured against that prediction: `shape-tight`,
`shape-wide`, `shape-stacked` on red give MiG separations of **1394 / 3537 / 4113 m from the lead at a
fixed row — the same three numbers to the metre as the baseline**, and the telemetry hashes agree.

G2 (`weapon ttaS`), G4 (`Phase::Bfm`, no unit briefs `task bfm`) and G6/G7 (the MiG carries no bombs)
are inert for reasons already booked in `E-6`, `85c1a74` and this file.

### §3 — Red's own key has a gradient on ONE of the five cells, and that is the hard ceiling on red's evolvability

| cell | red's own class over its 5 alleles | red's best | blue's class over the same 5 |
|---|---|---|---|
| `sat-02` | (14,6) (14,6) **(16,8)** (15,7) **(16,8)** | `right`/`none` | (14,10) (13,9) (13,7) (14,9) (14,8) |
| `sat-04` | (8,4) (7,3) (7,3) (8,4) (8,4) | `left` = the seed | (27,18) (27,19) (27,19) (28,19) (27,18) |
| `sat-07` | (24,8) × 5 — **constant** | — | (17,17) (17,16) (18,21) (18,21) (17,20) |
| `sat-08` | (12,4) × 5 — **constant** | — | (14,27) (14,25) (14,30) (14,28) (14,38) |
| `sat-09` | (12,4) × 5 — **constant** | — | (22,23) (23,25) (24,26) (22,19) (22,21) |

**The three DRY cells `E18` built to remove chaos also removed red's fitness.** Their MiGs declare one
objective each — `survive until <s>` — and nothing can shoot them, so red sits at its own ceiling on
every allele. A search cannot climb a constant: on three of five cells red's own key carries no
information about red's doctrine at all.

**Blue's baseline, by contrast, moves on all five** — up to (14,27) → (14,38) on `sat-08`. The arena is
strongly opponent-dependent even though the opponent can only choose a sort contract.

### §4 — S7 is a property of the (cell, OPPONENT) pair, not of the cell — and 4 of 25 pairs are dirty

`E18` certified all five cells **0 of 8** on the ±3 m spawn grid. That was measured against the committed
red only. Re-asked for every (cell, red allele) pair — 200 runs, `build/e19/sweep.log`:

```
cell                       red-left      red-near      red-right     red-far       red-none
sat-02-picture-split       0 of 8 ok     0 of 8 ok     0 of 8 ok     0 of 8 ok     0 of 8 ok
sat-04-vul-window          0 of 8 ok     3 of 8 NO     0 of 8 ok     0 of 8 ok     1 of 8 NO
sat-07-dry-merge           0 of 8 ok     0 of 8 ok     0 of 8 ok     0 of 8 ok     0 of 8 ok
sat-08-ident-qra           0 of 8 ok     0 of 8 ok     3 of 8 NO     0 of 8 ok     3 of 8 NO
sat-09-gate-strike         0 of 8 ok     0 of 8 ok     0 of 8 ok     0 of 8 ok     0 of 8 ok
chaos-clean (cell, red) pairs: 21 of 25
```

**Every dirty pair carries a non-committed red.** The criterion as written screens ONE point of the
opponent space and reports the answer as a property of the cell. Filed as `X-17`; the four dirty pairs
are dropped from everything below rather than argued about.

### §5 — The sweep over the WHOLE opponent space, and it is NEGATIVE in a new way

`E18` §4's instrument, run once per red allele over that red's chaos-clean cells: 24 one-gene
displacements from the committed blue, one-sided sign test at the **outcome class (V, then M)** — X-11's
column, not `C_aim`. 625 runs, `build/e19/sweep/sweep.csv`.

| red allele | clean cells | its own best blue lever | p |
|---|---:|---|---:|
| `red-left` (the committed opponent — `E18`'s world) | 5 | `emcon-mid` 4 W / 1 L | **0.188** |
| `red-near` | 4 | `emcon-tight` 3 / 0 · `shape-abreast` 3 / 0 | 0.125 |
| `red-right` | 4 | `shape-tight` 2 / 0 | 0.250 |
| `red-far` | 5 | `net-off` 4 / 0 | **0.062** |
| `red-none` | 3 | `emcon-tight` 2 / 0 · `shape-trail` 2 / 0 | 0.250 |

**No lever reaches the 0.031 ceiling under ANY opponent.** `emcon-mid` reproduces `E18`'s 4/5 and
p = 0.188 exactly under `red-left` — a second instrument agreeing with a committed number.

**And this is the round's finding.** Sign per opponent, `L | N | R | F | O`:

| lever | signs | pooled over the 21 clean pairs | sign-stable? |
|---|---|---:|---|
| `emcon-mid` | **+ + − − =** (4/1, 2/1, 0/1, 1/2, 1/1) | 8 W / 6 L, p = 0.395 | **no** |
| `emcon-tight` | + + − + + | 10 / 4, p = 0.090 | **no** |
| `net-off` | = = = **+** − | 11 / 8, p = 0.324 | **no** |
| `shape-trail` | + − = + + | 8 / 2, p = **0.055** | **no** |
| `shape-abreast` | + + + − − | 10 / 7, p = 0.315 | **no** |
| `shape-tight` | − = + + + | 10 / 7, p = 0.315 | **no** |
| `bias-rail`, `ccip-tight` | − − − − − | 0 / 21 | yes — and both are RAILS (§6 §2) |
| `cover-*`, `energy-*`, `net-on`, `sort-*`, `ccip-open` | = = = = = | 0 / 0 | yes — identity maps (`E-26`) |

**Every lever that has a direction at all reverses it against some opponent. The only sign-stable
directions in the whole table are "always worse" and "always nothing".** `emcon-mid` — the candidate
`E18` came closest with — is better against exactly the one opponent `E18` measured against, and worse
against `red-right` and `red-far`.

**That is the explanation `E18` could not give itself.** Its p = 0.188 was not a weak signal that more
cells would have sharpened; it was a signal conditioned on one point of an opponent space that has at
least five, and the conditioning is worth more than the p-value.

### §6 — The co-evolution as `§3` specifies it, run

`tools/fb_campaign_coevolve.py --generations 3 --points 3 --archive-sample 3
--genes "blue:pilot_emcon_frac,pilot_flight_trail_frac;red:-"`, artefacts `build/e19/coevo.log`,
`archive-blue.txt`, `archive-red.txt`.

Red is **enumerated rather than searched**, and that is the honest form: its declared genome is one gene
whose alphabet has five members (§1), so its whole strategy space is smaller than any population that
would search it. §3.4 B's non-domination admission and §3.6's three instruments run unchanged on both
sides. **915 runs, 3 generations, blue 8 × red 6 per generation.**

**Both champions are fixed from generation 0 and never move again.**

| | generation 0 | 1 | 2 |
|---|---|---|---|
| blue champion | `b0_s1` (`sort=left`, and the hidden `dl=off` of `X-19`) | same | same |
| blue frozen-field score | 0.800 | 0.800 | 0.800 |
| red champion | `r0_s4` (`sort=none`) | same | same |
| red frozen-field score | 0.600 | 0.600 | 0.600 |
| archive size, blue / red | 0 / 0 | 1 / 1 | 1 / 1 |

**The three instruments of §3.6, as the runner prints them:** (a) non-decreasing on both sides — and
FLAT, so it reports "not circling" for the trivial reason that nothing moved; (b) **not computable, 1
distinct champion per side** where the statistic needs 3; (c) not computable, needs four generations.
**The archive admitted exactly one genome per side**, because §3.4 B's behaviour vector collapses: on
the five cells the population produces two distinct vectors and one dominates.

**Blue's numeric gene is flat and the whole separation is the channel/sort bit.** `pilot_emcon_frac` at
0, 0.75, 1.5, 2.25 and 3 scores **identically** (co-evo 0.586, frozen 0.800) in all three generations
once `sort=left` is fixed; the only genome that scores differently is the one without it (0.381 /
0.500). That is `X-4`'s finding — *"the cooperative datalink costs an F-16, and NOT through the sort"* —
reproduced by a search that was not looking for it.

**The champion pair does move the world**, and it is reported because the table is cheap, not because it
is a shift:

| cell | committed / committed | champ / committed | committed / champ | champ / champ |
|---|---|---|---|---|
| `sat-02` | (14,10) \| (14,6) | (14,9) \| (15,7) | (14,8) \| (16,8) | (14,8) \| (16,8) |
| `sat-04` | (27,18) \| (8,4) | (27,19) \| (8,4) | (27,18) \| (8,4) | **(28,20)** \| (8,4) |
| `sat-07` | (17,17) \| (24,8) | (17,19) \| (24,8) | (17,20) \| (24,8) | (17,19) \| (24,8) |
| `sat-08` | (14,27) \| (12,4) | (14,35) \| (12,4) | (14,38) \| (12,4) | **(14,40)** \| (12,4) |
| `sat-09` | (22,23) \| (12,4) | (23,22) \| (12,4) | (22,21) \| (12,4) | (23,22) \| (12,4) |

**Nothing is published from it.** The blue champion's advantage rides the hidden `dl=off` of `X-19` —
i.e. it is `net-off`, the lever §5 measured as `= = = + −` across the opponents — and `sat-09` trades
V up for M down. Red's own three columns are unchanged on three of five cells, which is `E-30`.

**One tool failure is booked on this round's own account because it was expensive.** Four attempts died
with missing files. The first cause was banal — the disk stood at 99 %. The other three were mine:
`ThreadPoolExecutor.map` submits ALL jobs up front, so an instance that raised in one worker kept
running the remaining ~500 for another twenty minutes, and its `shutil.rmtree` deleted the directories of
the newly started instance — same deterministic tags, same paths. The tool now asserts tag uniqueness
per batch and reports `fb-gym`'s exit and the directory contents when a run produces no log; neither is
a fix for the overlap, which is an operating rule: **one instance at a time.**

### §7 — What the round exploited — every item is an instrument or a boundary defect

| # | The hole | The channel it rides | Who owns it |
|---|---|---|---|
| **X-15** | **A genome value applied to a module that cannot express the gene is ACCEPTED, silently.** `FBPilotTuning` is the pilot's table and knows nothing about a module's hooks, so `set pilot_emcon_frac 0.4` on a MiG-29 returns `true` from `ApplyTuning` and changes not one bit. In a co-evolution that is one whole side of the search doing nothing while reporting success | `FBMig29Module::Set` → `FBPilot::ApplyTuning`; no channel at all, which is the defect | [`pilot.md`](pilot.md) §9 — the alphabet is read from the binary precisely so a genome cannot drift, and it still cannot say which MODULE can carry which gene |
| **X-16** | **Two of the genome's gene families are single-module genes and nothing says so.** G5 needs `SilentRadarModeOrdinal`/`EmconRadiateNm`, overridden in **1 of 3** flying modules; G1 needs a datalink block for the flight to exist to its own members (`flt_mates` = 3 on the F-16, **0** on the MiG), overridden in 1 of 3. Four rounds of doctrine were graded on genes half the arena cannot carry | `flt_mates`, and the absent EMCON block | [`duels.md`](duels.md) D3c, [`formation.md`](formation.md) F5 — and this file, whose §2 boundary test checks that a gene is DIMENSIONLESS, never that it is REACHABLE |
| **X-17** | **S7 is measured against one opponent and reported as a property of the cell.** [MESS] 4 of 25 (cell, red) pairs flip 1…3 of 8 on the same ±3 m grid on which `E18` certified all five cells 0 of 8 — `sat-04`/`red-near` 3 of 8, `sat-08`/`red-right` 3 of 8, `sat-08`/`red-none` 3 of 8, `sat-04`/`red-none` 1 of 8 | `fb_campaign_arena.chaos_flips`, which splices only ONE side | this file — same shape as `X-12`: a screen for one failure mode read as a screen for another |
| **X-19** | **A genome's own text line omits a bit the splice writes, so no archive in this tree is re-flyable.** `Genome.line()` prints `dl=` only when it differs from `"off"` — and `"off"` is truthy, so the splice injects `set datalink off` and drops the mission's own `set datalink on`. Every blue genome of §6 flew with the net off while printing no channel bit at all | `archive-*.txt` against the spliced mission | this file §3.2 (*"verbatim and re-flyable"*), [`pilot.md`](pilot.md) §9 |
| **X-18** | **A dry rig removes the chaos by removing the opponent's fitness.** `E18`'s three DRY cells give red a single `survive` objective that nothing can threaten, so red's own class is CONSTANT over its whole strategy space on 3 of 5 cells. The device that bought the chaos-clean arena is the device that makes the arena one-sided | `mission OBJECTIVE` on the red units; `fb_fitness.side_key` | this file — §4's criteria are all stated about ONE side's spread |

The opponent's clock is the other trap and it was checked rather than assumed (`durationS` per pair):
`sat-02` runs 248.6 s under `red-left` and **520.0 s** under `red-none`, `sat-09` 460.0 s against
**260.1 s** under `red-right`. Every comparison in §5 is blue-lever against blue-committed **inside one
red allele**, so the red-induced clock shift is common to both sides of every comparison and X-7 does not
reach it.

### §8 — What is NOT claimed

- **No doctrine shift is published.** §6's template is not filled in, and §6's binding rule is the reason:
  the strongest candidate anywhere in the round is `shape-trail` pooled at p = 0.055 with a sign that
  reverses under `red-near`, which is `X-8`'s "a gene can act everywhere and carry no direction" one
  level up — now on the OPPONENT axis instead of its own band.
- **The pooled column is not an independent n.** The 21 (cell, red) pairs share five cells, so the pooled
  p is an ordering aid, not a significance statement. The per-opponent columns are the honest reading and
  none of them beats 0.062.
- **The opponent space is five points because the GENOME is one gene wide on the MiG, not because the
  MiG has five doctrines.** Twelve `Free` pilot keys move it (§1) and every one of them is outside the
  genome. Whether growing the genome opens this is `E-29` and was NOT tried here: §2 and the gene set may
  not move in the round whose verdict they decide.
- **§6's champion is not a doctrine claim** and is not offered as one: its blue side carries `X-19`'s
  hidden `dl=off`, so what the co-evolution found is `net-off` plus a briefed sort — a lever §5 measured
  as `= = = + −` across the five opponents. The run's value is the three §3.6 instruments and the
  archive, and both come back degenerate (1 champion per side, 1 archive member per side).
- Nothing in `sim/src/`, `sim/vendor/` or `sim/assets/aircraft/` was touched.

### §9 — The cost

| | |
|---|---|
| runs | **≈ 1 950** — 125 class probe, 75 bit probe, 104 alphabet probe, 5 cross-check, 200 chaos screen, 625 sweep (25 shared), **915 co-evolution**, 3 determinism |
| gates | determinism `--threads 1/2/4` on a spliced two-sided pair: identical key AND byte-identical telemetry (`sat-09`, blue `emcon 0.4 sort=left`, red `sort=none`, SHA-256 `929d49b2ea9a8a9a` three times) · `make -C sim verify-models` green · `git status --porcelain -uno sim/missions sim/assets/aircraft sim/assets/MODEL-DELTAS.md sim/vendor` empty before and after every run |
| what moved in `sim/src` | **nothing** |
| what moved elsewhere | `sim/tools/fb_campaign_coevolve.py`, `sim/tools/duels-sat.txt`, artefacts under `sim/build/e19/` |
| `sim/vendor`, `sim/assets/aircraft` | untouched |

---

## State — round `E20` (2026-08-04): eight silent genes were THREE different facts, and one of them was a missing seam

`E19` closed with E-29: *"the genome cannot be co-evolved because it is F-16-shaped"* — eight of nine
genes bit-still on the MiG-29. This round took that measurement apart. **It is not one fact. It is
three**, and only one of them was a defect.

### §0 — The instrument first, against the same committed number `E19` used

The bit-level probe of this round is new code, so it was pointed at `E18`'s published table before it
was believed — the identical check `E19` ran, on the identical five cells:

| cell | `E20` reader, blue key | `E18` §3 table |
|---|---|---|
| `sat-02` | V=14 M=10 | (14, 10) |
| `sat-04` | V=27 M=18 | (27, 18) |
| `sat-07` | V=17 M=17 | (17, 17) |
| `sat-08` | V=14 M=27 | (14, 27) |
| `sat-09` | V=22 M=23 | (22, 23) |

**5 of 5.** And the pre-repair baseline it then took reproduces `E19`'s own central number exactly:
**1 of 24 levers moves the MiG-29 at the bit level, and it is `sort-near`** (75 runs, `sat-02`/`sat-04`/
`sat-07`, SHA-256 over the whole side).

### §1 — The three causes, each with its own measurement

| gene | verdict | the measurement that decides it |
|---|---|---|
| **G5** `pilot_emcon_frac` | **A MISSING SEAM — repaired** | see §2 |
| **G4** `pilot_energy_frac` | **REACHABLE ALL ALONG; the RIG, not the airframe** | `duel-merge`, where a MiG-29 actually enters `Phase::Bfm`: `energy-low` / `-mid` / `-high` are **three distinct trajectories**, all different from the baseline. `E19` measured G4 only on `sat-*`, and `sat-07`'s own header states the reason it is dead there: *"it lives only inside `Phase::Bfm`, and no unit here flies `set task bfm`"* — which is equally true of the F-16 side |
| **G1** `pilot_flight_spread/trail/stack_frac` | **CORRECT ASYMMETRY, sourced** | [`formation.md`](formation.md) F6, answered this round. The Lazur-M is a ground command channel and not a shared picture; the alternative F6 left open (visual station keeping) cannot be built from this tree's eye — `FBVisualContact` withholds range structurally and identity always, and at the 1 852 m combat spread the lead's 17.32 m beam-on extent subtends **0.536°** against a **0.8°** recognition rung, so it does not even carry a TYPE |
| **G2** `pilot_cover_frac` | **CORRECT ASYMMETRY, same channel** | `flt_mate_bound` = 0 on every MiG; [`formation.md`](formation.md) F3 already books it as *"the MiG has no cover channel at all"* |
| **G6** `pilot_attack_bias_s` · **G7** `pilot_attack_ccip_m` | **CORRECT ASYMMETRY, sourced** | measured on `mig29-opt-low`, a MiG-29 flying a full air-to-ground `Phase::Attack`: `bias-early`/`-late`/`-rail` and `ccip-tight`/`-open` are **bit-identical, all five**. `FBMig29Pilot::DirectorPass` replaces the generic release pass, which never runs because this fire control publishes no release cue — *"an aircraft that has no release cue"*, `modules/mig29/weapons.md` §5.4.2. A CCIP cross-error tolerance has no referent on a jet with no CCIP, and a pickle lead has none on a jet where the AIRCRAFT releases |
| **G3** `sort` | live, unchanged | `sort-near` |

**So the MiG-29 can carry THREE of the nine genes** (G3, G4, G5) where `E19` counted one, and the other
six are asymmetries with a source. E-29's headline — *the genome is F-16-shaped* — survives in a weaker
and more useful form: two thirds of it are F-16-shaped **because the aeroplanes differ**, and the arena
must stop grading a side on those rather than the genome growing to cover them.

### §2 — G5 was a real gap, and repairing it took TWO seams rather than the two numbers `E19` asked for

`E19`/`D3d` named the owed repair as *"an N019 emission ordinal + search range on `FBMig29Pilot`"*.
**Both numbers were added and changed not one bit**, which is the finding:

1. **The switch was the wrong axis.** The EMCON actuation posted `RadarMode`. On this jet the mode
   selector already carries RAD-against-ACM (`SearchRadarModeOrdinal` / `BfmRadarModeOrdinal`), and the
   documented emission control is a switch of its own — PUR-31 ILLUM/DUMMY/OFF, `DCS-EA p.63`.
   `FBPilot::EmissionControl()` now names the switch; its DEFAULT is the old two ordinals, so the F-16's
   path is the same code.
2. **The picture was the wrong shape.** `EmconSilent_` required a cooperative report, i.e. as code the
   rule said *an airframe without a datalink may never be quiet* — the inverse of this aircraft's whole
   doctrine. `BriefedPictureRangeM()` lets the **controller's last call** be that picture, expiring at
   the controller's own briefed cadence.

The two numbers, each from this jet's own source: silent = **DUMMY** (*"does not radiate"*, and never
OFF, which drops the power `pilot.md` §7.6b proves he cannot come back through), radiate reach =
`FBMig29Radar::kSearchRangeM` = 50 km = **27.0 nm** (T4 §7.1) against the F-16's 40.0.

**Measured, bit level, MiG side, 3 cells:**

| | `sort-near` | `emcon-*` | everything else |
|---|---|---|---|
| before | MOVED 3/3 | bit-identical | bit-identical |
| after | MOVED 3/3 | **`emcon-wide` MOVED 3/3** | bit-identical |

**2 of 24, up from 1** — the acceptance bar, and it is met thinly for a reason worth stating rather than
smoothing: `emcon-tight` (0.1) and `emcon-mid` (0.4) are the identity map here because the committed
cells brief GCI ranges of 120 / 90 / 60 km and the seed's own gate is 50 km, so all three sample the
same side of every threshold. **That is X-14's class in mirror image**, and it is a property of the
LEVER FILE against these rigs, not of the gene. Asked at the band's own resolution (10 points, and the
rigs were NOT touched to make it come out):

| cell | distinct MiG trajectories over the 10-point band | how they group |
|---|---:|---|
| `sat-02` | **4** | `{base, 0.1, 0.4, 1.0} · {1.2, 1.5} · {1.8, 2.0} · {2.5, 3.0}` |
| `sat-04` | **6** | the same four groups, but `1.2 · 1.5 · 1.8 · 2.0` each separate |
| `sat-07` | **4** | as `sat-02` |

All three cells brief the SAME ladder — 120 / 90 / 60 km — so the three group boundaries sit exactly
where `f × 27.0 nm` crosses it: **f = 2.4 / 1.8 / 1.2**. That is the derivation reproduced from the
mission text without a fitted constant. `sat-04`'s two extra trajectories are the second term of the
same rule — `nearestM` also takes the minimum over the jet's OWN radar contacts, which move
continuously with `f` once it is radiating — and they are the reason that cell resolves the band
where the other two quantise it.

**And the conservation proof is the gene's own rail:** at `f ≥ 2.5`, i.e. "never be quiet", the repaired
MiG-29 is **byte-identical to the PRE-REPAIR binary** on all three cells (`0d8d78e666d7a8a9`,
`010f15d3c0b73011`, `5cd231b5129bce09`). Nothing was removed; the old behaviour is one brief away.

### §3 — What it cost the rest of the tree, and the membership test is EXACT

Two `fb_regress.sh`-equivalent snapshots over all **293** missions, one binary each, `--threads 1`:

| | |
|---|---|
| byte-identical | **237** |
| moved | **56** |
| exit code changed | **3** |

**A mission moves IFF it contains a MiG-29 flying `set task intercept` with at least one `brief_gci`
whose reported range exceeds the N019's own 50 km reach.** Predicted 56, moved 56, **0 either way** —
including `mig29-rwr-blind`, which carries a 60 km call and does NOT move because its MiG flies `Route`
and the emission decision is an Intercept-phase act. **No mission without a MiG-29 moved** (155 of them),
which is contract E21.

**The three exit codes, individually, each read on its own file's binding instrument** (all three files
state that the exit code is not their verdict):

| Mission | Exit | Blue (V, M) | Red (V, M) | What moved |
|---|---|---|---|---|
| `sat-09-gate-strike` | 3 → 0 | (22, 23) → **(24, 26)** | (12, 4) → (12, 4) | all eight F-16 conclude SUCCESS at t = 260.1 instead of six plus three TIMEOUTs at 460; `damage KILL` 4 → 4 and `monitor KO` 4 → 4 are unchanged, so this is windows closing, not a different fight |
| `ar-01-headon-noon` | 1 → 3 | (20, 12) → **(21, 13)** | (15, 7) → (15, 7) | the MiG that lost its `survive` bit at t = 417.4 keeps it; blue `sms LAUNCH` 25 → 13, `monitor KO` 9 → 6 |
| `ar-10-vertical-evening` | 3 → 1 | (20, 12) → (20, 12) | (16, 8) → **(15, 7)** | the mirror case — blue `sms LAUNCH` 12 → 33, `monitor KO` 5 → 14, one more MiG dies |

**The direction is consistent and it is NOT a buff:** a MiG that goes quiet on the controller's word
does the SAME or WORSE on these rigs, because quiet here means not building its own picture while the
F-16s keep theirs on the datalink. The repair makes the gene expressible; whether quiet is better is the
question the sweep exists to answer, and `f = 1.0` happens to be a quiet-heavy setting against briefed
ranges that all sit outside 50 km.

### §4 — X-19 repaired: an archive line is now provably re-flyable

`Genome.line()` printed `dl=` only when it differed from `"off"`, and `"off"` is truthy, so the splice
wrote `set datalink off` and dropped the mission's own line while the record showed nothing. Three
changes, and the third is what makes it checkable rather than asserted:

* `line()` prints the channel bit whenever it is non-empty;
* the constructor's default is `dl=""` (*"leave the mission's briefing alone"*, the rule
  `fb_campaign_arena.load_levers` already applied on the input side) — forcing the net off stays an
  ALLELE and is spelled out;
* `Genome.parse()` + `reflyable()`, and **every archive line is checked as it is written**.

[MESS] the three `dl` states now produce three distinct lines and three distinct spliced missions
(`a2e97d891045ff61` / `9b2ed9ca35a7ac99` / `df66f79746cb42b4`), each of which its own printed line
reproduces exactly. Before the repair the first two printed identically.

### §5 — The co-evolution, re-run: the opponent MOVES for the first time, and blue still does not

2 220 runs, 2 generations, both sides declared. **The genome was cut to the genes each side can
express on THIS arena, and the cut was declared before the run rather than derived from it** (contract
E20): blue carries the five the `sat-*` headers themselves measure live here — `emcon`, the three
shape ratios, `ccip` — and NOT `cover`/`energy`, which those headers state are inert on every one of
these cells for both sides; red carries `emcon` plus its sort alleles, which is §1's measured answer.
Two generations and not four is a **budget** cut and is named as one: instruments (b) and (c) need
three champions and four generations, so neither is computable here — as neither was in `E19`.

| | `E19` | `E20` |
|---|---|---|
| distinct RED champions | **1** (fixed from generation 0) | **2** — `r0_s4` (`emcon 1.5 sort=none`) → **`r1_02` (`emcon 3` sort=none)** |
| distinct BLUE champions | 1 | **1** — `b0_seed`, still fixed from generation 0 |
| archive members, red / blue | 1 / 1 | **2 / 3** |
| red's population spread | flat over its whole strategy space | `r0_00` (`emcon 0`) co-evo **0.536** against the seed's **0.484**; the sort alleles separate the frozen score **0.667 / 0.500** |
| fixed yardstick (a), red | flat | flat, 0.667 / 0.667 |

**The one-sentence result: red is no longer a fixed point, and the gene that moved it is the one this
round repaired** — its generation-1 champion differs from its generation-0 champion in
`pilot_emcon_frac` alone (1.5 → 3). **Blue is still fixed from generation 0**, so the degeneration is
REDUCED and not removed, and nothing about a doctrine is published from it (§6): the fixed yardstick is
flat on both sides and instrument (b) is still not computable.

**And the archives are re-flyable now, checked rather than claimed.** Every blue line carries the
`dl=off` that `E19`'s archives hid, `reflyable()` passed on all five as they were written, and each
line re-read reproduces its own spliced mission (`b0_seed` → `b74956ea2c54633a`, `r1_02` →
`dc52296ac87e4cb4`).

### §6 — One defect fell out of the repair, in the module that carries it

`FBRadarSystem::Powered_` defaults to `true`; `FBMig29Radar::Emit_` defaults to `OFF`; `SetEmission`
returns early on an unchanged state — so from spawn the block advertised a live set behind a dead
switch. It was invisible because **nothing in the tree read `Radar.Powered` except the EMCON branch**,
which this round is the first to reach on this airframe. The constructor now reconciles the two, and
without it the first measurement of the repair showed a MiG throwing DUMMY four seconds before its set
had any power.

---
## State — round `E30` (2026-08-04): three cells on which BOTH sides have a moving outcome class

`E-30` was the last structural blocker of the co-evolution and it was stated as a dilemma: `E18`'s dry
engagement bought a chaos-clean arena (amplitude 1.0 s → 0.10…0.30 s) by removing the opponent's
LETHALITY, and with it the opponent's fitness — on three of five cells red's outcome class is constant
over its entire strategy space. This round shows the dilemma is **false**, and the reason is one line
of the mission format: **an objective is read by the judge and by nobody else**, so a rig can be
re-graded without moving a metre of anybody's flight.

Three new cells, `sat-10-duel-merge`, `sat-11-duel-qra`, `sat-12-duel-gate`, and a two-sided gate
`tools/fb_duel_arena.py`. **All three pass S1, S2 and S7 on BOTH sides.**

### §0 — The instrument, checked against two committed tables before anything was believed

The new gate is new code, so it was pointed at numbers it did not produce, on the binary those numbers
were taken with (`build/fb-gym.e20before`, `7ffb224b6933281d`):

| cell | `E18` §3, committed | `fb_duel_arena`, blue column, pre-`E20` binary |
|---|---|---|
| `sat-07-dry-merge` | 9 distinct, 60.0 % (17, 17), 10 movers | **9, 60.0 % (17, 17), 10** |
| `sat-08-ident-qra` | 10 distinct, 60.0 % (14, 27), 10 movers | **10, 60.0 % (14, 27), 10** |
| `sat-09-gate-strike` | 9 distinct, 52.0 % (22, 23), 12 movers | **9, 52.0 % (22, 23), 12** |

**3 of 3, to the digit, including the mover NAMES.** And the red column reproduces `E19` §3's other
committed table — red's class constant at (24, 8), (12, 4), (12, 4) with **0 movers of 9** — on that
binary AND on the current one. A third reproduction fell out of the rung probe: the MiG-29's emission
band produces exactly **4 distinct trajectories** on `sat-07`, grouped `{base, 0.1, 0.4} · {1.2, 1.5} ·
{1.8, 2.0} · {2.5, 3.0}`, which is `E20` §2's table measured by a different tool.

### §1 — The device, and it costs nothing because an objective is not a physical thing

Each new cell is its source rig plus rung ladders on the side that had none. Measured against the
source under the same binary:

| pair | telemetry files byte-identical | what differs |
|---|---|---|
| `sat-07` → `sat-10` | **17 of 18** | `ma1`'s `zone_zb_s`/`zone_zb_in` AFTER t = 300.1 — the two columns the JUDGE writes, at the instant `sat-07`'s MiG concluded and this file's has not |
| `sat-08` → `sat-11` | **18 of 18** | nothing at all |
| `sat-09` → `sat-12` | 4 of 20, and the other **16 are PREFIX-identical** | the run is longer: 260.1 s → 460.0 s, because a unit with an unmet objective keeps the run open. Blue's eight `UNIT_RESULT` lines are unchanged (SUCCESS × 8) |

What changes is the VERDICT: red's fighters go SUCCESS → TIMEOUT and red's M becomes a count. **The
opponent's fitness was never removed by the dry engagement — it was never declared.**

The graded quantity on the red side is the one `E18` §1b measured as the robust axis: a PLANAR
MIN-RANGE between one named fighter and one named opponent, turned into a COUNT by a monotone rung
ladder, because the class is a SUM (`X-13`) and a lever that changes WHICH bit is met without changing
HOW MANY is invisible to it.

### §2 — The three cells

| cell | blue: distinct / modal / movers | red: distinct / modal / movers | narrowest rung margin, blue \| red |
|---|---|---|---|
| `sat-10-duel-merge` | 12 · **52.0 %** · 12 of 24 | 8 · **30.0 %** · 7 of 9 | 12.3× \| 14.5× |
| `sat-11-duel-qra` | 10 · **60.0 %** · 10 of 24 | 7 · **30.0 %** · 7 of 9 | 10.0× \| 13.2× |
| `sat-12-duel-gate` | 11 · **52.0 %** · 12 of 24 | 7 · **40.0 %** · 6 of 9 | 12.0× \| 11.9× |

**S7, per (cell, opponent) pair, as `X-17` demands and not per cell:** blue's spawn over the 0.8 m grid
against each of the 10 red alleles, red's spawn against each of the 25 blue levers. **0 of 8 on all
105 pairs.** 840 runs for the full matrix plus 224 for the two re-screens.

**The clock is not a confound and it is not argued away:** the run duration takes ONE value per cell
over all 35 runs of both lever sets — 520.0 / 520.0 / 460.0 s. `X-7` cannot reach a comparison in which
every member has the same length.

**Red's lever file is its own** (`tools/levers-red-mig29.txt`, 9 levers): the MiG-29 expresses 3 of the
9 genes (`E-31`), so grading it against blue's 24 would put 22 identity maps into S1's denominator. The
file is the two genes that reach this airframe on these rigs — G5 through the PUR-31 and G3's four sort
contracts — and `sort=left`, the committed brief, is deliberately absent because it IS the identity map.

### §3 — What the round had to measure its way into: a rung must clear EVERY opponent, not one

The first placement obeyed `E18` §1b exactly — every rung at the midpoint of a measured gap, ≥ 10× the
chaos amplitude measured against the COMMITTED opponent. The gate then refused two of the three cells:

| cell | blue S7, first placement |
|---|---|
| `sat-10` | `red emcon-hi` **4 of 8**, `red sort-far` **3 of 8** |
| `sat-11` | `red sort-near` / `sort-none` / `sort-right` **3 of 8** each |
| `sat-12` | clean, first time of asking |

Read at the level of the individual objective (`s7_why`, 9 runs per pair), the flipping rungs are
named exactly: `sw3`'s 5 849 m rung against `ma3` and `sw1`'s 3 941 m rung against `mb4` — rungs whose
margin against the committed MiG is **17×**. A different red allele moves blue's min-range onto them.

**The corrected rule, and it is `X-17` carried from the SCREEN into the PLACEMENT:**

> a rung must lie outside `[min_o − 10 c_o, max_o + 10 c_o]` for EVERY declared opponent `o`, where
> `min_o`/`max_o`/`c_o` are the quantity's minimum, maximum and chaos amplitude over that opponent's
> own S7 population (the graded side committed, its spawn over the ±3 m grid).

That interval is precisely the set of values S7 can produce under that opponent, widened by the same
10×. It costs 90 runs per cell to measure and it is what the two refused cells were re-placed under;
both then came back **0 of 8 on all ten pairs**.

**The rule lives in `tools/fb_rung_ladder.py` rather than in this paragraph**, and the tool was run
end to end to prove it does: 131 runs on `sat-10`'s blue side (25 spectrum, 16 chaos, 90 opponent-band),
worst margin 25.7×, and it reproduces the shipped ladder EXACTLY on the two units where its assignment
agrees — `sw1 → mb4` at 6 017 m and `sw2 → ma1` at 5 781 / 7 953 / 8 409 / 8 937 / 9 608 / 10 092 m.
It picks a different partner for the other two because it was run WITHOUT `--offsets`, i.e. maximising
resolution over the ladder alone rather than over the ladder plus the cell's existing `avoid zone` M —
10 movers instead of 12, which is the offset's whole effect and is stated rather than smoothed. `sat-12` passed without it, which is stated as luck
rather than design — nothing in it was tightened afterwards.

### §4 — A consequence of `E20` nobody had measured: its repair REFUSES two of `E18`'s five cells

`E20` §3 measured its emission seam over all 293 missions by exit code and by class on the three that
moved. It did not re-run the GATE, and the gate is what those cells exist for. Same instrument, same
lever file, the two binaries side by side:

| cell | pre-`E20` (`7ffb224b…`) | post-`E20` (`44b3d0ad…`) | S1 |
|---|---|---|---|
| `sat-07-dry-merge` | 9 distinct, 60.0 %, 10 movers | **5 distinct, 68.0 %, 8 movers** | **REFUSED** |
| `sat-08-ident-qra` | 10 distinct, 60.0 %, 10 movers | 9 distinct, 60.0 %, 10 movers | ok, at the bound |
| `sat-09-gate-strike` | 9 distinct, 52.0 %, 12 movers | **5 distinct, 68.0 %, 8 movers** | **REFUSED** |

The lost movers are `shape-wide`, `shape-flat` and `emcon-wide` on `sat-07` and `net-off`,
`shape-trail`, `shape-wide`, `emcon-wide` on `sat-09` — all four MiG-dependent, all four on cells whose
MiGs are briefed a GCI range beyond the N019's own 50 km. **`E18`'s five-cell arena and its 2⁻⁵ = 0.031
significance ceiling do not hold against the current binary**, and nothing in the tree said so, because
no gate re-run is part of a behaviour change's gate list. The three new cells restore it on their own
terms: 12 movers is the CEILING this lever file allows (12 of its 24 levers are the identity map on any
F-16 side, `E18` §3), and `sat-10`/`sat-12` reach it.

### §5 — The co-evolution, re-run: the opponent produces THREE champions and instrument (b) becomes computable

`tools/fb_campaign_coevolve.py --cells tools/duels-e30.txt --generations 3 --points 3
--archive-sample 2 --genes "blue:pilot_emcon_frac,pilot_flight_trail_frac;red:pilot_emcon_frac"`,
**1 212 runs**, artefacts `sim/build/e30-coevo/`. The genome cut is `E19`'s verbatim, so the only
variable against `E19`/`E20` is the ARENA.

| | `E19` (5 dry cells) | `E20` (same, G5 repaired) | **`E30` (3 two-sided cells)** |
|---|---|---|---|
| distinct RED champions | 1 | 2 | **3** — `r0_s1` (`emcon 1.5 sort=near`) → `r1_00` (`emcon 0 sort=near`) → `r2_s4` (`emcon 0 sort=none`) |
| distinct BLUE champions | 1 | 1 | 1 — `b0_s2`, still fixed from generation 0 |
| archive members, red / blue | 1 / 1 | 2 / 3 | **6 / 3** |
| §3.6 (a) fixed yardstick, red | flat | flat, 0.667 / 0.667 | **0.889 → 1.000 → 1.000, rising** |
| §3.6 (b) champion graph, red | not computable | not computable | **n = 3, cyclic triples 0 of 1, T = 0.0000, ok** |

**Instrument (b) is computable for the first time in this tree.** It needs three distinct champions on
a side; every earlier run produced one or two. Red's archive holds six genomes with six distinct
behaviour vectors where `E19`'s held one.

The champion pair moves the world on all three cells, both ways:

| cell | committed / committed | champ / committed | committed / champ | champ / champ |
|---|---|---|---|---|
| `sat-10` | (15,23) \| (16,24) | (15,32) \| (18,41) | (15,29) \| (19,42) | (14,29) \| (18,33) |
| `sat-11` | (14,15) \| (8,4) | (16,29) \| (8,14) | (15,23) \| (10,18) | (15,26) \| (9,19) |
| `sat-12` | (21,45) \| (8,15) | (21,49) \| (8,16) | (20,41) \| (9,21) | (20,43) \| (9,19) |

**Nothing is published from it.** §6's rule is unchanged: a champion is not a doctrine, the fixed
yardstick decides nothing here, and red's own gene sits at a band rail (`emcon 0`).

### §6 — Why BLUE freezes, first reading: its seed sits where its own genes have no call site

`E19` reported blue's numeric gene as flat and attributed it to `X-4`. The real mechanism is one line of
the runner and it is now measured. `fb_evolve.SORT_ALLELES[0]` is `("off", "")`, so the SEED of every
blue population this tree has ever evolved carries **`dl=off`** — and with the datalink off, both of
blue's declared genes have no call site:

* `FBFlightPicture::BuildMembers` adds SELF and returns unless `state.Datalink.H.Readable()`, so there
  are no mates, no lead member, and `FormationStation` is never called — **G1 is dead** (the identical
  mechanism `X-16` measured on the MiG-29, here produced on an F-16 by a BRIEFING rather than by an
  airframe);
* `EmconSilent_` needs an `other` picture — a mate's `Engaging` report or a net report — and with no
  mates and no net there is neither, so the pilot can never go quiet: **G5 is dead**.

[MESS, 30 runs, `build/e30-freeze`] blue's declared genome over its own grid, once with the seed's
channel bit and once with the mission's own:

| blue genome | `sat-10` | `sat-11` | `sat-12` |
|---|---|---|---|
| `dl=off` seed / `trail 0` / `trail 3` / `emcon 0` / `emcon 3` | (15,39) × 5 | (16,23) × 5 | (21,42) × 5 |
| mission's own channel, seed | (15,21) | (14,15) | (20,35) |
| … `trail 0` | (15,21) | **(15,22)** | (20,35) |
| … `trail 3` | (15,21) | (14,15) | (20,35) |
| … `emcon 0` | **(15,36)** | **(16,27)** | **(20,40)** |
| … `emcon 3` | (15,21) | (14,15) | (20,35) |

**Five genomes, one class, three cells — with the net off. With the mission's own channel the same five
produce three distinct classes on `sat-11` and two on the other two.** Blue's freeze is a property of
the SEED, not of the cells: the search was started inside the one subspace where its own alphabet is
the identity map. `fb_campaign_coevolve.py` grew a `--blue-alleles` argument for it, defaulted to
today's behaviour so that no published run moves.

### §7 — The second co-evolution, with the seed freed — and blue walks into the dead subspace on its own

`--blue-alleles ":,:left,:near,off:"`, everything else identical, another **1 212 runs**
(`build/e30-coevo2/`). The seed now leaves the mission's own channel alone and `off:` is one allele of
four rather than the ground the search stands on.

**Generation 0 separates blue's genome for the first time in this tree.** The frozen-field score, which
was 0.500 / 0.800 flat in `E19` and 0.667 flat in §5:

| blue genome | frozen |
|---|---:|
| `b0_seed` (mission's channel, `emcon 1.5`, `trail 1.5`) | 0.208 |
| `b0_01` (same point, re-emitted) | 0.267 |
| `b0_02` (`trail 3`) | 0.367 |
| `b0_00` (`trail 0`) | 0.700 |
| `b0_10` (`emcon 0`) | 0.900 |
| **`b0_s3` (`dl=off`)** | **0.933** |

**And then it freezes again — because the allele that wins generation 0 is the allele that silences the
other two genes.** From generation 1 on, every member of the population carries `dl=off` (`grid_poll`
mutates the CHAMPION), and all six print the identical pair 0.570 / 0.933 in generation 1 and
0.586 / 0.933 in generation 2. The three instruments: (a) 0.933 flat, (b) **1 distinct champion, still
not computable**, (c) needs four generations.

**That is a sharper statement than §6's and it replaces it as the reason.** Blue's freeze is not (only)
the seed: with a free choice of channel, blue's own fitness lands it in the subspace where `dl=off`
kills G1's call site and G5's picture. **The channel bit is a ONE-WAY GATE — the first move of the
search removes two thirds of the alphabet the search still has to explore.** `E-34` is rewritten around
that.

Red, on the same arena but against this different blue, produces **2** distinct champions
(`r0_s4` → `r1_02`, `emcon 1.5 → 3` at `sort=none`) and a rising yardstick 0.889 → 1.000 → 1.000; §5's
run produced 3. Both are reported; neither is a doctrine claim. **The honest summary of the pair of
runs is that instrument (b) is now REACHABLE — it was computed once, on the red side, at n = 3 with
T = 0.0000 — and that whether it is reached depends on which side and which opponent, which is the
first time in this tree that sentence could be written at all.**

### §8 — What the round exploited, and every item is an instrument defect

| # | The hole | The channel it rides | Who owns it |
|---|---|---|---|
| **X-21** | **A rung that clears the COMMITTED opponent by 17× still flips S7 under another opponent.** `E18` §1b's placement rule takes its two inputs — the spectrum and the chaos amplitude — against one point of the opponent space, and `X-17` had already shown that S7 is a property of the pair. [MESS, `E30` §3] `sat-10` blue **4 of 8** under `red emcon-hi`, **3 of 8** under `red sort-far`; `sat-11` blue **3 of 8** under three of the four sort alleles. Read per objective, the coins are `sw3`'s 5 849 m rung and `sw1`'s 3 941 m rung. **The rule now carries a third input** (§3) and the two cells were re-placed under it | `fb_duel_arena.chaos_pairs`, per (cell, opponent) | this file — and `tools/fb_rung_ladder.py`, where the rule now lives as code rather than as prose | **REPAIRED in the placement**; the screen is what caught it, which is the screen working |
| **X-22** | **A behaviour change is not gated on re-running the arena, so `E20`'s repair silently REFUSED two of `E18`'s five cells.** `E18` certified five chaos-clean gradable cells and a 2⁻⁵ = 0.031 significance ceiling; `E20` moved the MiG-29's emission seam and measured 293 missions by exit code. Nobody re-ran the gate. [MESS, §4, both binaries side by side] `sat-07` 60.0 % → **68.0 %** and `sat-09` 52.0 % → **68.0 %**, S1 REFUSED on both, four MiG-dependent movers lost. **Every statement in this file that rests on the five-cell arena is a statement about the pre-`E20` binary** | `fb_duel_arena` against `build/fb-gym.e20before` | this file, and [`build-and-ops.md`](build-and-ops.md)'s gate list — a change that moves an airframe moves every cell that airframe flies in | **OPEN as a process gap**, booked as `E-33` |
| **X-23** | **A lever file cannot express the `none` sort contract: `fb_tournament.load_variants` maps `sort=none` onto the EMPTY sort**, which is "splice nothing", which is the identity map. `none` is a real `FBMig29Module::Set` value and `E19` measured it as a mover on both sides; a red lever file written the obvious way would have carried a silent identity map into S1's denominator — `X-14`'s class, in the loader instead of in the rig | `fb_tournament.load_variants`, `sort=` | [`pilot.md`](pilot.md) §9 | **worked around, not repaired**: `fb_duel_arena.genomes` re-reads the raw token the way `fb_campaign_arena.load_levers` already re-reads `dl=`. The loader itself is untouched because four published tournaments read it |
| **X-24** | **A spectrum taken over the whole track grades a quantity the VERDICT never saw.** An `until <s>` window closes at `<s>` and the judge stops accumulating; a rung placed from the full track is therefore placed against a number nobody is graded on. [MESS] on the first `sat-10` draft this moved one MiG's rung count by **three of six** — predicted 4, judge said 1 | `mission WINDOW_CLOSED` against the track | this file; `fb_rung_ladder.min_range` takes the span as an argument for exactly this reason | **REPAIRED**, and the corrected predictor then reproduced the judge **8 of 8 units** on the first try |
| **X-25** | **The channel bit is a one-way gate, and every blue population this tree has evolved was seeded on the far side of it.** `fb_evolve.SORT_ALLELES[0] = ("off", "")`, so the seed carries `dl=off`; with the net off `FBFlightPicture::BuildMembers` finds no mates (G1 has no `FormationStation` to reach) and `EmconSilent_` has no cooperative report (G5 can never go quiet). [MESS, §6, 30 runs] five blue genomes spanning both declared genes give ONE class per cell with `dl=off` and three distinct classes on `sat-11` with the mission's own channel. `E19` attributed this to `X-4`; the mechanism is a missing CALL SITE and not a cost. **And freeing the seed does not fix it** (§7): with the channel a free allele, generation 0 separates blue's genome 0.208…0.933 and the WINNER is `dl=off`, after which the population is flat again | `fb_evolve.SORT_ALLELES`, `flt_mates` | this file §3, [`formation.md`](formation.md) F5 | **argument added** (`--blue-alleles`, defaulted to today's behaviour so no published run moves); the default is NOT changed in the round that measured it, and §7 says a different default would not have been the fix |

### §9 — What is NOT claimed

- **No doctrine shift is published.** §6's template is not filled in. The sweep of `E18` §4 / `E19` §5
  was NOT re-run on the new cells — the round's budget went into the cells and the two co-evolutions —
  so `E-28` ("every direction reverses against some opponent") stands exactly where `E19` left it.
- **Instrument (b) was COMPUTED once, not established.** §5's red side reaches n = 3 with T = 0.0000;
  §7's red side, on the same three cells against a different blue, reaches n = 2 and is not computable.
  What the round shows is that (b) is REACHABLE on this arena, which no earlier round could say at all —
  not that it is reached by every run.
- **Three cells is not five.** The significance ceiling a three-cell sign test can reach is
  2⁻³ = 0.125, worse than `E18`'s 0.031. What the three buy is not a ceiling but a SIDE: they are the
  first cells in the tree on which the opponent has an outcome class that moves at all.
- **The three cells are not independent.** They share the dry-engagement device and two of the three
  share the min-range ladder as their primary axis. `E18` §8's warning applies unchanged.
- **`sat-11`'s blue S1 sits at 60.0 %, the bound, with no slack**, and the arithmetic is in its own
  header: eleven of the 24 levers are bit-identical trajectories there, so 56.0 % is the floor.
- **Red's genome is two genes wide**, and that is `E-31` and not something this round fixed. Red's
  champion moving is a real movement inside a small space, not evidence that the space is big.
- Nothing in `sim/src/`, `sim/vendor/` or `sim/assets/aircraft/` was touched, so no behaviour
  regression is owed. `make -C sim verify-models` green; the guarded tree
  (`sim/missions sim/assets/aircraft sim/assets/MODEL-DELTAS.md sim/vendor sim/src`) shows no modified
  file before or after any run; determinism `--threads 1/2/4` on `sat-10-duel-merge` gives the identical
  telemetry SHA-256 `32720b2093962ca5` three times.

### §10 — The cost

| | |
|---|---|
| runs | ≈ **4 000** — 21+10 red probes ×3 cells, 33+11 blue probes ×3, 90+90 opponent-space probes, 102+68 gate sweeps, 840+224 S7 pairs, 108 pre-`E20` cross-check, 30 freeze probe, 1 212 co-evolution, 1 212 second co-evolution, 131 ladder-tool validation, 6 conservation, 3 determinism |
| what moved in `sim/src` | **nothing** |
| what moved elsewhere | three cells (`sat-10-duel-merge`, `sat-11-duel-qra`, `sat-12-duel-gate`), `tools/fb_duel_arena.py`, `tools/fb_rung_ladder.py`, `tools/levers-red-mig29.txt`, `tools/duels-e30.txt`, one defaulted argument on `tools/fb_campaign_coevolve.py` |
| `sim/vendor`, `sim/assets/aircraft` | untouched |


---

## Gaps

| # | Thing | Known from |
|---|---|---|
| **E-30** | **ANSWERED (`E30`) — a dry rig CAN grade both sides, because an objective is not a physical thing.** The dilemma was stated as "either the exchange is lethal and the chaos returns, or it is dry and the opponent has no fitness". It is false: [MESS, `E30` §1] adding a rung ladder to the red side of `sat-07`/`sat-08`/`sat-09` leaves **17 of 18, 18 of 18 and 16-of-20-prefix** telemetry files identical, because `objective` is read by `FBMissionMonitor` and by nobody else. The three resulting cells `sat-10-duel-merge`, `sat-11-duel-qra`, `sat-12-duel-gate` pass S1, S2 and S7 **on both sides**: blue 52.0 / 60.0 / 52.0 % modal with 12 / 10 / 12 movers of 24, red 30.0 / 30.0 / 40.0 % with 7 / 7 / 6 movers of 9, and **0 of 8 spawn flips on all 105 (cell, opponent) pairs**. The co-evolution on them produces **3 distinct red champions and a 6-member red archive** where `E19` produced 1 and 1, and §3.6's instrument (b) is computable for the first time. **What the dry device really removed was never the opponent's fitness — it was never declared** | `E30`, `E19` |
| **E-33** | **A behaviour change is not gated on re-running the ARENA, and the tree has now paid for it once.** [MESS, `E30` §4, both binaries side by side, same instrument, same lever file] `E20`'s emission seam took `sat-07` from 60.0 % modal / 10 movers to **68.0 % / 8** and `sat-09` from 52.0 % / 12 to **68.0 % / 8** — S1 REFUSED on two of the five cells `E18` certified, and with them the 2^-5 = 0.031 ceiling every later statement quotes. `E20` §3 measured 293 missions by exit code and three by class; nothing in [`build-and-ops.md`](build-and-ops.md)'s gate list says "re-run the gate whose cells this airframe flies in". **What is owed is one line in that list and a cheap re-run target**, not a new instrument: `fb_duel_arena.py --cells … ` already answers it in 102 runs for three cells | `E30`, `X-22` |
| **E-34** | **The channel bit is a ONE-WAY GATE: blue's search wins generation 0 by turning the net off, and `dl=off` is the state in which its other genes have no call site.** Two measurements, and the second replaces the first as the reason. (1) [MESS, `E30` §6, 30 runs] `fb_evolve.SORT_ALLELES[0] = ("off", "")`, so every blue seed this tree ever evolved carried `dl=off`; five blue genomes spanning both declared genes then give **one outcome class per cell** on all three cells, because `FBFlightPicture::BuildMembers` finds no mates (G1 never reaches `FormationStation`) and `EmconSilent_` has no cooperative report (G5 can never go quiet). (2) [MESS, `E30` §7, 1 212 runs with `--blue-alleles ":,:left,:near,off:"`] with the seed freed, generation 0 separates blue's genome for the first time — frozen field **0.208 / 0.267 / 0.367 / 0.700 / 0.900 / 0.933** over six genomes — and the winner is `dl=off` at 0.933. From generation 1 the whole population carries it and is flat again. **So the freeze is a property of the LANDSCAPE, not of the seed:** the first move of the search removes two thirds of the alphabet the search still has to explore. What is owed is not a different default but a decision about whether a gene that disables other genes may sit in the same genome — and the answer is a design question, not a measurement | `E30`, `X-25` |
| **E-29** | *(SPLIT by `E20` — the headline survives in a weaker form; see `E-31` and the `E20` State block.)* **The genome cannot be co-evolved because it is F-16-shaped, and the boundary test in §2 cannot see it.** [MESS, `E19`, bit-level over whole telemetry] of the nine genes, **eight are bit-identical on the MiG-29** on `sat-02`/`sat-04`/`sat-07`; only the sort contract reaches it. Over the full 24-key pilot alphabet, **12 keys DO move the MiG and not one of them is in the genome** (`abort_nm`, `action_s`, `beam_deg`, `chaff_s`, `crank_deg`, `defend_hold_s`, `lock_nm`, `react_s`, `shot_ata_deg`, `shot_rtr`, `shot_spacing_s`, `speed_kt` — all `Free`). §2.2's `static_assert` proves a `Scale` gene is DIMENSIONLESS and names its hook; nothing proves the hook is OVERRIDDEN by the module the gene is spliced into. **Not fixed here on purpose:** growing or re-cutting the genome in the round whose verdict it decides is what §8 forbids, and the fix is a design question (a per-module reachability declaration read from the binary like the alphabet itself, so it cannot drift) | `E19`, `X-15`, `X-16` |
| **E-28** | **Every doctrine direction this tree has measured reverses against some opponent, and no opponent admits one.** [MESS, `E19`, 625 runs over 5 blue lever sets × 5 red alleles × 5 cells, 21 chaos-clean pairs] per red allele the best blue lever is `emcon-mid` p = 0.188 (`red-left`, i.e. `E18`'s world, reproduced exactly), `emcon-tight`/`shape-abreast` 0.125, `shape-tight` 0.250, `net-off` **0.062**, `emcon-tight`/`shape-trail` 0.250 — **none reaches the 0.031 ceiling under any opponent.** And the sign of every non-rail lever FLIPS: `emcon-mid` is `+ + − − =` across the five, `emcon-tight` `+ + − + +`, `net-off` `= = = + −`, `shape-abreast` `+ + + − −`. The only sign-stable directions are "always worse" (`bias-rail`, `ccip-tight`, both rails) and "always nothing" (the identity levers of `E-26`). **This is the diagnosis `E18`'s p = 0.188 was missing**: the near-miss was not a weak signal but a signal conditioned on one point of the opponent space. **Still open:** whether an opponent-INVARIANT direction exists at all, which needs both `E-29` (a genome the opponent can carry) and `E-30` (a rig that grades the opponent) first | `E19`, `E18` |
| **E-1** | **CLOSED.** `mission OBJECTIVE unit=… kind=… state=met\|unmet\|violated`, one line per declared objective, published in `FBMissionMonitor::Conclude`. Cost, as promised and measured: 136 lines over 60 of 137 missions; 432/432 telemetry files byte-identical | this file |
| **E-2** | **CLOSED.** `dmg_hits` read off three gun missions; the mechanism is confirmed and the 1,500 points/s figure is an upper bound measured at 900–1,278. See §State | this file |
| **E-3** | **The −1450.0 decomposition is derived, not read** off the tool's printed items — and it no longer decomposes that way at all, because `no shot` and `hits landed` are gone. The re-run is now `--attribution mig21` under the order scalar | this file |
| **E-4** | **G5 (EMCON) cannot be evolved before `duels.md` D3 closes.** The runner prints the blocker at start | [`duels.md`](duels.md) D3 |
| **E-5** | **G1 (formation shape) cannot be evolved before `formation.md` F5 closes.** The runner prints the blocker at start | [`formation.md`](formation.md) F5 |
| **E-6** | **CORRECTED (`E2`): G2 is not inert, it was measured on the wrong geometry — and half its band is dead.** The 132/132 zeros were taken on `xmirror`, whose east seat is a MiG-29, an aircraft with **no datalink at all**, so the element being measured never had a mate's bound-bit to act on. [MESS, `--flight 2`, `dl=on` both sides] `flt_defer_s` at rails 0 / 1.0 / 3.0 is 0.0 / **6.3** / 6.3 s on `split` with `flt_both_s` (5.6, 4.6) → (0, 0), and 0 / 0 / 0 on `mirror`, `far`, `xclose`, `xmirror` — the binding is the round's time-to-active and only a 12 000 m / 500 kt seat launches far enough for it to outlast the spread between the two members' solutions. **Still open:** the band is flat above 1.0 (the cap already outlasts the mate's binding there), and the 6.3 s of deferral buy −0.4 craft points and never an outcome class | [`formation.md`](formation.md) F3, `E1`, `E2` |
| **E-7** | **The archive's arena fingerprint has no definition yet.** The archive file names its arena and timeout in a header and a mismatched one must be refused by hand; nothing computes a fingerprint that binds the binary and the elevation record | this file |
| **E-8** | **Wall-clock, now measured for the shapes this round flew** and still not for the specified P = 12 / k = 8: arena check 8 geometries × 39 runs = 312 runs in **2 m 33 s**; 4 generations × P = 4 at `--flight 2` (≈ 260 runs) in **5 m 39 s**; a 420 s 1v1 BVR duel costs ≈ 0.5 s wall at `--jobs 6` | this round |
| **E-9** | **`flt_dup` counts sharing, not the violation** | [`formation.md`](formation.md) F4 |
| **E-10** | **Level C's engagement gate has no channel for "committed but never locked"** | this file |
| **E-11** | **The fitness is NOT silent in a saturated arena, and §Knowledge 1 claims it is.** Pairwise domination consults C whenever V and M tie, and two floats are never equal, so a dead arena still produces a full ranking. The mitigation built is a REPORT and not a change to the order: every tournament prints which level decided each run and shouts `SATURATED` when V and M decided none. Changing the order to abstain on a level-C tie was NOT done, because it would make the tournament silent about real craft differences in an informative arena too | this round |
| **E-12** | **RUN, and the arena FAILS it.** `tools/levers-genome.txt` (the three live genes at three points each) plus `fb_arena_check.py --flight N`. [MESS] at `--flight 2` under the genome's own alphabet the arena has **1 informative geometry of 12** against S5's 3 and is **REFUSED**; at `--flight 1` under the declared nine it has 4 and PASSES. **Still open:** nothing in the runner yet REQUIRES the genome-alphabet gate to have passed — `fb_evolve.py` still only prints the reminder. Making it a hard refusal would stop every run this tree can currently fly, so it is booked rather than built | `E1`, `E2` |
| **E-13** | **DIAGNOSIS REPLACED (`E2`).** The total tie was the AGGREGATION, not the genome: §1.4's cross-seat comparison returns the SEAT in both mirrored runs wherever the seat carries the key, so every variant takes one point of two. [MESS, same telemetry, only the comparison changed] cross-seat 0.500 × 12, same-seat 0.227…0.773. Three of the five genes grip when measured one at a time (§State 1). What remains true of E-13's second half: the genome and the arena still barely intersect — of the five genes exactly **one** (G3) moves an outcome class on exactly **one** geometry (`xfarsplit`) | `E1`, `E2` |
| **E-14** | **HALVED (`E4`): the merge now has an OUTCOME, and G4's mover is a CFIT.** (a) and (b) are unchanged — no transition from the intercept phase into `Phase::Bfm`, and the merge writes no `eng_*` column, so level C is `GATE` on both sides. (c) is CLOSED: `Phase::Bfm` employs the round on the rail ([`pilot.md`](pilot.md) §5.11) and `xmerge` goes from 60/60 `(2,1)` to **30 (3,2) + 30 (1,0)** — every run decided, all 40 kills by a missile, none by the gun. The gun itself went 2.21 % → **7.68 %** of its rounds on target (§5.8) and still needs 17.0 landed 30 mm rounds against 9.53 delivered. **The new half of this gap:** all three `pilot_energy_frac` alleles now move the class on `merge` — S2's first pass on a merge cell — and all three do it with a `monitor KO ATTITUDE_CONTACT` of a jet the AIM-9 exchange had already blinded. `E-15`'s rule applies unchanged and G4 is not published |
| **E-15** | **CLOSED (`E3`), and closing it CONFIRMED the reading: the merge's S1 pass WAS the CFIT.** At n = 120 runs per pass the merge cells produced **77 monitor KOs and every single one was the MiG-29** (38 `ATTITUDE_CONTACT`, 37 `CFIT`, 2 `STRUCTURE_CONTACT`); zero F-16 KOs, in either seat. The cause was not the pilot and not the floor: `systems/FBFlightControl` bound this airframe's own rate damper only on its FLCS path while `Phase::Bfm` commands `Manual` ([`pilot.md`](pilot.md) §5.10a). With it on the hand stick the same 120 runs produce **0 KOs** — and `xmerge`/`xmergesplit` fall from 2 outcome classes at a 50.0 % modal share to **1 class at 100 %**, i.e. they lose their S1 pass with the defect that was carrying it. That is the finding stated forwards: a geometry whose informativeness comes from one side dying of a bug is a measurement of the bug | `E2`, `E3` |
| **E-17** | **The campaign breadth is REFUSED as an arena, and 89 of its 154 cells are not moved by the genome at all.** [MESS, `E5`, 2026-07-30] 0 informative cells under both the published `levers-genome.txt` and that round's 15-point file; 2 under the loosest reading the gate admits, which is W1's own verdict. **The three checkable points, with their state after `E6`:** (a) **CLOSED** — level C was `GATE` on 32 of the 46 cells that aim a bomb and is now `GATE` on **0 of 46**, with the two deciding levels unmoved on all 154 cells (§State `E6` 1); (b) **OPEN, unchanged** — S1 needs a fixed field that acts on the cell it judges, and the only frozen one is six BVR intercept doctrines; a ground-flavoured yardstick would fix it on paper and is refused for `E2`'s reason; (c) **OPEN, unchanged** — G2 and G7 need an arena that does not exist in the campaigns at all (a long-binding round on a netted element; a CCIP delivery). **The debt is PAID (`E7`)**: the 154-cell gate is re-run in full, 4,158 runs, and the answer refutes the prediction it was booked with — the X-1 fix moves the mover distribution by **two cells** (89·46·15·3·1 → 89·46·16·2·1), because a mover count is a difference and the fix shifted baseline and levers across the boundary together. **(b) is CLOSED (`E7`) and the closing INVERTED it**: the field was not merely ground-blind, it was disjoint from the genome in every gene, and a commensurate field passes S1 on **0 of 154** cells where the incommensurate six passed 13 — all thirteen false positives, traceable one by one (§State `E7` 2). **(c) is now the ONLY thing standing**, and it is the binding constraint: S2 does not read the field at all, 0 of 154 cells reach its 5 movers, the best in the whole breadth has 4, and **5 of the 15 levers are structurally dead everywhere** (G2's three, G7's two) | `E5`, `E6`, `E7`, [`campaigns/w1-red-flag.md`](campaigns/w1-red-flag.md) |
| **E-27** | **CLOSED as a COUNT, OPEN as a result.** A shift needs FIVE chaos-clean graduable cells; `E18` builds three more and the tree now has **five** — `sat-02`, `sat-04`, `sat-07-dry-merge`, `sat-08-ident-qra`, `sat-09-gate-strike`, all S1 ok / S2 ok / **S7 0 of 8**, gate output `sim/build/e18/arena.log`, **ARENA: PASSED**. Ceiling `2^-5` = **0.031**. The structural device that made them possible is the DRY ENGAGEMENT (`brief_master_arm sim` on BOTH sides: the full picture/sort/emission machinery runs, no weapon leaves a rail, and the chaos amplitude of a graded dwell falls from 1.0 s to **0.10…0.30 s**). **And the sweep the ceiling was wanted for is NEGATIVE:** no lever is better on 5 of 5; the two that reach `p = 0.031` (`bias-rail`, `ccip-tight`) are both WORSE and both sit on a RAIL, which §6 §2 refuses; the strongest positive is `pilot_emcon_frac` 1.0 → 0.4 at 4 of 5, `p = 0.188`, non-monotone in its own gene (0.1 wins 3 of 5, 3.0 wins 1 of 5) and therefore a point rather than a direction. It passes X4a (0 of 8 on its OWN genome, all five cells), X4b (held at `timeout` × 1.5) and X4c. **Still open:** an admissible shift, and the independence of the five (all three new cells share the dry-air element, because `pilot_emcon_frac` is inert without an engagement) | `E16`, `E17`, `E18` |

| **E-26** | **Half the lever file is the identity map, and S2's bar is computed from the file's LENGTH.** [MESS, `E16`] 12 of the 24 levers in `tools/levers-campaign-g5.txt` are bit-identical to the baseline in all 28 published channels — `durationS` and the energy integral `bfm_es` included — on all three `sat-*` cells: `cover-off/one/three` and `energy-low/mid/high` (structurally unreachable on an F-16 side, as `85c1a74` already booked), plus `net-on` and `ccip-open` (they ARE the missions' own briefing), `sort-left`/`sort-near` (dead beside a live net) and **`emcon-tight`/`emcon-mid`** (§6b: below the step they are the seed). `kMoverFrac = 3/9` puts the bar at `3/9 × 24 = 8`, so eight movers must come out of **twelve live levers**, not twenty-four. E10 wrote the ratio so a LONGER file could not buy a pass; the unforeseen direction is that padding a file with identity levers RAISES the bar for reasons that have nothing to do with the genome. **Not fixed here on purpose** — a denominator retuned in the round whose verdict it would change is what E10 forbids. The honest reading of every S2 number in this file is "movers of the LIVE levers", and that count is not printed today | `E16` |
| **E-25** | **X1 is nearly empty on a three-cell arena, and a champion can pass it while being net negative on 154 independent cells.** [MESS, `E13`] the published shift survives X1 (better on 3 of 3 arena geometries, the two it rests on flipping 0 of 8) and then scores **17 better against 38 worse** on the campaign breadth, with its own mechanism channel `flt_switch` falling on 6 cells and RISING on 9. X1 asks for *"≥ 2 of the arena's other informative geometries"*; with S5's minimum of three that is a two-sample test. **The fix is not to loosen or tighten X1 but to name the validation set**: selection on the arena, validation on the breadth, and a shift that does not transfer is reported as arena-specific rather than as doctrine. Not retrofitted into §5 here — a criterion rewritten in the round whose result it would change is not a criterion | `E13` |
| **E-24** | **Thirteen cells are invisible to level M, and every one is inert.** [MESS, `E11`] 13 of 154 cells carry `M = 0` on all 24 levers and **not one has a single mover** — among them the breadth's largest symmetric engagement, `o1-10-mole-cricket` at eight against eight, `(16, 0)` on both sides. It is not a defect of the mission: its own header says no aircraft declares `objective survive`, and its reading rule names five channels — `campaign CARRY`, `site LAUNCH`, `net LOST`, the per-jet `eng_*` debrief, the campaign ATTRITION line — **none of which the fitness reads**. A rung whose product is an attrition arc cannot be seen by an outcome class of `(V, M)` and therefore can never be informative, whatever its size or opposition. **Not fixed here on purpose:** widening level M after measuring which cells it cannot see would select the instrument on the result | `E11` |
| **E-23** | **The gate has been reading DEFECTS, and four independent repairs prove it.** [MESS, `E10`] `E-15`'s FLCS damper took `xmerge`/`xmergesplit` from 2 outcome classes at 50.0 % to **1 at 100 %**; X-1's judge took `w4-10-allied-force:f16` from **3 movers to 0**; the two together took the generated arena from **4 informative to 1** of 12; and D3a's `CanPressOn` took `w1-07-emcon:f16` from **5 movers to 0** while improving its baseline from `(3, 1)` to `(4, 2)`. `E-15` wrote the rule for one geometry — *"a geometry whose informativeness comes from one side dying of a bug is a measurement of the bug"* — and it is now a property of the whole arena. **No escape hatch is proposed:** every one of the four made the simulator more correct, and the conclusion is about the INSTRUMENT — a criterion built on "does the outcome class move" measures a mixture of doctrine and defect, and in this tree the mixture has been mostly defect. What survives the repairs is the real signal, and today it is **one mover short of S2 on one cell of 154** | `E10`, `E-15`, X-1, `E-12` |
| **E-22** | **S2's threshold is a RATIO, so growing the genome moves the bar with the measurement.** [MESS, `E9`] a gene contributing `k` levers raises `3/9 × n` by `k/3`. G1 brought 6 levers, supplied **3** movers on the best cell, and the bar rose by **2**: the breadth went from *best 4 of 15, threshold 5* to *best 6 of 21, threshold 7* — **deficit 1 in both cases**. This is E10 working exactly as written; what was never written down is the consequence — **a gene helps only where its own per-cell reach beats `k/3`**, so a gene that moves many cells by one lever each (G1: 13 cells) cannot open the gate and only a gene that moves ONE cell in three of its own levers can. Falsifiable in advance for the next gene: G5's EMCON levers must move ≥ 3 of their own on a single cell or they will move the bar and not the verdict | `E9` |
| **E-21** | **The gate cannot tell a lever from a coin, and the contamination is largest where the gate likes the cell most.** [MESS, `E8`] `w1-09-lfe-four:mig29` carried the **most movers of the whole campaign breadth (8 of 21)** and its outcome class flips on **8 of 8** spawn perturbations of ±3 m under a genome that sets one unrelated gene; `o3-10-october-six:mig29` flips on 3 of 8. Both passed S1 AND S2 and were certified informative. §5's noise floor is 2 of 8, so both are geometries on which *"no claim may be made at all"* — and the gate that certified them has **no chaos criterion**, although the detector has existed all along as a post-hoc audit on the champion. **The contract this asks for (next round, not retrofitted here): a seventh criterion S7 — a cell is informative only if its BASELINE outcome class survives the same 0.8 m grid. Cost: 8 runs per cell.** Applied to this round's three it would leave one, and the arena would be REFUSED — honestly | `E8` |
| **E-20** | **BOTH arenas are refused, so the blocker is the GENOME.** [MESS, `E7`] the campaign breadth 0 informative of 154, the generated geometries **1 of 12** at `--flight 1` against the **4** E-12 recorded — three were lost to the tree's own repairs (`E-15`'s FLCS damper, X-1's judge), which is `E-15`'s rule applied to itself. Of the five genome growths the owner goal names, **two are not keys at all** (G1 blocked by [`formation.md`](formation.md) F5, G5 by [`duels.md`](duels.md) D3 — both `SET_REJECTED` at t = 0.0, exit 1), G2 is inert for want of a weapon binding, G4 lives only in `Phase::Bfm`, and G3 alone acts broadly. The backlog that follows is ordered in §State `E7` 5 | `E7`, `E-12`, `E-15` |
| **E-19** | **S1's threshold is a SHARE, and a share is not invariant under the size of the field it is taken in.** [MESS, `E7`] adding five members that are inert on a cell raises its modal share from `m/n` to `(m+5)/(n+5)`: the thirteen cells that passed S1 kept their outcome-class count EXACTLY (2→2, 3→3, 4→4) and went 50.0 % → 72.7 % and 33.3 % → 63.6 %. The direction is right — an inert member must not certify a cell — but `kModalMax = 0.60` was calibrated against a field of six, and no constant in the gate knows how large its field is. **Not fixed here on purpose:** a threshold retuned at the end of the round that its own field made fail is exactly what E10 forbids | `E7` |
| **E-18** | **CLOSED (`E6`, 2026-07-30). Level M could not tell "judged and unmet" from "never judged"** — both read 0, and the difference was worth 8 points of M on `w4-10`. The runner now asks every open judge after the loop, whatever ended the run, and always AFTER the combination so no verdict can move. [MESS] `w4-10-allied-force:f16` baseline goes from `V = 16, M = 0` to `V = 18, M = 8`, which is what its three levers already read; 251 missions with 0 telemetry values and 0 exit codes moved | `E5`, X-1 |
| **E-16** | **A converged population and a circling one look alike in instrument (c).** The `E2` run's champion never changed, so "min distance to a champion 3+ generations back" is 0.0000 — the reading §3.6c gives a CYCLER. T = 0.0000 and the yardstick was flat, so this one is a fixed point; the instrument cannot tell the two apart on its own and the file now says so | `E2` |

### Exploits the evolution found

**Round `E5`, over the ten campaigns.** One is an exploit of the INSTRUMENT and rides no physics at
all; two are defects of the delivery chain that a genome value buys back; one is a doctrine effect
whose channel is published and whose reading is uncomfortable. Each row carries the channel, the
number and the file that owns the defect. **Nothing here was fixed** — this round did not touch
`sim/src/`, and a fix is a round of its own.

| # | What the search found | The channel it rides, with its number | Owner | X3 |
|---|---|---|---|---|
| **X-1** | ~~**A healthy departure of ANY unit, on either side, deletes level M for the whole mission**~~ — **CLOSED `E6`, 2026-07-30.** The finding stands with its date and its numbers | **[MESS 2026-07-30, `E5`]** `FBMissionRunner`'s loop ends at `FirstFlightKo`; `ExpectedLoss` forgives a K.O. only for a unit that is already combat-ineffective, so nobody `Conclude`s and **0 `mission OBJECTIVE` lines** are published. `w4-10-allied-force` baseline: `kamig4 LOC "stall/mush"` at t = 695.3 of 700 ⇒ eight F-16s at `V = 16, M = 0`. `net-off`, `bias-early` and `bias-rail` each keep it flying ⇒ **`V = 18, M = 8`**. `bias-rail` gets there by throwing its bombs **2,794 m** wide. **17 of 154 cells** have a lever that crosses the boundary, **5** baselines sit on the wrong side. — **[FIX, `E6`]** the runner asks every judge still open after the loop and AFTER the combination, so a run that ENDS publishes its vector while `FirstFlightKo` keeps its meaning to the tick. The same cell now reads `V = 18, M = 8` for all four, movers 3 → 0, `RESULT`/exit unchanged | this file (level M's input, E-1) + `missions/FBMissionRunner.cpp` | **FAILED** — the advantage had no chain to the opponent at all. An exploit by §5's inverted burden, and the defect was ours |
| **X-2** | **The pilot's pickle carries ~0.2 s of uncancelled chain latency, and a genome value buys it back** | `ATTACK_RELEASE biasS=0 leadS=0.6 ttrS=0.569` → `stores DELIVERY predErrM = 52.57 m = gs × 0.227 s` (W2 measured 0.228–0.241 s over seventeen drops) → `aimLongM 36.34` of `aimErrM 36.38` (99.9 % along) → `damage rangeM 33.66` against a Mk-84's 17.7 m. Swept on 8 strike cells over four campaigns, two stores and four altitudes, the minimum of `aimErrM(bias)` sits at **−0.20 ± 0.05 s on every one of them** — a constant TIME, not a constant distance, which is what says it is a latency. At −0.20 s, `w2-01-dome` 36.38 → **10.06 m**, the hardened dome is DESTROYED and `(V, M)` goes (2,1) → (3,2) | [`pilot.md`](pilot.md) §5 attack pass — `FBF16Pilot::AttackReleaseBiasS()` returns **0.0 s** | passes: chain named, number at every link. **Not an exploit — a defect of the default** |
| **X-3** | **The release time is quantised at the pilot's decision tick, and the quantum is wider than the weapon's own lethal radius** | the release cue is evaluated once per `DecisionDtS_` = 0.1 s, so `aimErrM(bias)` is a STAIRCASE: 36.38, 36.38, 13.29, 13.29, 10.06, 10.06, 33.12, 56.24 m over bias 0 … −0.40 in 0.05 s steps — pairs, one step per tick. At a loaded F-16's 231 m/s one tick is **23.1 m** of track against a hardened target's **17.7 m** radius, so the best reachable lattice point can be 11.5 m off with a perfect bias and the residual floor is 10–22 m on all eight cells | [`weapons.md`](weapons.md) / [`pilot.md`](pilot.md) §5.8 — the same partition class as Exhibit C, on the release clock instead of the gun bundle | passes as a mechanism; it is §5's **partition class** and is listed as one |
| **X-4** | **The cooperative datalink costs an F-16 on the rung whose subject is emission discipline — and NOT through the sort** | [MESS, `w1-07-emcon`, `--threads 1/2/4`, identical] `flt_src`/`flt_assign`/`sort_assign`/`eng_shots` are **0 in both** variants, so no assignment was ever made. With `dl=on` the wingman is killed at t = 338.2 and the run ends at 382 s; with `dl=off` both jets live to 600 s — `V = 3, M = 1` → **`V = 4, M = 2`**. The divergence chain is published and starts at t = 0.1: `dl_on`/`dl_xmt` → `dl_tracks`/`flt_mates`/`blk_datalink` (t = 30) → `rwr_brg` (t = 90) → trajectory (t = 150). The net is not audible (`DatalinkXmt` has no `FBEmitterSignature` and only `FBDatalinkSystem` reads it), so what moved is the FLIGHT GEOMETRY and not the picture. `net-off` improves 4 cells and worsens 10 | [`formation.md`](formation.md) — the station-keeping path, not F2's switch instability | passes X3; **what it is not is a sort finding**, and the numbers say so |

| **X-5** | **The search bought a COIN, and neither the fitness nor the gate can see it.** [MESS, `E8`] the first evolution to run on a passing arena produced a champion whose advantage on `w1-09-lfe-four` is `C +395.6` against the yardstick baseline's `+270.4` at an identical `(V, M)`. That cell's outcome class flips on **8 of 8** spawn-longitude perturbations of ±3 m under a genome that sets one unrelated gene, and on 3 of 8 under the champion — §5's own noise floor is 2 of 8. `o3-10-october-six` flips on 3 of 8 in both. So on two of the three cells the fitness paid for a lottery, and it is `E5`'s *"a lucky trajectory dressed as a doctrine"* caught in the act rather than predicted. **The gate certified both cells**: they passed S1 and S2, and `w1-09-lfe-four` carried the most movers of the entire campaign breadth (8 of 21) — the contamination is largest exactly where the gate likes a cell most | the spawn, i.e. no channel at all: the class moves with the initial condition and nothing the pilot did | this file — S1/S2 measure sensitivity to DOCTRINE and cannot distinguish it from sensitivity to ANYTHING (`E-21`) | **FAILED by construction** — there is no chain to name, because the mover is the initial condition. §5's inverted burden applies and the champion was NOT published |

**Round `E16`, on the three measuring rigs.** Three more, and the first of them is the largest single
defect this instrument has surfaced: a four-ship that never fires.

| # | What the search found | The channel it rides, with its number | Owner | X3 |
|---|---|---|---|---|
| **X-6** | **The emission gate LATCHES on a report nothing can refresh, and a flight that latches never fires a shot** | [MESS, `E16`, `sat-02` baseline, `pb1`] `EmconSilent_ = other && nearestM > radiateM` (`FBPilot.cpp:1523`). `fcr_on` is 1 until t = 57.4 s and **0 for the next 462.5 s** — 574 of 5 200 ticks, **11.0 %**. The latch closes one tick after the flight's own first detection (`fcr_contacts` 0 → 4 at t = 56.0), and at t = 57.5 `flt_src` falls to 0 and stays there: **the picture layer counts zero sources for 462 s while the emission gate keeps reading one.** The report's range, bracketed by the gene itself (silent at `f = 1.30` = 96.3 km, radiating at `f = 1.35` = 100.0 km, the same bracket on all three rigs), does not leave a 3.7 km window while the geometry closes **220 km**. Consequence: `fcr_lock` **0 of 5 200 ticks**, **0 of the run's 18 `sms LAUNCH_SOLUTION` lines are blue**, `eng_shots` = 0 on all six F-16, `kill unit` **0 of 8 met**. Six F-16 with 4 × AIM-120 each, master arm armed, `task intercept`, 520 s against eight MiG-29 — **and not one missile leaves the rail.** Break the latch through EITHER of two independent genes and the same thing happens: `f ≥ 1.35` ⇒ `fcr_on` 100 %, `fcr_lock` 464, **2 of 8** kills, M 8 → 10; `dl=off` ⇒ 100 %, 543, the same 2 kills, the same key | [`pilot.md`](pilot.md) / [`duels.md`](duels.md) D3c — `FBPilot`'s EMCON block and `FBFlightPicture` read the same `state.Datalink` and disagree about whether a picture exists. **Nothing in `sim/src/` was touched this round**; a fix is a round of its own, and it must not be argued from a better mission result (principle 1) | **PASSES as a chain** and is filed here anyway, because the chain explains a DEFECT. §5's verdict is mechanical: X4.2 failed on `sat-03`, so this is an exploit finding. Publishing "radiate more" as doctrine would freeze a latch into a tuning key — `E14` §7's reasoning, applied a second time |

**`X-6` REPAIRED 2026-08-03, and the mechanism named above is REFUTED by measurement.** The SYMPTOM is
exactly as filed — six armed F-16, 520 s, not one missile off the rail. The CAUSE is not the gate
expression and not a report that nothing refreshes. A probe on `EmconSilent_` itself, logged every
decision tick on the same rig and the same binary, measures **10 silent ticks of 5,200 (0.19 %),
t = 57.0…57.9 s and nowhere else**, while the radar is off for **4,626 of 5,200 (89.0 %)**: the gate
closes, the mate's `Engaging` report expires one net cycle later exactly as designed, the gate opens
again at t = 58.0 — and the radar never comes back. **No ageing threshold is needed; the report already
ages.** The latch is one level down, in the HAND: `pilot/FBPilot::InterceptCockpit` guarded both
`RadarMode` posts with `state.Radar.H.Readable()`, and `FBRadarSystem::Run` invalidates that head the
moment nothing radiates — so the only path back to radiating was gated on the picture that going silent
removes. The `flt_src = 0` half is likewise not a contradiction: `FBFlightPicture::Assign` returns
`None` without OWN echoes to correlate a mate's point against, which a silent jet has none of. Both
layers read `state.Datalink` and agree; they answer different questions.
Repair, as an invariant rather than a guard swap — *every emission state the pilot ENTERS he must be
able to LEAVE*: `core/FBAvionicsBlocks.h` gives `FBRadarBlock` the `Powered` readback its two nearest
siblings already carry (plus the `SetAbsent()` the module's no-set branch owes it), the EMCON post asks
that readback instead of the picture, and `IntEmconSilenced_` keeps him taking back only HIS OWN
silence — a `set fcr_mode off` is a decision above him, and a first cut without that clause powered up
four missions whose premise is a briefed-quiet radar (`bvr-defend`, `bvr-defend-blind`, `damage-amraam`,
`o5-04-no-radar`). Derivation and the before/after table:
[`pilot.md`](pilot.md) §7.6b, [`core.md`](core.md) §1.1a. Ground truth on this rig, before → after:
`fcr_on` 11.0 % → 87.3 %, `fcr_lock` 0 → 18 ticks, blue `LAUNCH_SOLUTION` 0 of 18 → 4 of 12, `eng_shots`
0 → 1 per sweep member, named `kill unit` bits **0 of 8 → 2 of 8**. That 2 of 8 is the same outcome key
both bypass genes reached, which is corroboration and **not** the argument — the argument is the guard.
**Two consequences this file owns:** (1) the fixed run ends at t = 248.6 s on a hostile CFIT instead of
flying the full 520 s, so `sat-02`'s S7 chaos-immunity claim rests on a baseline that no longer exists
and has to be re-flown before the rig is used again; (2) `emcon-tight`/`emcon-mid` were measured INERT
(`E16` §6(b)) against a seed whose radar was latched off — the three-valued STEP reading of
`pilot_emcon_frac` is a measurement of the defect and must be re-taken.
| **X-7** | **The first wreck to reach the ground sets the clock for everyone, and a cell's better half can be nothing else** | `FBMissionSim::Conclude` states it in its own comment: *"a K.O. always ENDS the run but only DECIDES it when it was nobody's declared objective."* `ExpectedLoss` removes a loss from the VERDICT, never from the CLOCK — so a MiG that was named as somebody's `kill unit` still truncates the run when its wreck penetrates the ground. [MESS, `E16`] on `sat-03` **6 of 6 improving levers extend the run** (301.7 s → 520.0 s on five of them, +218.3 s) and X4.2 independently kills the same claim: the candidate goes (26, 24) → **(23, 22)** at `timeout × 1.5`. On `sat-01` the detector applied to the BASELINE gives (32, 23) → **(30, 22)** — the cell's own reference point is a function of the clock. `sat-02` holds on both. **A committed claim needs correcting**: `sat-02`'s header argues its immunity from *"the baseline run reaches its full 520 s timeout"* — true, and insufficient, because **10 of its 12 movers do not**, and it is the COMPARISON that carries the verdict. The rig is left untouched so this round's numbers stay reproducible; the correction is owed to the file | `missions/FBMissionSim.cpp` + the three `sat-*` headers | **FAILED** — an advantage against the clock has no chain to an opponent. Same class as `E15`'s refutation of `w3-09-saturation`, now measured on a rig built to be immune to it |
| **X-8** | **A gene can act on every grid point and still carry no direction** | [MESS, `E16`, 3 × 10 runs] `pilot_flight_stack_frac` moves the outcome class on **9 of 9** non-seed grid points of `sat-02` while the 0.8 m spawn grid moves it on **0 of 8** — so it is not noise, it acts. Its SIGN along the ordered grid is − + + + · + − − − + : **four sign changes**, and the teeth do not agree across cells (`sat-01` better at {1.25, 1.5, 2.5}, `sat-03` the near-inverse in the lower half). The coarse three-allele file reported it as 2 : 1 and second-best of the round. **The screen this asks for costs 10 runs per candidate gene and did not exist**: a gene may only be published as a direction if its response is monotone over its own band on the cell the claim rests on | this file — §5's four instruments all test a POINT in genome space; none tests whether the gene's response is a function at all | **FAILED by construction** — there is no direction to name a chain for |

**And the one lever that moves the campaign breadth is a briefed contract on an aircraft with no net.**
`sort-near` improves the outcome class on **12 cells** and worsens it on 7; on `o5-01-cap:mig29` it
takes `M` from 2 to 4 with `eng_shots` 1 → 4 and `sort_assign` 2 → 6. It is the strongest genuine
doctrine signal in 2,464 runs — and every cell it moves failed S1, so §6 forbids publishing it as a
shift. It is the first thing a passing arena should be pointed at.

**Round `E18`, while building the three cells that took the arena to five.** No evolution ran, so these
are what the BUILDER hit — but each is a lever a search would find before a doctrine, and the first two
were caught only because a margin was re-derived from a second channel and disagreed with the first.

| # | What was found | The channel it rides, with its number | Owner | X3 |
|---|---|---|---|---|
| **X-9** | **`stores IMPACT lat=/lon=` is `%g`-printed, i.e. quantised to 9.3 m at 44 E, and a lattice derived from it is coarse by a factor two** | [MESS, `E18`, `sat-07`] the three bias alleles print impacts at lon 44.3004 / 44.3006 / 44.3009 — apparent steps 18.6 / 27.9 m — while the same three drops read **11.77 / 35.46 / 59.16 m** of `aimLongM`, i.e. an exact 23.7 m step. Aim points placed on the printed midpoint left `bias-early` at **48.05 m** from the target against the Mk 82's 45 m radius: a 3.0 m margin where 21.7 m was claimed, and `bias-early` a mover on all three new rigs. Re-derived from `damage DAMAGE … bodyFwdM/bodyRightM`, which carries full resolution, the same aim points converge to **0.0 m** baseline miss and **±23.7 m** rails | [`weapons.md`](weapons.md) — the full-resolution channel exists beside the coarse one, and nothing says which to read | **passes as a mechanism**; it is a defect of the published channel, not of the physics |
| **X-10** | **The CCIP impact follows the DESIGNATED TARGET with a gain below 1, so "put the target where the bomb lands" is a fixed-point iteration and silently wrong as a one-shot** | [MESS, `E18`] with the target on the steerpoint, `bias-late` lands at `aimLongM` 83.6 m; with the target moved to 60.4 m it lands at 59.16 m. One iteration of `target ← mean(early, late)` converges (largest second step **0.0 m** on all four rigs, 8 aim points) | [`pilot.md`](pilot.md) §5 attack pass | **passes**; it is why §2 of the `E18` State section iterates instead of placing |
| **X-11** | **A level-C-only effect reaches `p = 0.031` over five paired cells and is indistinguishable from a doctrine shift in the published order** | [MESS, `E18`] `bias-late` is worse on **5 of 5** informative cells under V→M→C domination, and `-....` (W0 L1 T4) at the outcome-class level: the whole result is `C_aim`, the 23.7 m the aim point moves. §6 lists *"a rank change inside level C"* first among things that are expressly not a finding, and **nothing in the tooling enforces it** | `C_air`/`C_aim` in the channels file; `fb_fitness.unit_key` | this file | **FAILED by construction** — there is no doctrine chain, only an aim-quality scalar. Not published, and named here so the next round cannot publish it either |
| **X-12** | **S7 passes a cell whose bias lever sits 3 m from a lethal threshold.** The chaos screen perturbs the SPAWN by ±3 m; it cannot see a threshold the GENOME sits 3 m from, and it certified all three new cells 0 of 8 while X-9 was still in them | `fb_campaign_arena.chaos_flips` — 8 samples of the initial condition, on the BASELINE genome | this file — S7 measures robustness to the initial condition, not margin to a threshold | **FAILED as a criterion**: it is a screen for one failure mode and was read as a screen for another. What excluded X-9 was the MARGIN, computed by hand from a second channel |
| **X-13** | **The outcome class is a SUM, so an instrument can be blind to a lever that moves every quantity it measures** | [MESS, `E18`] four drafts had **10 live levers and 2 outcome classes**: ten alleles each flipped one bit, all ten produced the same COUNT, and S1 read 60 % modal with the cell fully responsive. The repair is a monotone RUNG LADDER on one continuous quantity, which turns it into a count the class can see — `sat-08`'s `qa4 → an3` ladder alone then resolves **nine** alleles | `fb_fitness.side_key`, the `M` column | this file | **passes as a mechanism**; it is a property of the fitness, and it bounds what any cell can ever show |
| **X-14** | **`emcon-wide` is the identity map wherever the seed already radiates continuously** — with a live net and no threat the far rail IS the seed | [MESS, `E18`, `sat-08`] `emcon-wide` (`pilot_emcon_frac` = 3.0) is bit-identical to the baseline in all 28 published channels, while `emcon-mid` and `emcon-tight` move `M` 27 → 34 and 27 → 35. Same class as `E16` §6(b), on a different rig and after `X-6`'s repair | [`duels.md`](duels.md) D3c | **passes as a mechanism**; it means a three-point emcon sweep measures two points on any cell with a live net and no threat |


### Rejected before being tried, with the reason

| Approach | Why |
|---|---|
| **Keeping the weighted sum and re-tuning `W_HIT` to 0** | it removes Exhibit A and Exhibit C and leaves Exhibit B untouched — craft would still overturn outcome, because a sum has no "never". The defect is the FORM, and the tree's own precedent is that an invariant is defended structurally, not by choosing a bigger number (`FBSystemHealth`: private mutators, one friend, self-healing does not compile) |
| **Weighting the nine objective kinds against each other at level M** | nothing in the tree says an `identify` is worth more or less than a `suppress`, and a ladder invented here would be exactly the exchange rate §1.2 refuses. A COUNT needs no ladder |
| **Averaging the lexicographic key over runs** | a mean of ranks is a number without a currency, and level V's mean is almost never exactly equal, so M and C become dead code. §1.4 design A, rejected in favour of pairwise domination |
| **A random archive sample** | `conventions.md` forbids randomness in this simulator, and a sampled fitness would make a generation irreproducible. The stride rule (§3.3) is deterministic and covers the same history |
| **Evolving on the campaign runner directly** ([`missions/campaign.md`](missions/campaign.md)) | a campaign carries three monotone facts between missions, so a generation's runs would not be independent and a genome's score would depend on the order it was flown in. Evolution runs on single missions; a campaign is where a FINISHED doctrine is then flown |
| **Letting the evolution runner reach the deck writer** to "explore the model too" | it is principle 1. The deck path exists for the attribution instrument, where a deliberate, restored, logged perturbation is the measurement; an optimiser given that path would find the aeroplane it wants rather than the doctrine it needs, and `MODEL-DELTAS.md` would become a log of things nobody decided |

---

## Knowledge

### 1. The two fitness forms, side by side

| | present (weighted) | specified (lexicographic) |
|---|---|---|
| Shape | `Σ wᵢ·xᵢ`, one scalar | `(V, M, C)`, compared left to right |
| "Result dominates" is | a claim about the sizes of the weights | a property of the comparison |
| Falsified by | any new term, any growable count, any new arena | nothing that keeps the levels separate |
| Craft band | ±295, ≈ **0.49 kills** [DERIVED from the weights] | irrelevant by construction — C never crosses a level |
| Weight tuning | six numbers to defend, each with a paragraph | signs and boundedness only |
| Aggregation | mean over runs | pairwise domination → normalised win rate ∈ [0,1] |
| Behaviour in a saturated arena | manufactures a ranking out of craft noise | **honestly silent** — every variant ties, which is the signal to fix the arena |

That last row is the one that matters most for step 5: the reformed fitness cannot hide a dead arena,
and the present one demonstrably does (the symmetric F-16 tournament: 30 runs, **0 kills and 0 losses
across the entire field**, and a full ranking printed anyway — `longshot` 308.7 > `baseline` 278.1 >
`earlylock` 273.1 > `slowhand` 234.6 > `latelock` −101.9 > `deepshot` −126.5, carried by craft terms
alone [MESS, [`duels.md`](duels.md)]).

### 2. The craft band, computed

Items and their extremes, off `fb_tournament.py`'s own weights:

```
quality  [0, +100]        support [0, +80]        lead  [−40, +40]
defence  [0,  +40]        energy  [0, +40]        rounds −25·n        no shot {0, −250}

max C = 100 + 80 + 40 + 40 + 40           = +300     (n = 0 is unreachable with a quality term; at n = 1: +275)
min C = −250 − 40                          = −290
span                                       =  590  =  3.93 · W_HIT  =  0.49 · W_KILL
```

A tie-breaker worth half a kill is not a tie-breaker. Under the lexicographic order the same items are
kept and the same arithmetic is computed — the difference is that the number is only ever consulted
after two units have proved identical results.

**The second currency (`E6`), and why it does not widen that band.** `aim = 100 · mean over deliveries
of 1/(1 + e/10 m)` lives in a component of its own, so the bound to check is the ENCODING's and not a
tie-breaker's: `air ∈ [−190, +300]` and `aim ∈ [0, +100]`, so the projection `air + aim` stays inside
±400 < the 1e3 step between M levels and `order_scalar` keeps its property (D10). The half-quality
distance of 10 m is [SET] and, unusually, **not load-bearing**: `1/(1+e/h)` is strictly decreasing for
every positive `h`, so no choice of `h` creates or destroys an order between two deliveries — it only
decides where the curve is steep. It is put at the bottom of the band X-3 measured as reachable
(a residual floor of 10–22 m on all eight strike cells) so that the errors this tree can actually fly
sit on the steep part. **The aggregation is a MEAN and not a sum** — a sum would pay per delivery, i.e.
per store the mission author hung on the aircraft, which is Exhibit C's defect in a second currency.

### 3. Why the outcome constant of a saturated cell is readable

`−1450.0` is not an opaque score. Every item of `Score()` is printed, and only two of them are of that
magnitude, so the pair is forced:

| term | value | what it says |
|---|---|---|
| `lost` | −1200 | shot combat-ineffective |
| `no shot` | −250 | never fired a round |
| residue | **0** | either `shot lead` −40 (`tanh((t_foe − duration)/15) → −1`: the opponent shot, it never did) against `energy` +40 (`es_min/es_start` clamped to 1 — it died at its entry energy, never having manoeuvred), or neither item fires. Both readings say the same thing |

Eighteen runs of nineteen produce that identical total. **A score whose items are printed is a
diagnosis; a score that is only a total is a number** — which is the second reason the reformed fitness
keeps the itemisation even though the levels do the deciding.

### 4. The cyclic-triple count, derived

For a tournament graph on `n` champions with score sequence `s_i` (wins per champion), the number of
3-cycles is

```
d = C(n,3) − Σ_i C(s_i, 2)
```

because every triple of vertices is either transitive or cyclic, and a transitive triple has exactly one
vertex that beats both others — so the transitive triples are counted exactly once by `Σ C(s_i,2)`,
which counts, per vertex, the pairs of opponents it beats. `T = d / C(n,3)` is therefore the fraction of
triples that circle: **0 for a totally ordered field, 1/4 in expectation for a coin-toss field**. It
needs no new runs beyond the champion round robin and it is exact, not estimated.

### 5. Run-cost arithmetic

| Item | Formula | At `P = 12`, `k = 8`, both seats |
|---|---|---|
| Population round robin | `P(P−1)` | 132 |
| Archive sample | `2·P·k` | 192 |
| Fixed-yardstick check (champion only) | `2·6` | 12 |
| **Per generation** | | **336** |
| Arena check (once per arena) | `6 geometries × (9 doctrine + 9 deck + 1 control)` | 114 |
| Archive reset (after a simulator fix, `N` members) | `N(N−1)` | 4,032 at N = 64 |

The last row is the honest one: an archive reset is by far the most expensive event in this scheme, and
simulator fixes are the declared product. That is a reason to cap the archive at 64, and it is the
reason the cap is a number rather than "as many as we like".

### 6. The five genes against the two boundaries

| Gene | Pilot property, aircraft property, or neither | Expressed as |
|---|---|---|
| G1 `pilot_flight_shape` | neither — it is a FLIGHT decision | a mode selecting multiples of the row's own `FormationSpreadM` |
| G2 `pilot_cover_frac` | neither — a rule about the flight's trigger | a multiple of the weapon's own binding time (`ttaS`) |
| G3 `pilot_sort` | neither — a briefed contract | an enumerated contract + the channel bit |
| G4 `pilot_energy_frac` | touches an AIRCRAFT hook (`BfmCornerSpeedKt`, [MESS] 380 kt) | `Scale` — a fraction of that hook, never an absolute speed |
| G5 `pilot_emcon_frac` | touches a SENSOR number (the search gate) | `Scale` — a fraction of that gate, never an absolute range |

None of the five carries a mass, an area, a coefficient, a thrust or a drag. That is checkable by
reading five lines of a `constexpr` table, which is the reason the table is where the boundary lives.

---

## Related

| Place | Relationship |
|---|---|
| [`pilot.md`](pilot.md) | §9 the genome's home and its band table, §9.1 the tool being reformed, §11 the pilot/aircraft split this file's boundary rests on |
| [`duels.md`](duels.md) | the arena axes with their measurements, the mixed-tournament exhibit, and D3 which blocks G5 |
| [`formation.md`](formation.md) | three of the five genes, and the exhibit that names the present fitness an artefact against its own round |
| [`modules/air/module.md`](modules/air/module.md) | §Spec 11 the attribution instrument this file consumes unchanged, §State B6 the saturation blocker |
| [`missions/verdict.md`](missions/verdict.md) | the four verdicts and the nine objective kinds that levels V and M are built on |
| [`missions/campaign.md`](missions/campaign.md) | the fingerprint-and-refusal discipline the archive reuses verbatim; where a FINISHED doctrine is flown |
| [`campaigns/INDEX.md`](campaigns/INDEX.md) | the Bekaa yardstick (`band`, `residue`) — the same measurement one level up, and the reason the arena check comes before the campaigns |
| [`weapons.md`](weapons.md) | §3.1 the bundle, and the full-drum run that Exhibit C rests on |
| `sim/tools/fb_fitness.py` | the fitness itself — the ONE scorer the tournament, the arena gate and the evolution runner all import. `compare_craft` is the craft level's own comparison (domination between the air and the aim currency, `E6`) |
| `sim/src/missions/FBMissionRunner.cpp` | where a run ENDS and where its judges FINISH — two different things since `E6` ([`missions/runtime.md`](missions/runtime.md) §5, step 3) |
| `sim/tools/fb_arena_check.py` | §4's gate, and the tool that measured the old arena into a refusal |
| `sim/tools/fb_evolve.py` | §2/§3's runner: the genome out of `fb-gym --pilot-keys`, the archive, the three circling instruments |
| `sim/tools/levers-merge.txt` | the merge phase's OWN nine lever points — the declared nine are all intercept keys and not one of them is read in `BfmCommands` |
| `sim/tools/levers-genome.txt` | the three live genes as a lever set: the file that turns E-12 from a note into a gate run |
| `sim/tools/fb_campaign_arena.py` | §7's gate on a COMMITTED mission: the genome spliced into a copy, one cell per `(mission, team, module)`, the run read and pruned in the worker and appended to a resumable CSV |
| `sim/tools/fb_campaign_evolve.py` | §7's runner — the cell in the seat's place, the key cache that makes the archive free, and the grid poll of D8 |
| `sim/tools/fb_campaign_exploit.py` | §5's X1/X4a/X4b/X4c on a campaign cell; X2 is `n/a` for D2's reason |
| `sim/tools/arena-campaign.txt` | the 154 cells, generated by a stated rule and not curated |
| `sim/tools/levers-campaign.txt` | the genome's alphabet on a campaign rung: 15 points, both values of the channel bit, and G6's three points scaled to one DECISION TICK rather than to its band |
