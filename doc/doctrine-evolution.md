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

## Gaps

| # | Thing | Known from |
|---|---|---|
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
| **E-17** | **The campaign breadth is REFUSED as an arena, and 89 of its 154 cells are not moved by the genome at all.** [MESS, `E5`] 0 informative cells under both the published `levers-genome.txt` and this round's 15-point file; 2 under the loosest reading the gate admits, which is W1's own verdict. **What would make a run possible, named so it can be checked rather than hoped for:** (a) a fitness that can order two strike doctrines — level C is `GATE` on 32 of the 46 cells that aim a bomb, so a bomb 20 m out and a bomb 2 km out are EXACTLY tied; (b) S1 needs a fixed field that acts on the cell it judges, and the only frozen one is six BVR intercept doctrines — a ground-flavoured yardstick would fix it on paper and is refused for `E2`'s reason; (c) G2 and G7 need an arena that does not exist in the campaigns at all (a long-binding round on a netted element; a CCIP delivery) | `E5`, [`campaigns/w1-red-flag.md`](campaigns/w1-red-flag.md) |
| **E-18** | **Level M cannot tell "judged and unmet" from "never judged".** Both read 0, and the difference is worth 8 points of M on `w4-10`. The judge publishes `OBJECTIVE` lines only from `Conclude`, and `FBMissionRunner` skips every `FinalizeMission` when a healthy unit's K.O. ends the run. Until that is closed, every campaign-cell fitness contains a term that is a property of the OPPONENT's airmanship | `E5`, X-1 |
| **E-16** | **A converged population and a circling one look alike in instrument (c).** The `E2` run's champion never changed, so "min distance to a champion 3+ generations back" is 0.0000 — the reading §3.6c gives a CYCLER. T = 0.0000 and the yardstick was flat, so this one is a fixed point; the instrument cannot tell the two apart on its own and the file now says so | `E2` |

### Exploits the evolution found

**Round `E5`, over the ten campaigns.** One is an exploit of the INSTRUMENT and rides no physics at
all; two are defects of the delivery chain that a genome value buys back; one is a doctrine effect
whose channel is published and whose reading is uncomfortable. Each row carries the channel, the
number and the file that owns the defect. **Nothing here was fixed** — this round did not touch
`sim/src/`, and a fix is a round of its own.

