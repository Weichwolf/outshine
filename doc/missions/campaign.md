# The campaign layer — `.fbc` and the aggregating runner (`C0`)

**Status: specified here, nothing built.** Written as the contract a build round has to satisfy, per
[`../conventions.md`](../conventions.md)'s spec-first rule. It mirrors `sim/src/missions/` like every
other file in this directory: the parser would be `core/FBCampaignFile.h` beside `core/FBMissionFile.h`,
the runner `missions/FBCampaignRunner` beside `missions/FBMissionRunner`.

**The problem, in one line.** Ten `.fbm` files are ten unrelated runs
([`../campaigns/INDEX.md`](../campaigns/INDEX.md), gap `C0`): nothing carries a loss, a destroyed target
or an expended missile from one to the next. All ten campaign specs name it; O1 states it sharpest —
*the SAM belt destroyed on mission 1 is intact on mission 2.*

---

## Spec

### 1. What a campaign IS, and what it deliberately is not

| It is | It is not |
|---|---|
| an ordered list of existing `.fbm` files | a new mission dialect. **No `.fbm` changes to be part of a campaign** |
| plus a **carry** of three monotone facts between them | a force-structure model. It attrites the named cast; it does not manage a squadron, a replacement pool or a maintenance queue |
| plus an aggregating verdict | a single collapsed exit code. A campaign reports numbers, and its exit code is a coarse summary with a stated reading rule |
| **a sequence of ordinary runs** | a new kind of run. That is the acceptance criterion, §5 |

### 2. The file

```
name o5-airfield-defence                  # mandatory
time 1999-03-24T22:00:00Z                 # optional: the campaign clock (C2)
carry units ground stores                 # optional; default = all three, may only be NARROWED
stop_on never                             # optional: never (default) | fail | crash
mission missions/o5-01-alert.fbm          # ordered, ≥ 1, relative to the campaign file
mission missions/o5-02-airborne.fbm
```

Same line discipline as `.fbm`: one statement per line, `#` to end of line, blank lines ignored, an
unknown keyword is a parse error. `FBCampaign = Name + UtcT0S/HaveTime + CarryMask + StopOn + Missions`.

| Keyword | Rule |
|---|---|
| `name` | mandatory, names the output directory and every log line |
| `time` | optional. Applies to every mission that declares **no** `time` of its own. It does **NOT advance** by mission duration — see §6 |
| `carry` | optional subset of `units`, `ground`, `stores`. Default all three. **Narrowing only** — there is nothing to widen it to, and a campaign that could invent a carried quantity would be inventing state |
| `stop_on` | `never` (default) / `fail` / `crash`. Default `never` because attrition *is* the subject: O1 and O5 ask what an air force looks like after losing, and stopping at the first loss deletes the arc |
| `mission` | ordered, at least one. A file that does not parse is a campaign parse error, checked for **all** missions up front — a campaign that dies at step 7 on a typo has wasted six runs |

### 3. The carry — three things, and a rule for every omission

The test applied to every candidate is the same, and it is the interesting part:

> **Is the quantity (a) monotone, (b) already observed by the runner, and (c) expressible in a
> declaration the `.fbm` format already has?** A carried value with nowhere to land is not state, it is
> a number in a file.

| Quantity | Carried | Reason |
|---|---|---|
| **Units destroyed** | **yes** | this is the campaign. One bit per callsign, already the roster's `CombatEffective`, already monotone. Lands as: the unit's block is dropped from the next mission |
| **Ground targets destroyed** | **yes** | O1's belt, W3's arc, O5's shelters. Same bit, same source, same landing |
| **Stores expended** | **yes** | O5's anchor is explicitly *"a limited stock of R-73/R-27"*; W2's is fuel and range. One integer per station, already in `FBStoresBlock`. Lands on the `set store …` lines the mission already declares |
| **Airframe damage below the kill threshold** | **no** | two independent blockers. (a) There is no repair or turnaround model, so a degraded jet would stay degraded for ever — that is not attrition, it is a slow death with no mechanism. (b) `C21`: a `.fbm` cannot declare initial damage, so the value would have **nowhere to land**. Named as the first extension once either is closed |
| **Fuel** | **no** | a sortie starts from a base with a briefed load. Carrying fuel across a landing models the *ground* time, which does not exist. `set fuel_pct` is the correct sortie-level abstraction. (Tanker state for W2/`C5` is a different quantity, not this one) |
| **Position / attitude / speed** | **no** | every mission declares its own spawn. Carrying the last position would make mission N+1's geometry depend on how mission N ended, destroying the one-variable-per-step discipline all ten campaign specs are built around |
| **Sortie count, crew fatigue, pilot experience** | **no** | no model exists, and `vision.md` forbids inventing one |
| **Score** | it is an **output**, not carried | §4 |

