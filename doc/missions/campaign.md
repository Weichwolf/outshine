# The campaign layer — `.fbc` and the aggregating runner (`C0`)

**Status: BUILT 2026-07-28, exactly where the spec put it** — the parser in `core/FBCampaignFile.h`
beside `core/FBMissionFile.h`, the carried state in `core/FBCampaignState.h`, the runner in
`missions/FBCampaignRunner` beside `missions/FBMissionRunner`, driven by `fb-gym --campaign`. The Spec
below is unchanged and is what was built against; `## State` names the four places the implementation is
NARROWER or more explicit than the text, and both acceptance criteria of §5 with their numbers.

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

**A fingerprint is comparable only within ONE environment base.** It says "these two runs computed the
same thing", never "this is the campaign's number in the absolute" — the ground alone changes every
byte of it. The campaign therefore **records the ground it was flown over** beside its state
(`campaign-summary.txt`: `elev`, `swiss_dem`, `base`, `threads`), and every comparison READS that record
instead of assuming one. A comparison across two bases is not a divergence, it is a category error, and
a tool that guesses the base manufactures exactly that error. This is part of the contract because it
decides what criterion 2 below even means.

**Four named ways determinism can be lost, each with its rule:**

| Hazard | Rule |
|---|---|
| **Environment drift** | the elevation source (and, under `swiss`/`tiles`, the asset path resp. the server) decides every number in the run. It is WRITTEN DOWN by the runner; a replay reads it, and an output tree without the record is REFUSED rather than replayed against a default. `--threads` is recorded too, but is result-neutral by criterion 1; `--timeout` is deliberately not reachable for a campaign step — the step's timeout is its own file's |
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

**Built.** Four files, one CLI flag, one measurement tool, one example campaign.

| Piece | What it is |
|---|---|
| `core/FBCampaignFile.h/.cpp` | the `.fbc` parser — `name`/`time`/`carry`/`stop_on`/`mission`, same line discipline as `.fbm`, unknown keyword is a parse error |
| `core/FBCampaignState.h/.cpp` | `FBCampaignState` (the three carried facts as text) + `FBApplyCampaignCarry`, the overlay |
| `missions/FBCampaignRunner.h/.cpp` | the loop over `FBRunMission` and the aggregate report. **Gym-only**, like `FBTickPool`: it is not in `libfbcore.a` and never reaches wasm |
| `missions/FBMissionRunner` | grew ONE optional parameter, `const FBMissionCarry *` (state in, campaign clock, outcome out). Null = the run that existed before, byte for byte |
| `fb-gym --campaign FILE` | runs a campaign; `--mission FILE --state FILE [--carry LIST] [--campaign-time ISO]` runs ONE step standalone with the same overlay AND the same clock — the two halves of §5 criterion 2 |
| `sim/campaigns/viper-attrition.fbc` | the first campaign file: four existing missions, all three carried facts demonstrated |
| `sim/tools/fb_campaign_verify.py` | the instrument: `fingerprint` / `campaign` / `determinism` / `replay` |

**Output per campaign** (`--out DIR`): `NN-<missionfile>/` per step with the ordinary per-run files plus
`campaign-state.txt` (the state AFTER that step — so the state BEFORE step *k* is step *k−1*'s file, which
is exactly what `--state` takes), plus `campaign.log` and `campaign-summary.txt` at the root. The summary
carries the **environment record** (`time`, `elev`, `swiss_dem`, `base`, `threads`) the comparability rule of §5
requires; `FBCampaignEnv` is how the client tells the runner which ground it injected, because the runner
sees only an `FBElevationProvider&` and could not name it.

### The four places the implementation is narrower or more explicit than the Spec

| # | Spec says | Built |
|---|---|---|
| 1 | the overlay may delete a `unit` block **or change the value of a `set` line** | **only deletion.** Nothing needed the value change, so `FBApplyCampaignCarry` has no path that writes a value: it erases a `unit` block or a `set store` line, and asserts afterwards that neither the block count nor the `set`-line count grew. A narrower permission is a smaller door |
| 2 | stores land on the `set store …` lines the mission declares | **as a per-(unit, store kind) STOCK.** A kind enters the book on the first sortie that DECLARES it and can only fall; a kind never carried is uncapped, so re-loading a type the jet has never flown with is not "inventing state" but the mission's own declaration. Surplus lines are dropped from the END of file order, so the effective load is a PREFIX of what the file says and never a selection |
| 3 | the fingerprint is over telemetry + "normalised" `events.log` + exit code | the normalisation is now **named, and it is exactly two field classes**: `wallS`/`speedup` (host speed) and the absolute path inside `telemetry=` (the `--out` directory). Without the second, criterion 2 cannot even be stated — a standalone re-run necessarily writes into a different directory. Nothing else is touched, so every other byte is compared raw |
| 4 | `campaign ATTRITION` carries "stores expended per type" | one `campaign EXPENDED store=… count=…` line per type beside `ATTRITION`. The store catalogue is a list, not a fixed field set, and a log line with a variable field set is worse than N lines |