| # | What the search found | The channel it rides, with its number | Owner | X3 |
|---|---|---|---|---|
| **X-1** | **A healthy departure of ANY unit, on either side, deletes level M for the whole mission** — so a doctrine is paid for keeping the OPPONENT flying | `FBMissionRunner`'s loop ends at `FirstFlightKo`; `ExpectedLoss` forgives a K.O. only for a unit that is already combat-ineffective, so nobody `Conclude`s and **0 `mission OBJECTIVE` lines** are published. [MESS, `w4-10-allied-force`] baseline: `kamig4 LOC "stall/mush"` at t = 695.3 of 700 ⇒ eight F-16s at `V = 16, M = 0`. `net-off`, `bias-early` and `bias-rail` each keep it flying ⇒ **`V = 18, M = 8`**. `bias-rail` gets there by throwing its bombs **2,794 m** wide. **17 of 154 cells** have a lever that crosses the boundary, **5** baselines sit on the wrong side | this file (level M's input, E-1) + `missions/FBMissionRunner.cpp` | **FAILS** — the advantage has no chain to the opponent at all. An exploit by §5's inverted burden |
| **X-2** | **The pilot's pickle carries ~0.2 s of uncancelled chain latency, and a genome value buys it back** | `ATTACK_RELEASE biasS=0 leadS=0.6 ttrS=0.569` → `stores DELIVERY predErrM = 52.57 m = gs × 0.227 s` (W2 measured 0.228–0.241 s over seventeen drops) → `aimLongM 36.34` of `aimErrM 36.38` (99.9 % along) → `damage rangeM 33.66` against a Mk-84's 17.7 m. Swept on 8 strike cells over four campaigns, two stores and four altitudes, the minimum of `aimErrM(bias)` sits at **−0.20 ± 0.05 s on every one of them** — a constant TIME, not a constant distance, which is what says it is a latency. At −0.20 s, `w2-01-dome` 36.38 → **10.06 m**, the hardened dome is DESTROYED and `(V, M)` goes (2,1) → (3,2) | [`pilot.md`](pilot.md) §5 attack pass — `FBF16Pilot::AttackReleaseBiasS()` returns **0.0 s** | passes: chain named, number at every link. **Not an exploit — a defect of the default** |
| **X-3** | **The release time is quantised at the pilot's decision tick, and the quantum is wider than the weapon's own lethal radius** | the release cue is evaluated once per `DecisionDtS_` = 0.1 s, so `aimErrM(bias)` is a STAIRCASE: 36.38, 36.38, 13.29, 13.29, 10.06, 10.06, 33.12, 56.24 m over bias 0 … −0.40 in 0.05 s steps — pairs, one step per tick. At a loaded F-16's 231 m/s one tick is **23.1 m** of track against a hardened target's **17.7 m** radius, so the best reachable lattice point can be 11.5 m off with a perfect bias and the residual floor is 10–22 m on all eight cells | [`weapons.md`](weapons.md) / [`pilot.md`](pilot.md) §5.8 — the same partition class as Exhibit C, on the release clock instead of the gun bundle | passes as a mechanism; it is §5's **partition class** and is listed as one |
| **X-4** | **The cooperative datalink costs an F-16 on the rung whose subject is emission discipline — and NOT through the sort** | [MESS, `w1-07-emcon`, `--threads 1/2/4`, identical] `flt_src`/`flt_assign`/`sort_assign`/`eng_shots` are **0 in both** variants, so no assignment was ever made. With `dl=on` the wingman is killed at t = 338.2 and the run ends at 382 s; with `dl=off` both jets live to 600 s — `V = 3, M = 1` → **`V = 4, M = 2`**. The divergence chain is published and starts at t = 0.1: `dl_on`/`dl_xmt` → `dl_tracks`/`flt_mates`/`blk_datalink` (t = 30) → `rwr_brg` (t = 90) → trajectory (t = 150). The net is not audible (`DatalinkXmt` has no `FBEmitterSignature` and only `FBDatalinkSystem` reads it), so what moved is the FLIGHT GEOMETRY and not the picture. `net-off` improves 4 cells and worsens 10 | [`formation.md`](formation.md) — the station-keeping path, not F2's switch instability | passes X3; **what it is not is a sort finding**, and the numbers say so |

**And the one lever that moves the campaign breadth is a briefed contract on an aircraft with no net.**
`sort-near` improves the outcome class on **12 cells** and worsens it on 7; on `o5-01-cap:mig29` it
takes `M` from 2 to 4 with `eng_shots` 1 → 4 and `sort_assign` 2 → 6. It is the strongest genuine
doctrine signal in 2,464 runs — and every cell it moves failed S1, so §6 forbids publishing it as a
shift. It is the first thing a passing arena should be pointed at.

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
| `sim/tools/fb_fitness.py` | the fitness itself — the ONE scorer the tournament, the arena gate and the evolution runner all import |
| `sim/tools/fb_arena_check.py` | §4's gate, and the tool that measured the old arena into a refusal |
| `sim/tools/fb_evolve.py` | §2/§3's runner: the genome out of `fb-gym --pilot-keys`, the archive, the three circling instruments |
| `sim/tools/levers-merge.txt` | the merge phase's OWN nine lever points — the declared nine are all intercept keys and not one of them is read in `BfmCommands` |
| `sim/tools/levers-genome.txt` | the three live genes as a lever set: the file that turns E-12 from a note into a gate run |
| `sim/tools/fb_campaign_arena.py` | §7's gate on a COMMITTED mission: the genome spliced into a copy, one cell per `(mission, team, module)`, the run read and pruned in the worker and appended to a resumable CSV |
| `sim/tools/fb_campaign_evolve.py` | §7's runner — the cell in the seat's place, the key cache that makes the archive free, and the grid poll of D8 |
| `sim/tools/fb_campaign_exploit.py` | §5's X1/X4a/X4b/X4c on a campaign cell; X2 is `n/a` for D2's reason |
| `sim/tools/arena-campaign.txt` | the 154 cells, generated by a stated rule and not curated |
| `sim/tools/levers-campaign.txt` | the genome's alphabet on a campaign rung: 15 points, both values of the channel bit, and G6's three points scaled to one DECISION TICK rather than to its band |
