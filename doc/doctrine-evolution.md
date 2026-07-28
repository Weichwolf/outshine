# Doctrine evolution — fitness, genome, archive, arena

**Subject:** step 3 of the owner goal. What the evolutionary tournament **optimises** (the fitness), what
it is **allowed to change** (the genome), what stops the co-evolution from **circling** (the archive),
and what an arena must be before any of it measures anything (the **saturation criterion**).

**Status: SPEC ONLY. Nothing in this file is built.** No line of `sim/` was touched to write it. The
tool it specifies (`sim/tools/fb_evolve.py`) does not exist; the tool it *reforms*
(`sim/tools/fb_tournament.py`) does, and every claim below about the present fitness is read off that
file or off a committed measurement.

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
evolution round. C is also **latent**: the tournaments that produced A and B are BVR and fire no gun,
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

## State

**Nothing of this file is built.** What exists is the instrument it reforms and the measurements it
rests on.

| Piece | Status | Anchor |
|---|---|---|
| The weighted fitness (`Score()`, nine items) | **built, and this file is the case for replacing it** | `sim/tools/fb_tournament.py` |
| Both-seat pairings, mirrored geometries, `--check-determinism` | **built and consumed unchanged** — §1.4 design C uses exactly this structure | [MESS] 0 of 60 files differ between `--threads 2` and `--threads 1` |
| The genome as mission text (`FBPilotTuning`, 18 keys, band-checked, rejection ⇒ mission FAIL) | **built** — §2's `Free`/`Scale` split is an extension, not a replacement | [`pilot.md`](pilot.md) §9 |
| Three of the five genes as mission text (`dl=`, `sort=`; the station as airframe hooks) | **partly built** — G1 needs F5 closed, G3 exists | [`formation.md`](formation.md) §§4–6, F5 |
| Two of the five genes (G4 energy rule, G5 EMCON timing) | **not built, and both are named as missing by the files that own them** | [`pilot.md`](pilot.md) rejected-approaches; [`duels.md`](duels.md) D3 |
| The attribution instrument (`band_deck`, `band_doctrine`, the control cell, the one-sided rule) | **built, repaired 2026-07-28** — §4 consumes it unchanged | [`modules/air/module.md`](modules/air/module.md) §Spec 11 |
| The saturation blocker | **MEASURED** — 18 of 19 runs identical, `band_deck` = 0.0 | [`modules/air/module.md`](modules/air/module.md) §State B6 |
| The fingerprint + refusal discipline the archive needs | **built** for campaigns and reusable verbatim | [`missions/campaign.md`](missions/campaign.md) §5 |
| Per-objective met/unmet publication (level M's input) | **NOT built** — see Gaps E-1 | `core/FBMissionMonitor::ObjectivesMet` is all-or-nothing |

---

## Gaps

| # | Thing | Known from |
|---|---|---|
| **E-1** | **The judge does not publish WHICH objectives were met**, only one verdict and a prose `reason`. `ObjectivesMet()` is a single bool; `FBIdentifyProgress` is the only per-objective state and it is private. Level M therefore has no input today. The bounded fix is one event at `Finalize` — `mission OBJECTIVE unit=… kind=… state=met\|unmet\|violated`, one line per declared objective, no new judge logic and no new column. **Its cost is stated rather than hidden:** it adds lines to `events.log` for every mission that declares an objective, so the "events.log identical" half of the regression gate moves by exactly that many lines and the diff must be shown | this file |
| **E-2** | **Exhibit C is derived, not read.** The bundle→`NoteHit` mapping is source-exact and the arithmetic is off published rates, but `dmg_hits` has not been read off `mig29-gun`'s last telemetry line. One read settles it, and it is an acceptance item of the first round | this file |
| **E-3** | **The −1450.0 decomposition is derived, not read** off the tool's printed items. The four terms sum exactly and the reading ("flew straight and level into a missile") follows from them, but the confirmation is one `--attribution mig21` re-run with the item list kept | this file |
| **E-4** | **G5 (EMCON) cannot be evolved before `duels.md` D3 closes.** The pilot's intercept picture is built from the Radar block alone, so a silent jet is a blind jet and the gene's band is degenerate at one rail | [`duels.md`](duels.md) D3 |
| **E-5** | **G1 (formation shape) cannot be evolved before `formation.md` F5 closes**, and a four-ship is four abreast until it does (`FormationTrailM` = 0) | [`formation.md`](formation.md) F5 |
| **E-6** | **G2's gradient is nearly flat on the only airframe that has the channel.** The cover rule is almost free for the AIM-120 (0.3 s) and unavailable to the MiG (no cooperative terminal, F3). The gene is therefore only measurable in a MIXED tournament, and its most interesting value is on an aircraft that cannot express it | [`formation.md`](formation.md) F3, [`duels.md`](duels.md) |
| **E-7** | **The archive's arena fingerprint has no definition yet.** The campaign fingerprint is SHA-256 over telemetry + normalised events + exit code; an ARENA fingerprint must additionally bind the binary and the elevation record, and nothing computes one today | this file |
| **E-8** | **Wall-clock cost is unmeasured.** The run COUNT is derived (324 per generation at P = 12, k = 8); the time is the arena timeout divided by the measured speedup and nobody has measured the speedup for a 420 s BVR duel | this file |
| **E-9** | **`flt_dup` counts sharing, not the violation** (the acceptance metric is `dup ∧ free > 0`, computed in the analysis tool). A fitness or a gene report reading the raw column will misread it | [`formation.md`](formation.md) F4 |
| **E-10** | **Level C's engagement gate has no channel for "committed but never locked".** `eng_shot_s ≥ 0 ∨ eng_lock_s ≥ 0` misses a pilot who pressed to the merge and never got a lock — which in this tree's stalemate arena is a real and defensible doctrine | this file |

### Exploits the evolution found

*(Empty — nothing has been run. This section is where §5's failures land, one row each: the champion,
the channel it rode, the file that owns the defect, and whether it was fixed or bounded.)*

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