One case the Spec did not cover: a carry that would remove **every** unit block of a mission. That is a
mission `FAIL` with the reason spelled out, before the spawn — not an empty run.

### The two acceptance criteria, measured

Measured on **both** ground bases, because one base proves one base — `swiss` is what `fb-gym` picks by
itself (the baked DEM is on disk), `const` is what `viper-attrition`'s four missions declare in their
headers:

| # | Criterion | `--elev swiss` (fb-gym's own default) | `--elev const` |
|---|---|---|---|
| **1** | one campaign fingerprint over 3 reps × `--threads 1/2/4` | **9 runs, 1 fingerprint** `f6dda7e6510f0d1e…` | **9 runs, 1 fingerprint** `0811c2cc79448df3…` |
| **2** | each step's per-mission fingerprint equals the same mission run STANDALONE with step *k−1*'s state | **4/4 match**: `69209e01…`/0, `5176c214…`/3, `938892b0…`/0, `943f036e…`/3 | **4/4 match**: `4deb7101…`/0, `0b492548…`/3, `70886a46…`/0, `b835dcce…`/3 |

Campaign exit 3 and the same four verdicts under both. The layer adds no hidden state — a fresh process
handed only the text state file and the recorded ground reproduces the step bit for bit.

**Re-measured 2026-07-30 (`E6`, the judge-completion fix of
[`../doctrine-evolution.md`](../doctrine-evolution.md) X-1), `--elev const`:** 9 runs, 1 campaign
fingerprint `fdf1da2ba166fcb3670538b8cb4e20572a006f48c7919e5e0697efee00b3e774`, per-step
`5ed024359f1c067e 9ab284a585f1a546 16da3efcae8c47fa 90aa24d31a87218e`, campaign exit 3, **4/4 standalone
replays MATCH**. The two hashes above predate several rounds of pilot work and are kept as history; the
value that matters for THIS round is the A/B, and it is exact — **the pre-round binary produces the same
campaign fingerprint to the byte** (`fdf1da2b…`), because none of `viper-attrition`'s four missions ends
before its judges are finished.

### The clock was missing from the replay half, and the second campaign found it (2026-07-29)

Criterion 2 is a statement about the campaign layer, and it was only ever tested on `viper-attrition`,
which declares **no** `time`. The first campaign that does — `sim/campaigns/o4-gaf-mig29g-dact.fbc`
([`../campaigns/o4-gaf-mig29g-dact.md`](../campaigns/o4-gaf-mig29g-dact.md)) — replayed **9 of 10 steps
DIVERGED** on the first attempt, and the one that matched was the only mission declaring its own clock.
The cause is exactly what criterion 2 exists to catch: `FBMissionCarry` has carried `CampaignUtcT0S` /
`HaveCampaignTime` since the layer was built, and **`fb-gym --mission --state` had no way to fill them**,
so every standalone step of a clocked campaign ran under no sky at all.

Closed the way §5 closes the ground rather than by a better default, because it is the same rule:

| Half | Where |
|---|---|
| the clock is **RECORDED** by the run | `campaign-summary.txt` gains `time <ISO>` (or `time none`), beside `elev`/`swiss_dem`/`base`/`threads` |
| the clock is **READ** by the replay, never guessed | `tools/fb_campaign_verify.py` passes what it read; a summary without the field is the pre-round shape and reads as `none` |
| the receiving flag | `fb-gym --mission … --campaign-time <YYYY-MM-DDThh:mm:ssZ>`. It is campaign DATA, not a client clock: it goes into the same two `FBMissionCarry` fields the runner fills, so `FBResolveMissionClock` applies it by §6's rule — it fills in for a mission that declares no `time` and never displaces one that does. `--campaign-time` beside `--campaign` is the same refusal `--state` already gets |

MEASURED after the fix: O4's ten steps **10/10 MATCH**, and `viper-attrition` is unchanged — 9 runs one
campaign fingerprint, 4/4 standalone replays MATCH. Conservation for the flag itself: `fb-gym` built with
the two touched sources reverted, both binaries over all **150** `sim/missions/*.fbm` — **515/515
`telemetry*.csv` byte-identical, 150/150 `events.log` identical modulo `wallS`/`speedup`/`--out`**, exit
codes identical. A `--mission` run without the flag takes the same `nullptr` path it always did.