**A lost unit is dropped, not replaced.** If a campaign needs a fresh aircraft it declares a different
callsign in the next mission file. The alternative — a replacement pool — is the force-structure model
§1 refuses. Consequence, stated: O5's five MiG-29s over three nights are modelled exactly; W3's
ten-night arc with a resupplied wing is **under-modelled**, and its campaign file must say so in its
header.

### 4. How the carry is applied — the overlay, and the rule that keeps the mission file authoritative

Two designs, and the recommendation takes one mechanism from each:

| | in-memory overlay only | **explicit state file + logged overlay** — recommended |
|---|---|---|
| Mechanism | the runner mutates the parsed `FBMission` before spawning | the runner writes `campaign-state.txt` after each mission and applies it to the next |
| Reproducibility | a run is no longer reproducible **from the file alone** — the `wx` objection, verbatim | a single mission can be re-run standalone by handing it the same state file |
| Auditability | the effective mission exists only in memory | the state is text: diffable, greppable, part of the fingerprint |

**The binding rule on what an overlay may do:**

> The overlay may **delete a `unit` block** or **change the value of a `set` line the mission file
> already contains**. It may **never add a line**. If a mission declares no loadout for a unit, the
> campaign cannot inject one.

Because the moment the overlay can add, the `.fbm` stops being the statement of what was flown, and
[`INDEX.md`](INDEX.md) rule 5 (the header comment is a binding reading rule) becomes false. Every
applied change is logged (`campaign CARRY unit=… action=drop|stores`), so the effective mission is
reconstructible from `events.log` without re-deriving it.

**The state file** is a canonical text artefact, ordered by mission declaration order — never by hash or
filesystem order, because it is part of the fingerprint:

```
# campaign-state after mission 03
unit knight1  destroyed
unit knight2  alive  stores r27r=1 r73=2
ground shelter1 destroyed
ground shelter2 alive
```

### 5. The core requirement — replayable, repeatedly, deterministically

The owner's requirement is that a campaign be playable **more than once** and give the **same** result.
The tree already owns the instrument: the per-run fingerprint is SHA-256 over all `telemetry*.csv` + the
normalised `events.log` + the exit code, invariant over `--threads 1/2/4` × repetitions
([`runtime.md`](runtime.md)).

**Campaign fingerprint** = SHA-256 over, in campaign order: each mission's own fingerprint, each
`campaign-state.txt` after it, and the campaign exit code.

Two acceptance criteria, and the second is the one that matters:

| # | Criterion |
|---|---|
| **1** | A campaign of N missions run **3× at `--threads 1`, 3× at 2 and 3× at 4** produces **one** campaign fingerprint. Nine runs, one hash |
| **2** | **The per-mission fingerprint of mission *k* inside the campaign equals the fingerprint of that same mission run STANDALONE with the state of step *k−1* supplied as input.** |

Criterion 2 is the statement that *the campaign layer adds no hidden state*. If the standalone re-run
diverges, the layer has leaked something into the mission, and the diff names the file and the column.
It is also what makes debugging a campaign tractable: any step is a normal `fb-gym --mission` run.

**Three named ways determinism can be lost, each with its rule:**

| Hazard | Rule |
|---|---|
| **Wall-clock leakage** | a campaign must never read the host clock. This is where `C0` and `C2` meet: a campaign whose missions declare no `time` and whose client falls back to the wall clock is reproducible only **within one day**. The runner therefore **warns at start** when neither the campaign nor a mission declares a clock, and a campaign meant as a measurement declares one |
| **Output ordering** | per-mission output directories are `NN-<missionname>/`, index-prefixed, so the fingerprint's file order is the campaign's order and never the filesystem's |
| **State ordering** | the state file is written in mission declaration order, canonically formatted, one fact per line |

### 6. The clock does not advance with the campaign

Each mission's clock is the campaign clock unless the mission declares its own; the campaign clock is
**not** incremented by the elapsed sim time of the preceding missions. A campaign spanning three nights
says so with a `time` line in each mission file.

The reason is the same one that keeps the overlay from adding lines: an advancing campaign clock would
be a calendar the mission files cannot see, and a mission's declared instant would silently mean
something else depending on which campaign ran it. One sentence of rule beats one class of bug.

### 7. What the runner emits

| Line | Content |
|---|---|
| `campaign CAMPAIGN_START` | name, mission count, carry mask, `stop_on`, the clock (or the warning that there is none) |
| `campaign CARRY` | per applied overlay change: unit, action, the value |
| `campaign MISSION_RESULT` | index, file, exit, result, reason, decisive unit, output directory |
| `campaign ATTRITION` | at the end: units lost per team, ground targets lost per team, stores expended per type |
| `campaign CAMPAIGN_RESULT` | missions run / succeeded / failed / timed out / crashed, and the exit code |

**The exit code is the worst mission's code** (0 SUCCESS, 1 FAIL, 2 CRASH/LOC, 3 TIMEOUT), and it
carries the same reading rule combat missions already carry: *a campaign of measuring rigs is expected
to be non-zero, and its verdict is the `ATTRITION` and `MISSION_RESULT` lines, not the code.* Collapsing
ten runs into one 0/1 would be exactly the invented single verdict O5 refuses.

---

## State

**Nothing built.** No `.fbc` file exists, no parser, no runner. What exists and carries this layer, all
of it unchanged:

| Piece | Why it carries |
|---|---|
| The per-run fingerprint over telemetry + normalised log + exit code | the campaign fingerprint is a concatenation of it |
| Determinism over `--threads 1..4`, measured on four missions × 5 repetitions | criterion 1 is that measurement, one level up |
| The roster (`FBUnitObservation`: id, team, the one health bit) | two of the three carried facts are that bit |
| `FBStoresBlock` (`Station[12]`, loaded count, `ReleasedCount`) | the third |
| `set store …` and the `unit` block as declarations | the two places the carry lands |
| `FBRunMission` as a pure four-step orchestrator taking a path and returning a result | a campaign runner is a loop over it, not a second engine |

## Gaps

| Gap | Detail |
|---|---|
| The whole layer | see Spec. All ten campaign specs list `C0`; it degrades ten and blocks none, which is why it is the last of the four foundation contracts and not the first |
| Damage does not carry (`C21`) | double-blocked: no repair model, and no `.fbm` syntax to spawn a damaged jet. The first extension once either closes |
| No replacement pool | a lost unit is dropped. Correct for O5, under-modelled for W3/W4's multi-week arcs — their headers must say so |
| No campaign-level objective | the campaign verdict is an aggregate of mission verdicts; there is no way to declare "the belt is down by mission 5". Deliberately out of this round: it would need a campaign-scope objective vocabulary on top of `C12`'s, and one vocabulary at a time |
| The tournament runner is a separate mechanism | `sim/tools/fb_tournament.py` already sweeps missions × variants and aggregates. It is **not** a campaign — no carry, no order, no state — and the two must not be merged: one measures a pilot over independent geometries, the other measures a force over a dependent sequence |

## Knowledge

- **Why the campaign is a list of unchanged `.fbm` files rather than a new format with inline missions.**
  Every mission in the tree carries a binding header comment stating what it proves ([`INDEX.md`](INDEX.md)
  rule 5). Inlining would either duplicate those files or delete the rule. It also keeps criterion 2 of §5
  meaningful: a campaign step must be re-runnable standalone, which is only possible if the step is a file.
- **Why the carry set is exactly three.** The three-part test in §3 (monotone, already observed, has a
  declaration to land in) admits exactly these and rejects the rest for reasons that are properties of
  the tree rather than opinions — fuel has no turnaround, damage has no `.fbm` spelling, position would
  destroy the experimental discipline.
- **Why `stop_on never` is the default.** Two of the ten campaigns (O1, O5) are explicitly about what a
  force looks like after it has lost. A runner that stopped at the first FAIL would answer the question
  by refusing to run the part that contains the answer.
- **Why the campaign exit code is the worst mission's and not a computed score.** A score needs weights;
  weights are `[SET]` numbers that would silently decide what a campaign "means". The aggregate lines
  carry the raw counts and the analysis lives in a tool, exactly as `fb_duel_report.py` does for a duel.