**The first version of this measurement was wrong, and the way it was wrong is the reason for the
comparability rule in §5.** `fb_campaign_verify.py` defaulted its own `--elev` to `const` while `fb-gym`
defaults to `swiss`, so a campaign started the way a human starts it replayed as **4/4 DIVERGED**
(`groundAsl=782.97` against `groundAsl=0`) — a false alarm on the one measurement the whole campaign
programme rests on, and the kind that hides a real leak behind noise. The fix is not a better default:
the environment is now RECORDED by the run and READ by the check, `--elev` is an override, and a tree
without the record is refused.

### The overlay can remove and cannot add — measured, not asserted

Removal: in `viper-attrition` the overlay drops `bandit` (shot down in step 01) from step 02 and `bunker`
(destroyed in step 03) from step 04, and two of step 02's three `set store … aim120` lines plus one of
step 04's two Mk-82 lines. Every one is a `campaign CARRY` line in that step's `events.log`.

Addition: a hand-written state file claiming `unit viper alive stores mk82=9 aim120=4`, a unit `ghost`
the mission does not declare and a `ground newtarget` was handed to `attack-ccrp` via `--state`. Result:
**the identical fingerprint to the run without any state at all** (`70886a4672c4c8bc…`), zero `CARRY`
lines, two `SPAWN` lines. A state file that asks for more produces exactly the unchanged mission.

### Conservation

All **104** `sim/missions/*.fbm` + `missions/negative/*.fbm`, run singly as before, produce
**fingerprints identical to the pre-round binary** — same exit code, same telemetry bytes, same
`events.log`. The reference binary was built from the same tree with the five touched files reverted.

## Gaps

| Gap | Detail |
|---|---|
| ~~Only one campaign file exists~~ | **TWO as of 2026-07-29.** `sim/campaigns/o4-gaf-mig29g-dact.fbc` is the first of the ten real campaigns: ten new `.fbm`, both criteria measured on it, and it is the run that found the clock hole above. The other nine campaigns still have no `.fbm` files |
| **A controlled variant cannot share a callsign with its control** | learnt building O4 and worth stating here, because it is a property of the LAYER: the store carry is keyed by callsign, so a mission that is "its sibling plus one line" ALSO inherits that sibling's expenditure and stops being a control. The rule that falls out — carry chains and controlled pairs must not overlap — is a campaign-authoring constraint the format cannot enforce |
| The environment record names the DEM, it does not fingerprint it | `swiss_dem assets/swiss-dem-90m.bin` is a path. Re-bake the asset and every fingerprint changes while the record still reads the same — the comparison would then be across two grounds calling themselves one. Under `--elev tiles` it is worse and unfixable here: the ground is a live server's answer, so a `tiles` campaign is not replayable across time at all. **Never measured under `tiles`** |
| ~~Both criteria are measured on ONE campaign~~ | **two campaigns, and the second one earned its keep**: O4's ten steps under `--elev const` (9 runs / 1 fingerprint `461e0ff5299d83d03b…`, 10/10 replays). The advice stands unchanged — every further campaign re-runs `tools/fb_campaign_verify.py replay` for itself, and O4 is the reason it is not optional |
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
- **What `viper-attrition` actually showed, step by step.** It is the demonstration the layer was
  accepted on, and each step's outcome is a consequence of the previous one:

  | # | Mission | Carry applied on entry | Exit (standalone → in campaign) | State after |
  |---|---|---|---|---|
  | 01 | `intercept-aim120` | — | 0 → 0 | `viper alive aim120=1`, `bandit destroyed` |
  | 02 | `intercept-dlz` | `bandit` dropped; 2 of 3 `aim120` lines dropped | 0 → **3** | unchanged (nothing to shoot, nothing shot) |
  | 03 | `attack-ccrp` | nothing — `mk82` has never been on this jet's books | 0 → 0 | `+ mk82=1`, `bunker destroyed` |
  | 04 | `attack-ccip` | `bunker` dropped; 1 of 2 `mk82` lines dropped | 0 → **3** | `mk82=0` |

  Campaign exit 3, `ATTRITION unitsHostile=1 groundHostile=1`, `EXPENDED aim120=1 mk82=2`. Two of the
  four steps changed their verdict **because** the campaign worked; step 03 shows the other half of the
  rule — a store type the jet has never carried is not capped, because the mission's own declaration is
  the briefed load and the carry only ever takes away.
- **Why the campaign clock enters through `FBClockBoot.h` and not through the overlay.** Filling in a
  clock for a mission that declares none is the one thing that looks like "adding a line", so it happens
  in the ONE file that already owns clock precedence (`FBResolveMissionClock`, two extra defaulted
  arguments) and nowhere near `FBApplyCampaignCarry`. Measured both ways: a campaign `time` reaches a
  mission without one (`payerne-takeoff-only` gains a `CLOCK` line at the campaign's instant) and never
  displaces a mission that has one (`clock-night-payerne` keeps `1999-03-24T22:00:00Z` under a campaign
  declaring `2001-06-21T12:00:00Z`).
