# The player layer — FlightBox as a playable game

**Status: the first playable layer is BUILT (§11, §State), the player-control round §10 is built — a
bound stick, a trigger and a visible damage path — and since the tactical-map round §9 is built too:
a friendly declared net, a force-level picture at its control node, an APP-6 map over OSM ground in the
existing HUD pass, and orders that go to a unit's AI and can be refused (§12). §8 is not.** The rule this file was
written to protect held through the build: it adds no keyword to `.fbm`, no field to `.fbc`, no line to
any judge and no column to any telemetry file. Three screens in `sim/web/`, one save in `localStorage`,
and below them exactly two names the client reads (`window.FB_MOD`, `window.FB_MISSION`).

**Subject:** the layer between a finished run and a human — campaign select, mission select with a
difficulty ladder and an unlock rule, primary and secondary objectives, and a debriefing. Explicitly
**no score**.

### The owner's three sentences, verbatim, and they are the whole contract

> „Die saubere Trennung zwischen Fitness und Gamer-Score finde ich gut. Die Spielerschicht bauen wir
> zuletzt. … Wir brauchen dann ein paar Menüs, Kampagnen-Select, Mission-Select (nach Schwierigkeit
> gestaffelt und mit Freischaltmechanismus) und Debriefing für den Spieler. **Einen Score brauchen wir
> nicht.** Einfach welche Ziele erreicht wurden und ob es für einen erfolgreichen Abschluss der Mission
> reicht."

> „FlightBox soll komplexe Szenarien realistisch durchrechnen können. **Der spielbare Teil ist eine
> vereinfachte Repräsentation.**"

> „Für das Gym zum Trainieren und Simulieren gibt es Szenarien mit **tausenden Akteuren**, in denen
> komplexe Gefechte durchgerechnet werden können und Systeme optimiert, verbessert, ausgetestet und auch
> die KI verbessert wird. **Das Spiel sind vereinfachte Missionen mit Unterhaltungswert.**"

The second sentence decides the architecture: the player layer is a **VIEW on a run**, not a second
truth. Same simulation, same two judges, same reading rules underneath; above them a representation
that **omits and summarises**. That is what §1 turns into a rule with a check.

The third sentence decides the **scope**, and it is the one that stops this layer from growing into a
second simulator:

| | **Gym scenario** | **Game mission** |
|---|---|---|
| Purpose | compute a complex engagement; optimise, test and improve systems and the AI | entertainment |
| Size | up to thousands of actors — that is the gym's own scaling question | **small, and small is the property, not the shortfall**: it has to be *understandable*, not exhaustive |
| Verdict | the file's header reading rule + the telemetry | the completion rule of §3, over the judge's own lines |
| Who reads it | an engineer, once, with `grep` | a player, in the cockpit and then on one screen |
| Fidelity | full. Nothing below is simplified for either | full — identical, because the game changes nothing below itself |

**The scaling question is a GYM question and this layer does not inherit it.** A game mission is not a
cut-down gym scenario and does not have to survive one; what a `.fbm` costs per actor, where the tick
barrier stops paying and what "thousands of actors" costs is measured in
[`missions/runtime.md`](missions/runtime.md) (today: 2 units 1.29–1.41× at 2 threads, 4 units up to
1.77× at 4) and is **being quantified by a parallel round** — no number for it is invented here.

What follows for a game mission's shape, and each is a consequence rather than a preference:

| Property | Consequence |
|---|---|
| Cast | as many actors as the player can hold in his head and no more. A cast he cannot account for turns the debrief into a report about strangers |
| Length | short enough that a failed attempt is worth retrying. §4.2's retry rule is only humane if the run is |
| Objectives | few, and each nameable in one line of the briefing (§2.2). A seat with eight declared objectives has no primary |
| Threat | it must be *possible to notice*. A threat that only exists in a telemetry column is a measurement, not a mission |
| **What it does NOT have to do** | carry a controlled variable, isolate one lever, or answer anything. That is the gym scenario's job, and the game must not be asked to do it as well |

| What it reads | Where that is specified |
|---|---|
| the judge's per-objective vector, `mission OBJECTIVE unit=… kind="…" state=met\|unmet\|violated` | [`missions/verdict.md`](missions/verdict.md) §State (`E1`) |
| the per-unit verdict, `UNIT_RESULT result=… reason=… decisive=…` | [`missions/output.md`](missions/output.md) |
| the ordered mission list and the three carried facts of a campaign | [`missions/campaign.md`](missions/campaign.md) |
| the seat's own telemetry columns, incl. `eng_*` | [`pilot.md`](pilot.md) §8 |
| what the browser can and cannot do today | [`clients/clients.md`](clients/clients.md) |
| what it must never become | [`doctrine-evolution.md`](doctrine-evolution.md) §1.2, [`vision.md`](vision.md) |

**Where this file sits, and why at the root.** The player layer is not a source directory: it cuts
through `core/`'s judge output, `missions/`' two file formats, `clients/`' browser and a UI half that
exists nowhere yet. That is the same cut [`duels.md`](duels.md), [`formation.md`](formation.md),
[`air-defence-network.md`](air-defence-network.md), [`air-to-ground.md`](air-to-ground.md) and
[`doctrine-evolution.md`](doctrine-evolution.md) already make, and the same reason they are root files.

---

## Spec

### 0. The ten contracts

| # | Contract | Acceptance / measurement anchor |
|---|---|---|
| **P1** | **The layer is one-way: it reads a run, omits from it, and adds nothing** | §1's three checks, the third of which is a byte-identity |
| **P2** | **Primary/secondary is an ANNOTATION over objectives the mission already declares, and lives outside `.fbm` and `.fbc`** | §2. Acceptance: no `.fbm` and no `.fbc` gains a token; a briefing naming an objective the file does not declare is a parse error |
| **P3** | **The judge judges once. The game re-reads the judge's own output and never recomputes a state** | §3. Acceptance: for a briefing that marks every objective primary, "completed" equals `exit == 0` over every `mods/f16/src/missions/*.fbm` with exactly one judged unit |
| **P4** | **No score, anywhere** | §3. There is no number to print: the debrief shows a list of objectives with three states and one boolean. A field whose value is a total does not exist in the data model |
| **P5** | **The game grade can never reach the fitness** | §2.3. Acceptance: the fitness's inputs (`UNIT_RESULT`, `mission OBJECTIVE`, `eng_*`/`dmg_*` columns) are byte-identical whether a briefing exists or not — which is trivially true because the runner never sees one, and is *measured* rather than argued |
| **P6** | **The debrief shows nothing the seat did not see, except what the judge is entitled to say** | §5's three information classes and their refusal list |
| **P7** | **A game mission is SMALL, and smallness is a property** | §0's second table. The gym's scaling question is not inherited |
| **P8** | **Entertainment SELECTS, it never GRADES** | §8. Acceptance: no entertainment property appears anywhere in §3's completion rule, and the two live in different files |
| **P9** | **The map draws the OWN FORCE's picture, with age and uncertainty — never the truth** | §9.2. Acceptance: every map symbol resolves to a published block, a datalink track or a net report; the map adds **no** registry reader (`verify-layers` still prints six) |
| **P10** | **A player order is a PROPOSAL on the existing command path, never a state write** | §9.3. Acceptance: the set of state writers is unchanged — `FBFdmBoot` stays the only one, and every order produces `CMD_ISSUE`/`CMD_ACK` like any AI order |

### 1. The one-way rule

> **The player layer READS a run and MAY OMIT. It may never add a fact the run does not contain, and it
> may never write into anything below it.**

The tree already owns this shape: the campaign overlay *"may delete a `unit` block or change the value
of a `set` line … It may never add a line"*, and the built version is narrower still — it only deletes,
and asserts afterwards that neither the block count nor the `set`-line count grew
([`missions/campaign.md`](missions/campaign.md) §4 and §State 1). The player layer is the same move one
level up, with three checks instead of one argument:

| # | Check | How it is run |
|---|---|---|
| **1** | **Nothing below it changes.** The runner, the judges and the FDM never see a briefing | the same mission, run with and without a briefing present, produces byte-identical `telemetry*.csv` and `events.log`. Same gate shape as `C2`/`C12` conservation |
| **2** | **The view's postcondition.** Every row the view emits carries a PROVENANCE KEY naming the artefact line or column it came from | a debrief row without a source key cannot be constructed (the key is a constructor argument, not a field to fill in later — *"a narrower permission is a smaller door"*). After building: displayed objective rows **==** `mission OBJECTIVE` lines for the seat; displayed unit rows **⊆** units the seat's own sensors or the judge reported. **No set grew** |
| **3** | **The briefing resolves, it does not declare.** There is no syntax in a briefing for creating an objective, a unit, a zone or a value | every `primary`/`secondary` line must resolve to an objective the `.fbm` declares for that seat — the identical end-of-file resolution `kill unit` already gets ([`missions/syntax.md`](missions/syntax.md)). Unresolvable = parse error. The move is the genome's `Scale` key: *"it cannot express an absolute. There is no syntax for it"* ([`doctrine-evolution.md`](doctrine-evolution.md) §2.2) |

The run directory is opened **read-only**. The only thing the layer writes is its own save (§6).

### 2. Primary against secondary — and the line the fitness may never cross

#### 2.1 Where the line runs

| Layer | Knows primary/secondary? | Why |
|---|---|---|
| `core/FBMissionMonitor` (the judge) | **no** | it publishes an UNORDERED vector, one line per declared objective. Ordering the nine kinds against each other is the exchange rate [`doctrine-evolution.md`](doctrine-evolution.md) §1.2 refuses |
| `.fbm` | **no** | no new token. The conservation rule of [`missions/verdict.md`](missions/verdict.md) ("a mission without a new line judges byte-identically to today") stays true by having nothing to add |
| `.fbc` | **no** | a game key in the campaign file would enter the campaign fingerprint's own input and tempt the runner to read it |
| fitness / tournament / evolution (`tools/fb_fitness.py`) | **no** | level `M` stays an UNWEIGHTED COUNT because there is no mark in its inputs to weight by |
| **the briefing (`.fbp`, §6) and the menus/debrief that read it** | **yes, and only here** | the ladder a game needs exists exactly where a game is |

#### 2.2 The annotation, in one shape

```
campaign o4-gaf-mig29g-dact
title "GAF MiG-29G against the Viper"

mission o4-04-entry-10nm
  seat        fulcrum                 # the callsign the human flies; must be a unit of that .fbm
  difficulty  3                       # 1..5, [SET] and evidenced — §4
  ref         exit 0 met 2 of 2 fp d6f2052cf8f42ffd
  primary     kill unit viper         # must be an objective THIS seat declares in the .fbm
  secondary   survive
  brief       "Baltic range period. The bandit enters at ten miles."
```

Line discipline as `.fbm`/`.fbc`: one statement per line, `#` to end of line, unknown keyword is a
parse error. One file per campaign, `mods/f16/src/campaigns/<name>.fbp`, beside the `.fbc` and never inside it.

**An objective the briefing does not mention is PRIMARY** `[SET]` — because that default makes the
game's rule degrade exactly into the judge's rule (§3.2), so every relaxation is a visible line and no
briefing can silently claim more than the run did.

#### 2.3 What structurally stops the game grade from flowing back into the fitness

Not discipline — three independent facts, and the first alone is sufficient:

| Mechanism | Effect |
|---|---|
| **The grade is not a fact about the run at all** | it is a fact about the *briefing*, computed by the reader. The run's artefacts contain no mark, so there is nothing in the fitness's input for a future author to weight. This is the load-bearing one |
| A RESTRICTED header, in the shape `core/FBZone.h` already has | the briefing parser is declared in `tools/verify_layers.py`'s `RESTRICTED` table with an includer list of exactly the player-facing files — a NARROWING, like the zone gate's empty outside-includer list ([`air-defence-network.md`](air-defence-network.md) §4). `core/`, `missions/`, `pilot/` cannot name it |
| The Python half prints its own alphabet | the debrief tool imports the briefing parser and **not** `fb_fitness`; `fb_fitness` imports neither. Each prints what it can reach at start, in the shape of `fb_evolve.py`'s *"5 genes of 22 pilot keys; 0 non-pilot keys reachable"* ([`doctrine-evolution.md`](doctrine-evolution.md) §2.2) |

### 3. What "successfully completed" means — a rule, not a number

#### 3.1 The rule

> A mission is **COMPLETED** for its seat iff
> **(a)** every objective the briefing marks PRIMARY has `state=met`, **and**
> **(b)** no objective of that seat has `state=violated`, **and**
> **(c)** that seat's `UNIT_RESULT result` is neither `CRASH` nor `LOC`.

Every term is read off lines the judge already emits; nothing is recomputed. Three notes:

- **(b)** costs nothing new: `violated` is reserved for the three kinds that have a violation condition
  of their own (`survive`, `no_fire`, `protect`) — the judge's own "you decisively broke a rule you
  declared".
- **(c)** is `[SET]`, and it is the one outcome no briefing may absolve: `CRASH`/`LOC` in a
  `UNIT_RESULT` means an **undamaged** wreck, because for a shot-down unit the mission verdict takes
  precedence over the later impact ([`missions/verdict.md`](missions/verdict.md)). So (c) fails exactly
  when the player flew a serviceable jet into the ground or departed it.
- The rule can therefore say **"mission accomplished, aircraft lost"**, and it should — that is the
  trade case the judge already distinguishes (a unit with `kill` but without `survive` declares that its
  own loss is not a failure).

**Secondary objectives change nothing about completion.** They are reported met/unmet and are the only
thing that distinguishes two completions from each other. That is the whole of "no score": the
distinction is a LIST, not a total.

#### 3.2 Why the rule degrades into the judge's verdict, and how that is measured

With the default of §2.2 (unmarked = primary) and a briefing that marks nothing, completion is *"every
declared objective met, none violated, the jet not thrown away"* — which is the judge's SUCCESS.

> **Acceptance:** over every `mods/f16/src/missions/*.fbm` with **exactly one JUDGED unit**, that unit being the
> seat and declaring at least one objective, `COMPLETED == (exit == 0)`. A divergence is a defect in
> this rule, not in the judge.

The restriction to one judged unit is not a hedge: the run verdict quantifies over *all* judged units
(the AI wingman, the opponent), the completion rule over *one seat*. Those are different questions and
only coincide when there is one seat.

**And "judged" is the operative word, not "declares objectives" — corrected here because the loose
reading was measured and fails.** A unit is judged iff the mission gave it *a plan **or** objectives*
(`FBMissionBoot.h`), so a mission can carry one seat with objectives and a second aircraft with nothing
but a route, and that second aircraft's TIMEOUT is the run's exit code while the seat's own verdict is
SUCCESS. Four missions in the tree have exactly that shape (`f16-aim9`, `mig29-defend`, `mig29-r27`,
`mig29-r73`) and every one of them diverges under the loose filter. Under the corrected one the
acceptance holds without exception (§State).

#### 3.3 The TIMEOUT case — the annotation reads it, nobody rewrites it

Many campaign and combat missions have **TIMEOUT (exit 3) as their passing value**, with the binding
reading rule in the file's own header ([`missions/INDEX.md`](missions/INDEX.md) rules 4 and 5). The
completion rule above does **not** disqualify on `TIMEOUT` or on `FAIL`, and that is deliberate: it is
precisely what lets a measuring rig carry a playable task without a byte changing.

**Worked, on a rig whose seat declares `survive` + `kill unit X`:**

| Reader | Reads | Says |
|---|---|---|
| the rig (header rule 5) | exit code + the `eng_*` columns | exit 3 by construction; the verdict is the telemetry |
| the judge | its own two lines | `UNIT_RESULT result=TIMEOUT`, `OBJECTIVE survive=met`, `OBJECTIVE kill=unmet` |
| the game, briefing `primary survive` / `secondary kill unit X` | those same two lines | **COMPLETED** — "you came home; the bandit did not" |

The primary is phrased differently from the exit code, and neither reading disturbs the other.

**Worked, on `mods/f16/src/missions/net-blind-cue.fbm`** — the case whose pass criterion is *something not
happening*:

| Reader | Reads | Says |
|---|---|---|
| the rig | `grep -c "site TRACK" events.log` | passes iff **0**. This is the measurement, and it is not the exit code |
| the judge | the seat declares no `objective` line, so the flight plan is the entire verdict | `UNIT_RESULT result=SUCCESS reason="all waypoints reached"`; **zero** `mission OBJECTIVE` lines |
| the game | that one line + the briefing's prose | **COMPLETED** — "you flew the corridor south through the battery's own arc" |
| the game's debrief (view, §5) | the seat's own release lines and store telemetry | "two Mk 82 away, both impacted 52 m from your aim point; the position was **not** destroyed" |

Three properties of that table are the point of this whole section:

1. **The file is untouched.** No objective added, no header rewritten, no measurement moved.
2. **The game says LESS than the rig, never more.** The zero-TRACK criterion stays a rig reading; the
   game does not promote it to a pass condition, because promoting an event-log grep to a verdict would
   make the game a second judge (P3).
3. **A game author who wants "kill the radar" as a pass criterion must declare it as an objective in a
   mission file** — and that is mission authoring, which produces a NEW file with its own header. The
   player layer itself never forks, edits or shadows a flown `.fbm`.

> **Retracted from the first draft of this file, and named because it was wrong.** That draft had a
> rule "a game mission that needs a different objective block gets a sibling file". Under the owner's
> ruling the player layer is a view, so it has no conversion mechanism at all: one mission, one file,
> one judge, two readings.

### 4. Difficulty and unlocking

#### 4.1 Where difficulty is read from — declared with evidence, never computed

Two designs:

| | **A — a formula over declared mission features** | **B — a declared tier with a mandatory reference run (RECOMMENDED)** |
|---|---|---|
| Mechanism | weight hostile count, ground threat, `wx`, `time`, loadout, timeout into a number | the briefing declares `difficulty 1..5` `[SET]` **and** `ref exit … met … of … fp …` from a recorded AI-flown run of that file |
| Failure mode | it is a weighted sum, i.e. the exact defect `doctrine-evolution.md` §1.2 removed: the exchange rate between "one more MiG" and "night" has no answer, and whatever is chosen silently decides what "hard" means | a tier that contradicts its own reference run is a defect, exactly as a mission whose result contradicts its header is a finding (rule 5) |
| Provenance | a number with no derivation | `[SET]` + a measurement, which is one of the three admissible forms ([`conventions.md`](conventions.md)) |

**Recommended: B.** The features below are then the **evidence a reviewer reads**, never terms of a
sum, and every one of them is declared in the mission file or measured, so none is guessed:

| Evidence | Where it is read | Note |
|---|---|---|
| opposing cast: count, module/catalogue row, `flight` structure | the `.fbm`'s `unit` blocks | |
| ground threat: `net` block, site rows, `zone` cylinders | the `.fbm` | the belt is the difference between airspace and a corridor |
| sky and light: `wx`, `time` | the `.fbm` | **and the honest caveat:** night today moves 6 of 184 telemetry columns and reaches no decision ([`campaigns/o4-gaf-mig29g-dact.md`](../mods/f16/mods/f16/doc/campaigns/o4-gaf-mig29g-dact.md) sortie 9). A night tier would be a claim the tree cannot back |
| own loadout and timeout | the `.fbm` `set store` lines, `timeout` | O4 measured the magazine as worth more than any doctrine lever |
| the AI's own outcome on that file | the reference run's exit code + objective vector | the only ordinal input that is actually about difficulty |
| **`ACCEPTED` vs `ALPHA` of every catalogue row in the cast** | [`modules/air/flight-model-recipe.md`](modules/air/flight-model-recipe.md) | **not a difficulty input — an ADMISSION filter.** An `ALPHA` row *"may fly in a mission; it may not answer a campaign question"*. A mission whose opponent is `ALPHA` may be offered, and its briefing must carry that word, because the player's loss might be the deck rather than the pilot |

#### 4.2 The unlock rule, and there is no new graph

| Item | Rule |
|---|---|
| Order | **the campaign's own `mission` order in the `.fbc`.** It is already ordered, already carried, already the fingerprint's order. Inventing a second graph would put the campaign's sequence in two places |
| Unlock | step *k+1* unlocks when step *k* is COMPLETED (§3.1). Step 1 of every campaign is always unlocked |
| Campaign unlock | a campaign is unlocked when its `require` line (optional, naming another campaign) is completed; absent = unlocked. Difficulty tiers stagger the *presentation*, the `.fbc` order is the *mechanism* |
| Failure | a failed attempt is **retried from the same entry state** and appends nothing `[SET]` — a player expects a retry, where a measuring campaign's `stop_on never` deliberately wants the arc *after* the loss ([`missions/campaign.md`](missions/campaign.md) §Knowledge). Those are different questions and the difference is named rather than merged |

#### 4.3 How this sits on the campaign carry — nothing new is needed

The carry is three monotone facts and its currency is `campaign-state.txt`; the state **before** step
*k* is step *k−1*'s file, which is exactly what `fb-gym --mission … --state` consumes, and criterion 2
of the layer says a step re-run standalone from that file is bit-identical to the campaign's own step.

| Player action | Mechanism it uses | New? |
|---|---|---|
| fly step *k* | that mission + step *k−1*'s state file (+ `--campaign-time`) | **nothing new** |
| retry step *k* | the same two inputs again | nothing new — and this is *why* the retry is legitimate: the entry state is a file, not a memory of the failed attempt |
| advance | write the completed attempt's state file as step *k*'s | nothing new |

**One honesty note that must not be skipped.** Determinism criterion 1 (nine runs, one fingerprint)
**does not apply to a played run**: a human's inputs are not a declared quantity, so a played run is a
fresh trajectory whose fingerprint means nothing. What stays deterministic is the *carry* — given the
same entry state and the same declared file, the layer adds no hidden state. Fingerprints certify AI and
replay runs; a played run is certified by nothing but its judge output, and the layer must never present
one as a measurement.

### 5. The debriefing — every line with its source, and a refusal list

#### 5.1 What the player sees

| Row | Source | Class |
|---|---|---|
| campaign, mission, attempt number | `.fbp` + the save | own |
| **COMPLETED / NOT COMPLETED** | §3.1 over the seat's judge lines | judge |
| primary objectives, each `met`/`unmet`/`violated` | `mission OBJECTIVE unit=<seat> kind="…" state=…` + the briefing's marks | judge |
| secondary objectives, the same | ″ | judge |
| how the run ended | the seat's `UNIT_RESULT result=` + `reason=` | judge |
| time airborne, distance, fuel remaining, max g, landing state | the seat's own `telemetry*.csv` | own |
| what he shot and when | his own `sms RELEASE` / gun burst lines, his stores' telemetry files | own |
| the engagement debrief: time to detect, time to lock, shot range and the launch envelope at the shot, support fraction, reaction time, energy floor | the seat's `eng_*` columns — *"each is computable from one's OWN instruments and is a quantity a real debriefing would argue about"* ([`pilot.md`](pilot.md) §8) | own |
| his own exposure inside a declared zone | `zone_<name>_in` / `zone_<name>_s` of his own telemetry — **his own dwell, not the geometry** | own |
| BDA at roster granularity: which units his own weapons made combat-ineffective | `damage KILL` attributed to his stores | judge, by decision |

#### 5.2 The three information classes, and the boundary

He sat in the cockpit. A debrief that shows him what he never saw is a **different decision** from a
display, and it is taken here explicitly:

| Class | Rule |
|---|---|
| **1 — own-ship facts** | always shown. His telemetry, his `eng_*`, his releases, his radar/RWR picture, his damage |
| **2 — the judge's own statements** | shown, and this is the decision: the game is entitled to quote the outside judge, because a post-flight BDA exists in reality and because §3's whole point is that the judge already decided. Scope: the objective vector, `UNIT_RESULT`, and the roster-level "destroyed / not destroyed" for units **his own weapons** hit |
| **3 — opponent-interior facts** | **refused by default** |

The refusal list, stated as items rather than as a principle:

| Refused | Why |
|---|---|
| the opponent's telemetry and his `eng_*` (when he locked you, when he fired, his energy, his fuel) | it is the information the perception boundary exists to keep out of a pilot's hands, one layer up. A player who reads it learns the opponent's doctrine without flying against it |
| per-system damage breakdown of another unit (`damage SYSTEM … system=radar state=failed`) | interior. Class 2 stops at the roster bit; the debrief is **coarser** than the log, which is what "simplified representation" means |
| the truth position of a unit his sensors never held, and any miss distance measured against it | his own aim point and his own impact point are his; the target's real position is not. Where both are unknown to him, the row is omitted, not approximated |
| the declared `zone` geometry (centre, radius, floor, ceiling) | `core/FBZone.h` is RESTRICTED with an EMPTY outside-includer list; a belt drawn on a debrief map is the SAM ring handed over without a sensor. His own dwell columns are published per judged unit and are his |
| the identity of a contact his IFF did not answer for | *"a radar contact carries no identity; the only identity source is IFF Mode 4, and it knows no 'hostile'"* |
| anything at all about a mission he has not completed | see below |

**No in-game exception exists.** An earlier draft of this file offered an opt-in instructor track
(`debrief full`) showing both sides on a completed mission. It is **withdrawn** by the owner's ruling
that the view sees only what the own faction detected: an omniscient reading is an **engineering tool**
— gym, debugging, after-action analysis of a measured run — and it lives on the gym side with the other
instruments (`fb_duel_report.py`'s two-sided `eng_*` debrief is exactly that tool and already exists).
It is not reachable from the game client, so there is no switch to get wrong.

### 6. The menus — three screens, and most of the state already exists

| Screen | Shows | State source | New? |
|---|---|---|---|
| **Campaign select** | one row per `.fbc` with title, mission count, difficulty span, completed/total, locked/unlocked | `mods/f16/src/campaigns/*.fbc` (exists) + `.fbp` title/`require` (new) + the save | the `.fbp` |
| **Mission select** | the campaign's missions in `.fbc` order, each with tier, primary/secondary list, locked/unlocked, best attempt | `.fbc` order (exists) + `.fbp` marks (new) + the save | the `.fbp` |
| **Debriefing** | §5's rows | computed at run end from the run's own artefacts; nothing persisted except the completion facts | the view |

| State | Where it lives | Shape |
|---|---|---|
| campaign order, carried facts, entry states | `.fbc` + `campaign-state.txt` per step | **exists, untouched** |
| seat, tier, marks, brief prose, `require`, `debrief full` | `.fbp` beside the `.fbc` | new, text, one statement per line |
| unlocks, attempts, completions, which primaries fell | the player save — client state, canonical text, one fact per line, in campaign declaration order (the same discipline `campaign-state.txt` has, for the same reason: it is diffable and exportable) | new, and it is the ONLY new mutable state in the layer |
| the debrief itself | runtime only | not persisted |

### 7. The expensive part, named honestly

> **Superseded in part.** Items 1 and 3 of the table below CLOSED in the player-control round; they are
> kept with their original wording because the table is the reason the rest of this file is shaped the
> way it is. What replaced them is §10 and [`clients/clients.md`](clients/clients.md) §Knowledge.

**There was no way to fly FlightBox.** Not "partially" — no bound input device existed. What is
missing, from the tree's own gap tables, ordered by how hard it blocks a playable combat mission:

| # | Missing | Where it is stated | Blocks |
|---|---|---|---|
| 1 | **wasm has no release and no damage path.** `FBAppWasm.cpp` drains neither `Stores().TakeRelease()` nor `Guns().TakeBurst()`, holds no projectile pool, resolves no burst | [`clients/clients.md`](clients/clients.md) 5.1 | every combat mission. The browser can carry a weapon; nothing leaves the jet |
| 2 | **Units and weapons are invisible.** `FBUnitsStage`/`FBSpritesStage` are NoOp | [`render/units-visual.md`](render/units-visual.md) | you would fly against aircraft that are not drawn |
| 3 | **No bound input.** `FBInputSystem` is the NoOp default in `systems/FBSystemSlots.h`; `?ap=manual` routes a stick through the FBW but nothing fills it | [`clients/clients.md`](clients/clients.md) 5.3, [`architecture.md`](architecture.md) | flying at all |
| 3b | **No control curve.** The real jet's stick is a force sensor; the mapping from a gamepad axis to that command does not exist as a decision | [`modules/f16/hotas.md`](modules/f16/hotas.md), [`modules/f16/controls-commands.md`](modules/f16/controls-commands.md) | flying *the F-16* rather than a stick |
| 4 | **No cockpit displays in wasm**, and no lock/TD-box HUD symbology anywhere — *and it will not be invented* | [`clients/clients.md`](clients/clients.md) 5.2/5.4 | employing a weapon by eye |
| 5 | **`payerne-full` crashes under `--elev tiles`** | [`clients/clients.md`](clients/clients.md) 4.2 | flying over real terrain reliably |
| 6 | **`FBCampaignRunner` is gym-only by decision** and never reaches wasm | [`missions/campaign.md`](missions/campaign.md) §State, [`clients/clients.md`](clients/clients.md) | campaign progression in the client that would show the menus |

**What makes sense without an input path, and what it is not.** Campaign select, mission select, the
completion rule, the unlock state and the debrief all consume run **artefacts** — and an AI-flown run
produces the same artefacts a played run will. So the whole layer is buildable and byte-checkable
against gym runs before a single axis is bound.

> That is a **preview, not a game**, and it must be labelled as one wherever it appears. Its only real
> value is that none of it changes when input arrives.

### 8. Selection — the actual work of this layer

**"Entertainment value" is a SELECTION criterion and never a grading criterion.** Without a score, what
decides whether a mission is passed is §3's rule and nothing else. Entertainment decides **which** runs
are marked up as game missions and **how they are told** — the two must not touch, because the shortest
route back to a score is an entertainment property that starts influencing whether you passed.

| Question | Answered by | Lives in |
|---|---|---|
| Is this mission worth offering to a player? | entertainment properties, §8.1 | the `.fbp` — a mission is a game mission iff a briefing names it |
| Did the player pass it? | §3.1, over the judge's own lines | the run's artefacts |

#### 8.1 The properties a selector reads — all of them declared or measured

Not one of them is a new measurement; every row is a line in a file or a number a run already produced:

| Property | Read from | Why it matters for a player |
|---|---|---|
| **Something decides** | the reference run's `damage KILL` / `UNIT_RESULT` lines, or the objective vector moving at all | a run in which nothing happens is unplayable, whatever it proves |
| **When it decides** | the tick of the deciding event against `timeout` | a decision at t = 6.5 s is a knife fight; one at t = 549.8 s is a patrol with a surprise. Both can be good; a decision at 95 % of the timeout usually is not |
| **The seat has a task it can affect** | the seat's declared objectives, and whether the reference run met them | a seat whose objectives are met or lost independently of how it is flown is a cutscene |
| **Cast size** | the `.fbm`'s `unit` blocks | §0: as many as the player can account for |
| **The threat is noticeable** | the reference run's `rwr`/`vis`/`site`/`net` lines from the seat's own instruments | a threat that exists only in a column is a measurement, not a mission |
| **One sentence describes it** | the author | if the brief needs a paragraph, the mission is a gym scenario |
| **Fidelity admission** | `ACCEPTED`/`ALPHA` of every catalogue row in the cast (§4.1) | orthogonal to entertainment, and it is a veto rather than a preference |

#### 8.2 Who selects

**An author selects; a tool proposes.** The `.fbp` is written by a human, because "understandable" and
"worth flying" are not computable and a formula over the table above would be exactly the weighted sum
§4.1 refused. A tool may **rank candidates** out of existing run artefacts (decide/no-decide, decision
tick, objective movement, cast size) and print them for a human to accept — the same division the
tournament already uses: the instrument measures, the reader judges.

#### 8.3 The honest case, stated so nobody has to rediscover it

> A rig that flies 400 seconds and hits nobody can be **valuable as a measurement and useless as a game
> mission**, and neither of those is wrong.

Examples already in the tree, from their own published rows: `duel-merge` — the full 300 s with no K.O.
on either side and 231.6 s of it blind, which is precisely the finding; `net-cue-unnetted` — 0 contacts,
0 launches, viper SUCCESS, which is the whole point of its pair; `o1-05-beam-blind` — *"zero contacts,
zero shots, zero detonations, zero losses; the package walks past them"*, the sharpest measurement in
that campaign. Each is a first-class result, and none of the three should ever appear in a mission
select. The selection layer exists so that this can be said without either side being demoted.

### 9. The two-part view — map and cockpit

> The owner: *"Wir bauen später eine zweigeteilte Ansicht. TAB schaltet von der Kamera der Einheit auf
> eine OSM-Karte mit der kompletten Situationsübersicht um. Dort können alle Einheiten auf strategischer
> Ebene befehligt werden. Wenn ich eine Einheit selektiere und TAB drücke, sehe ich die Welt aus der
> Sicht der Einheit. Armored Fist hatte das so gemacht."*

#### 9.1 The map is not a new data product

Everything a situation map draws already exists as a published quantity. The map **renders** it; it
computes no picture of its own:

| Map element | Existing source | State |
|---|---|---|
| the base map | OSM tiles from `fb-tiles` | **exists**, the client already streams them |
| own units: position, heading, altitude, fuel, damage | each unit's own published blocks / its telemetry | **exists** |
| air tracks the force holds | `pilot/FBFlightPicture` — the shared picture, *"built from `FBState` blocks only"*, with the datalink's `dl_age` and its `TRACK_LOST` | **exists**, per FLIGHT |
| ground-net tracks | `FBNetReport` — a POINT with the sender's own `TgtLookAgeS`, **no id field and no team field** | **exists** |
| threat bearings | the seat's own RWR: a bearing and a class, **no range** | **exists** |
| own exposure inside a declared belt | `zone_<name>_in` / `zone_<name>_s` of the judged unit — his own dwell, never the geometry | **exists** (§5.2 refuses the geometry) |
| **a FORCE-level merge of several flights' pictures** | — | **missing.** The picture is per flight; there is no object that merges two flights and the ground net into one force view |
| **a 2D map stage in the client** | `render/` has no map stage; `FBUnitsStage`/`FBSpritesStage` are NoOp | **missing** |

**The structural line, and it is the one that a careless build crosses first:** the map is fed by
**published state**, never by `units/FBUnitRegistry`. The registry has exactly six readers inside the
perception boundary, pinned in `verify_layers.py`'s `RESTRICTED` table, and `verify-layers` prints the
number. **The map must leave it at six.** A map that reads the registry is not a map, it is the truth
with a map's icon set.

#### 9.2 What "our faction has detected" is — a union of unequal sources

> The owner: *"Die Karte sieht natürlich nur, was unsere Fraktion auch erfasst hat!"* — **binding.**
> There is no omniscient game view, and none is specified.

That decides the collision with §1 in the only direction that keeps the one-way rule: a map showing
units the own force never detected would **add** a fact the run does not contain. It is also the better
game, because early warning, the datalink and emission discipline become perceptible for the first
time. **A picture with holes in it is the content, not the limitation.**

"Detected" is not one source but a union, and the sources are **not equally certain**. The map must
show the difference, because the difference is the gameplay:

| Source | What it contributes | Certainty the map must show |
|---|---|---|
| own radar, firm track | position, altitude, velocity, updated at the sweep rate | the strongest thing on the map — and it means the unit is **radiating** |
| own radar, coasting | the last position, extrapolated | the block goes `held`: the numbers are real but old, and the track dies at the coast limit (documented: 6 s on the N019) |
| **IRST / KOLS** | a bearing and, with the laser, a range inside its limit | passive: seeing without being seen — and blinded by a cloud deck (`irst_masked`, measured) |
| **the eye** | contact → recognised → identified, with a TYPE at close range | a type is not an allegiance (see §9.3) |
| **RWR** | a **bearing and a class, never a range** | a wedge on the map, not a point. Also the launch warning — which in this tree arises only from a *supporting* emitter, because there is no MWS |
| **datalink between own units** | another unit's track, delayed by the net cycle | carries `dl_age`; `TRACK_LOST` when the terminal range is exceeded |
| **the controller (GCI)** | a call typed into the box, with its own latency | it is a *message*, not a sensor: it can be stale, wrong, or absent |
| **ground net cue** (`FBNetReport`) | a POINT plus the sender's own `TgtLookAgeS`, **no id, no team** | the age is part of the datum by construction |

**The uncertainty vocabulary already exists and must be used rather than re-invented:** the block bus is
three-state — `invalid` / `valid` / `held` — and `held` means *deliberately frozen: last good values,
last timestamp, no new computation*. The map's three symbol strengths are those three states plus the
ages the sources already carry (`dl_age`, `TgtLookAgeS`, the coast clock). Nothing new is computed.

#### 9.3 A contact carries no identity — on the map too

The perception boundary does not weaken because the information is being drawn instead of flown:
a radar contact is anonymous, **the only identity source is IFF Mode 4, and it knows no "hostile"**.

| Map class | When | Drawn as |
|---|---|---|
| **own** | it is one of your own units | full symbol, full data — you own it |
| **friendly** | your interrogator got a reply | friend symbol |
| **unknown** | everything else — no reply, no interrogation, or an enemy who happens to carry a working transponder | one neutral symbol, and it never becomes "hostile" on its own |

**Misidentification is therefore possible, and that makes it game content rather than a defect.** It is
already measured, in one run with three targets crossing the nose (`payerne-radar-iff.fbm`): a friend
with a transponder reads **friendly**, a friend *without* one reads **unknown**, and an enemy **with**
one also reads **unknown — not hostile**. Two consequences a player will feel:

- you can shoot a friend who is not answering, and the map will not have warned you;
- you can hesitate over an enemy the map cannot name, which is exactly the identification task the
  campaign set uses as its anti-cheat test.

The eye adds a **type** at close range (`vis IDENTIFIED`, reading `f16` / `mig29`, measured at 445 m) —
and a type is not an allegiance. The map may show the type; it may not colour the symbol from it.

#### 9.4 Losing information is the mechanic

A picture that only grows is a scoreboard. This one shrinks, and the shrinking is what the player learns
to read:

| Event | What the player sees | Where it is built and measured |
|---|---|---|
| nobody holds a contact any more | it ages, coasts, then goes | radar coast (6 s documented on the N019), `TRACK_LOST` on the datalink |
| a unit's link is jammed | **it drops out of the shared picture while still flying**, and falls back to declared autonomy | `net LOST reason=jammed` → `net AUTONOMOUS fallback=hold`, measured four times over in O1's sortie 09 |
| your own radar goes silent (EMCON, or an order you gave) | your own picture stops growing — and the opponent stops hearing you | the RWR's whole subject |
| a set is destroyed | its block goes `invalid`; its symbols stop | *"failure → block invalid"*, the coupling in [`weapons.md`](weapons.md) |
| the controller stops talking | the calls simply stop; there is no announcement | O1's sortie 03: a truncated brief IS *"a controller who stops talking at a declared instant with the aircraft already committed"* |

**Emission discipline becomes a dilemma the player can feel:** radiate and you see, radiate and you are
seen. Today that trade is a `[SET]` number in a mission file and an evolution gene; on this map it is a
button with a visible cost on both sides.

#### 9.5 The counter-check for the build — two runs, one line apart

In the shape the tree already uses for every rule it takes seriously:

> **Two runs that differ in exactly one line must be VISIBLY different on the map.**

The map view is a pure function of published state, so it can be dumped as a deterministic text trace
per tick by a gym-side tool and diffed — no GPU, no screenshot. Two pairs already exist and both are
measured:

| Pair | The one line | What the map must show |
|---|---|---|
| `net-cue.fbm` / `net-cue-unnetted.fbm` | the `net` block | netted: a cue at t = 8.0 s, a firm track at t = 145.1 s, two launches. Unnetted: **0 radiate, 0 track, 0 launch** — the battery never finds anything, *"not later, never"*. The two traces must differ from t = 8.0 s onward |
| `o1-08` / `o1-09` (`jam_comm_m`) | the jammer | four members leave the shared picture at t = 244.0…319.9 and go autonomous; `net CUE` 79 → 32, `site LAUNCH` 5 → 0 |

A build in which those two traces look alike has an omniscient map, whatever its symbols say.

#### 9.6 Commanding — the player writes no state, he talks to the unit's AI

> The owner: *"Und ja, ich kann **nur Befehle an die KI der Einheiten** geben."*

**The design decision, stated as one and not as a side effect:** the player stands on the **same side of
the anti-cheat boundary as an AI**. He sees only through simulated sensors (§9.2), he acts only through
simulated systems (this section), and he is judged by the same two incorruptible judges. **Nothing in
the anti-cheat structure has to be relaxed for a human, and nothing has to be duplicated for him.** For
a game that is unusual, and it is the whole reason FlightBox can afford a strategic map at all.

> **A player order takes the same path an AI order takes: a PROPOSAL, which can be accepted, clamped,
> inhibited, rejected, delayed — or never arrive.**

There is no player-only write path. `FBFdmBoot` stays the single state writer at spawn, and the
acceptance criterion is that this list does not grow.

**(a) An order can be refused, and the refusal is the mechanic.** The command bus has four outcomes
(`accepted`, `clamped`, `inhibited`, `rejected`), a closed rejection catalogue and two latency classes
(HOTAS 0.5 s, DED 4 s). Two of its existing rules will be felt immediately: a DED-class entry is
**rejected above 1.5 g** — you cannot re-task a pilot in the middle of a break turn — and an order into
a destroyed box acks `rejected/system_failed`.

**What the player is shown, and the boundary inside it:** he sees his order's *acknowledgement* — the
outcome and the reason the receiving unit's own box gave, because that is the unit's own knowledge and
it already travels in `events.log`. He is **not** shown a reason the unit does not have. If nothing
comes back, the display says exactly that — *no acknowledgement* — and never *"jammed by the emitter at
X"*.

**(b) An order can fail to arrive at all.** Then the unit falls back to its **declared autonomy** — built
and measured on the ground net — and does something other than what was ordered. On the map it has
dropped out of the shared picture (§9.4). Those two symptoms together are the core loop of commanding
under jamming, and neither of them is new code.

**(c) The order kinds, each checked against what exists:**

| Order | Exists today | What would be new |
|---|---|---|
| **Emission state** (radar on/off/mode, EMCON) | **yes** — a command-bus entry with its latency class; `set emcon`, `fcr_mode`, `n019_emission` are the mission-data spellings of the same thing | nothing but the button |
| **Fire authority** (`free` / `tight` / `hold`) | **yes** — the net node TRANSMITS a `wcs`, every member has a declared `autonomy` fallback | nothing. It is the model case: a doctrine word on a channel that can fail |
| **Route / vector** | **yes in shape** — `set brief_gci <atS> …` is a controller's call typed into the box, measured at **8.0 s from call to radiating radar** over three entries | the player is a second source of the same message |
| **Task** (`bfm` / `intercept` / `attack`) | **no** — `set task` is spawn-time mission data only | a runtime task command. This is the order a player will want most, and it is the layer's largest genuinely new write |
| **Formation station** (wedge / trail / wall) | **partly** — roles and positions are mission data (`flight`), but the station SHAPE is an airframe hook and *"a mission cannot brief a wedge, a trail or a wall"* | the same gap `pilot_flight_shape` (gene G1) already names |
| **Target assignment** ("you take the left one") | **no, and the price is already named** — a recipient field on the report, a member-readiness word, a minimum-cost matching | the matching is free (`FBFlightPicture::Assign` does it); the ADDRESSING is the new payload |

**Orders reach the player's own side only.** There is no mechanism to command another team and none is
specified — the identity to address one with does not exist in this tree.

#### 9.7 TAB is an information change, not a camera change

**This is the sentence a careless build deletes.** Going from the map into a unit means seeing what
**that unit** has: its own radar picture with its own ages, its own RWR, its own datalink tracks — not
the force's merged view, and not the map's. Coming back out means seeing only what the force has
actually collected.

| View | Data source | Hazard if merged |
|---|---|---|
| map | the force's merged picture (§9.1), own-force only | — |
| unit | that unit's own `FBState` blocks, i.e. **the same source the HUD already draws from** | one shared scene graph with a moving camera hands the map's knowledge to the cockpit and the cockpit's to the map, and the perception boundary is gone without a single line saying so |

The check is mechanical, in the shape of §1's check 2: **a symbol in the unit view whose provenance is
not that unit's own blocks is a defect**, and every symbol carries its provenance key. Selecting a unit
on the map and entering it must therefore *lose* information — if it gains any, the build is wrong.

*(The interaction precedent the owner names, Armored Fist, is exactly this shape: one map, one seat, and
the seat sees less than the map. FlightBox's addition is that the map sees less than the world.)*

### 10. The control path — **built**

> The owner: *"Steuerpfad kommt, ist aber erstmal zweitrangig."* — it came.

So it is **not** an open question: a human flies a unit. What §§10.1–10.3 predicted about it is now
either measured or still open, and each is marked below.

**Two rules that hold the shape open:** the view must be complete without it (§§9.1–9.7 all work on a
run flown entirely by unit AI), and nothing specified here may assume that the seat is AI-flown. The
missing pieces are §7's list — bound input plus a stick→command mapping, the client's release/gun/damage
path, visible units, displays and the symbology that *will not be invented*.

#### 10.1 What changes about the verdict when a human flies — nothing, and that is the point

**Both judges judge every unit regardless of who steers it**, and this is written down rather than
assumed: `FBFlightMonitor` is physical and module-agnostic (*the K.O. of ANY unit ends the run*), and
`FBMissionMonitor` reads observed position and an observed roster. **The physical judge knows no "but I
am the player":** a human who flies a serviceable jet into the ground produces `CRASH`, exit 2, and
§3.1's completion rule refuses it for the same `[SET]` reason it refuses it from an AI. Mission
objectives apply unchanged; the objective vector is published for a human seat exactly as for any other.

**What does change is the FLIGHT, because the human is the only actor without a phase machine.** The
cooperative machinery keys on observable states rather than on promises, so nothing breaks — but three
things degrade in ways a measurement must not be quoted through:

| Mechanism | With a human lead / member |
|---|---|
| **Station keeping** | the wingman holds station on the lead's datalink report, i.e. on a *moving point*. A human lead simply moves that point less predictably; the wingman still follows and `flt_sta` degrades. The published numbers (45.2 m median on a straight leg, ~1.9 km peak through a turn) are **AI-lead numbers** and may not be quoted for a human lead |
| **The sort** | assignments come from the shared picture, not from the lead's intent — `FBFlightPicture::Assign` ranks the cooperative sort above the briefed contract. A human who shoots whoever he likes can duplicate a target, and the existing violation metric says so: `flt_dup ∧ flt_free > 0` stops being zero |
| **The cover rule** | it defers on a *mate being bound*, which is a measurable state and not a pledge. So the AI holds up its end of the cover for a human it flies with, and the human owes it nothing back. The asymmetry is visible in `flt_defer_s` / `flt_both_s`, and it is honest: **cooperation is not a contract the human is bound by** |

Consequence, stated once: **a flight containing a human is not a formation measurement.** It is a game.

#### 10.2 What stays the same on the command path — and which half of it is only discipline

A human at the stick steers directly, but he must get **no more rights than the AI he replaces**: same
systems, same interlocks, same perception. Some of that is carried by the structure and some would not
be, and the difference is worth having in writing before anyone builds it:

| Boundary | Carried by | Verdict |
|---|---|---|
| *(the three rows marked "discipline today" below were the round's own shopping list; what the build did about each is the table after this one)* | | |
| **Perception** — he sees only what the blocks hold | the registry is reachable from six files, pinned in `RESTRICTED`, and the gate prints the number | **structural.** A UI reads published state and cannot widen it |
| **Damage** — he cannot repair himself | `FBSystemHealth` is monotone, all mutators private, exactly one friend; *self-healing does not compile* | **structural** |
| **Verdict** — he cannot be excused | both judges are outside every module and see no seats | **structural** |
| **State writing** — he cannot teleport, refuel or re-arm mid-run | `FBFdm`'s loading constructor is private with `FBFdmBoot` as its only friend — **but `FBFdmBoot` is reachable from `clients/`, and the player client IS a client** | **discipline today.** The boot-only usage is a convention, not a compiler error. The cheap gate, named so it is not improvised later: make the boot path refuse after the spawn window, in the shape the other gates take |
| **Avionics** — his switch actions obey the same latency, the same rejection catalogue, the same 1.5 g DED rule | only if the UI goes through `FBCommandBus`. A UI calling a system setter directly would bypass all three | **discipline today.** Same cheap gate: the setters private, the bus their friend |
| **Control inputs** — his stick goes through the FLCS like the autopilot's commands | only if the input path writes the same `fcs/*-cmd-norm` seam. Nothing structurally stops a client from touching an FDM property | **discipline today**, and the one most likely to be violated by accident in a hurry |

#### 10.2b What the build actually did about those three — and it is two out of three

| Boundary | State after the round |
|---|---|
| **State writing** | **still discipline.** `FBFdmBoot` is still reachable from `clients/` with no boot-window check. The player client does not use it after boot, and nothing forces that. UNCHANGED and still the cheapest gate in the list |
| **Avionics** | **structural in PRACTICE, not yet in the compiler.** The human's switch actions have no path except `FBCommandBus::Post`: `FBInputSystem` holds a queue of `FBHotasAction` and its only exit is `Post`, and the client never sees `FBStoresSystem` or `FBGunSystem` at all. The SETTERS are still public, so the gate ("setters private, the bus their friend") is still open |
| **Control inputs** | **structural.** The client cannot reach an FDM property: `FBInputSystem` lives in `systems/`, publishes a plain `FBStickInput`, and the MODULE turns it into the same `FBPilotCommands` the pilot returns. There is exactly one `ApplyPilotCommands`, and both seats go through it. The FDM is touched in one place, `FBF16Module::Run`'s substep loop, as before |

**And one boundary the build discovered rather than inherited:** with a human engaged the AI pilot is
**not run at all** — not run alongside and overridden, but skipped. Two sets of hands on one stick would
have been two writers of the same guidance, and whichever ran last would have won silently. The visible
consequence is that `pilot`'s own telemetry channels FREEZE where the human took over, which is honest:
that pilot did not decide anything after that moment.

#### 10.3 One thing to build with it, not after it

The `eng_*` debrief channels — time to detect, time to lock, the launch envelope at the shot, support
fraction, reaction time, energy floor — are recorded by `pilot/FBEngagement`, whose `Note*` methods are
called from `FBPilot::Run`. A human seat calls none of them, so §5.1's engagement block would be
**empty for exactly the seat the player cares about**. The class is a scoreboard and not a brain
(*"`FBPilot::Run` decides, this class records"*), so feeding it from the human's own actions is the
natural fix — and it belongs in the same round as the control path, not a round later.

**NOT built, and it is exactly where §10.3 said it would hurt:** `pilot/FBEngagement`'s `Note*` methods
are still called only from `FBPilot::Run`, and `FBPilot::Run` does not run for a human seat. So the
`eng_*` block is empty for precisely the seat the player cares about — the gap is unchanged in size and
is now REACHABLE, which it was not before.

Determinism ends at the stick and only there: a played run has no fingerprint (§4.3), while the same
mission flown by AI stays bit-reproducible. That is what keeps the measuring half intact.

**Except that in the browser it does not — measured, and booked as
[`clients/clients.md`](clients/clients.md) 5.5.** The browser paces its tick by rAF and the runner by a
fixed 0.1 s, and once weapons existed in the browser that difference became a different RESULT out of
the same file: on `cbu87-footprint` the AI's CCIP release executes at `aimMissM = 122.7 m` in Chrome
against `22.5 m` in fb-gym, and the run ends SUCCESS-without-a-kill where the gym kills two. A held gun
trigger fills `FBGunProjectiles`' 64-bundle pool in about a second, because the gun slot is unthrottled
by design and therefore emits one bundle per FRAME. This is principle 4's own case and it is a bug; it
is named here rather than worked around, because the fix is a fixed browser tick and that moves the
camera to 10 Hz — its own round.

### 12. The tactical map — **BUILT**, and the seven decisions it took

Both preconditions above were closed and §9's two missing pieces were built. What follows is what
exists, each row with the measurement that proves it; the numbers themselves are in `## State`.

| # | Decision | Why it is the narrower choice, and its anchor |
|---|---|---|
| **M1** | **A `net` block now works on an AIRCRAFT, so the player's faction can be given one in a mission file.** `modules/FBAirNet.h` answers the runner-generated `net_*` keys for both airframe families identically | the capability was already generic; what was missing was a member with wings. `net_link`/`net_period_s`/`net_hold` reach the terminal the jet already carries — **a fighter needs no second radio to be on a control net**. `net_sector` and `autonomy tight` are REFUSED with a reason rather than silently ignored: a sector of responsibility belongs to a position in the ground, and `tight` needs target addressing this tree has none of |
| **M2** | **The map's picture is `core/FBForcePicture`, built from PUBLISHED BLOCKS ONLY**, at the control node | `verify-layers` still prints **six** registry readers. The class names no registry, no world and no other unit's pilot; its whole input is one `FBState` plus one pose. It is `pilot/FBFlightPicture`'s move one level up, on the same terms |
| **M3** | **HOSTILE is never derived.** A PPLI is FRIEND, a radar echo with a Mode 4 reply is FRIEND, everything else is UNKNOWN | `FBForcePicture.cpp` writes exactly two affiliation values. `Suspect`/`Hostile` exist in the enum because APP-6 has them and nothing in the tree may assert one — measured on the map's own `map contact` lines: `aff=unknown` on every hostile datum of every run below |
| **M4** | **A bearing is drawn as a bearing.** RWR and the eye produce a dashed ray from the observer that runs off the frame; there is no point and no end | `FBForceSymbol::HavePoint` is false for them, and there is no code path that gives one a range. `FBVisualContact` withholds range structurally, so the map could not invent one even by accident |
| **M5** | **The map is drawn into the HUD pass, and REPLACES the cockpit symbology there** | one `FBHudGeometry`, one `FBHudStage`, **zero extra Begin\*Pass per frame** — the stage contract. It also enforces §9.7 by construction: `FBHudStage::Encode` builds the cockpit geometry only when there is no overlay, so the two pictures cannot be on screen together and neither can read the other's source |
| **M6** | **The OSM base map IS the terrain renderer, seen from nadir at the altitude whose footprint equals the map span** | so a symbol sits on the ground it was measured over, with no second projection to keep in step: `H = span / (2·tan(30°)·aspect)`. Two defects were MEASURED out of it, and both are the kind that draws a plausible picture over the wrong ground: (a) the pitch is **−89.9° and not −90°**, because `FBCameraBasisEcef` builds `right` from `fwd × world-up` and at exact nadir those are parallel — the basis collapsed and the frame came back EMPTY with 22 terrain leaves DRAWN; (b) the map's centre pixel is **`ViewH/2` and not `Height/2`**, because the scene is shifted up by the MFD bank's third — the first version put every symbol 120 px, i.e. **4.1 km at a 46 km span**, south of its own ground. (b) is measured: the scale-invariant fixed point of two frames rendered at 6 km and 12 km span from the same tick sits at **cy ≈ 226…240** (masked correlation 0.363/0.333) against **0.159 at cy = 360**. The residual ±15 px is the method's resolution and a tighter check is open |
| **M7** | **An order is finished by the BOX, not by the post.** A re-tasking is a typed DED entry; the pilot holds it until `FBCommandBus::AckOf(seq)` answers | measured: `order ISSUE` at t = 61.0 → `cmd CMD_ISSUE target=steerpoint class=ded dueS=65.1` → `cmd CMD_ACK outcome=accepted latencyS=4.1` → `order ACCEPTED ... phase=Intercept` at t = **65.1**. The first draft claimed acceptance from `Post`'s Pending, and the box then rejected the entry four seconds later — an order that was "accepted" on a map while its entry had failed. `AckOf` is the fix and it is the bus's own, not a second rule beside it |

**The one boundary this round DISCOVERED rather than inherited:** `modules/air/FBAirPilot::Run`
short-circuits the phase machine for an orbiting mover, so the base's order inbox was never serviced and
an order to the AWACS **vanished without a line**. `ConsumeOrders` is protected now and that branch calls
it. A refusal a commander cannot see is worse than a refusal.

### 11. The built preview — the six decisions this round takes, and each is a narrowing

The first playable layer is the three screens of §6 over gym-grade artefacts, in the browser, on top of
a client that flies and is judged but cannot shoot (§7). Six decisions were forced by that; every one of
them either quotes a judge line or refuses to claim something, and none of them touches the simulation.

| # | Decision | Why it is a narrowing, and its anchor |
|---|---|---|
| **B1** | **The artefact is the CONSOLE, not a run directory.** The browser writes no `events.log`; both judges self-log at the instant they conclude (`FBMissionMonitor::Conclude`, `FBFlightMonitor`), stdout reaches JS through emscripten's `Module.print`, and the layer parses the identical text a gym `events.log` holds | same lines, other transport. The parser is one function (`FBParseLogLine`) and it is exercised against recorded `events.log` files in the node harness — one reader, two venues |
| **B2** | **§3.1 (c) is read off `monitor KO` instead of `UNIT_RESULT`.** The browser never emits `UNIT_RESULT`: only `FBMissionRunner` does, and it is gym-only | STRICTLY STRICTER, and that direction is the whole argument: no `monitor KO` line ⇒ the physical judge never tripped ⇒ `UNIT_RESULT` is neither `CRASH` nor `LOC`. So every completion the preview claims is one §3.1 grants. The converse is not true and is booked as a gap |
| **B3** | **The primary SET is the seat's declared `objective` lines in the `.fbm`; the STATE of each is the judge's.** An objective the judge published no state for is not `state=met` | without it a run that dies before the judge speaks completes vacuously — measured: a spawn failure publishes `mission RESULT result=FAIL` and zero `OBJECTIVE` lines, and the naive reading called it COMPLETED. The declaration is the file's own line, not a computed fact |
| **B4** | **Unlock is ONE VERDICT, not a completion** `[SET]` | §4.2 spells the condition COMPLETED. In a client with no release path (§7.1) every combat rung ends `unmet`, so unlock-on-COMPLETED is a ladder no player can climb — the rule would be unreachable rather than strict. It is one predicate (`FBUnlocked`) and it tightens to §4.2 the day the release path lands. The save records `verdicts` and `completed` SEPARATELY, so tightening needs no new fact |
| **B5** | **No `.fbp`, therefore nothing is marked, therefore everything is primary.** The menus show the objectives the mission declares, in its own spelling | §2.2's default made load-bearing rather than decorative: with nothing marked the completion rule IS the judge's rule, which is what §3.2 asks to be measured — and it is (36/36 below). Primary/secondary is a gap in this file, not an invention in the frontend |
| **B6** | **The layer's entire write surface is three names:** `window.FB_MOD` (which scenario), `window.FB_MISSION` (which file of it to fly) and one `localStorage` key (the save) | the client reads the first exactly as it already reads `FB_TILES_URL`/`FB_ORIGIN_LAT`, sanitises it to a FILENAME, and changes nothing else about the run: same parser, same spawn, same two judges. `FBFdmBoot` stays the only state writer |

**What a rung is in the browser, stated so it is not mistaken for a campaign:** it is that `.fbm` flown
STANDALONE. `FBCampaignRunner` is gym-only (§7.6), so the client carries neither the campaign state
(units, ground, stores) nor the campaign clock from the rung before it. The mission-select screen says
so on the screen rather than in this file only.

---

## State

**The three screens are built, on artefacts a gym run and a browser run produce identically.** The
Spec's §§1–5 reading half, §6's screens and §4.2's ladder exist; §§8–10 do not.

| Piece | State | Anchor / measurement |
|---|---|---|
| the reading half — `.fbc`/`.fbm`/log parsers, the completion rule, the save format | **built**, DOM-free, one file (`sim/web/fbplay.js`), usable from node | 11 campaigns, 104 rungs, 104 missions parsed; the parsed objective count equals the files' own `objective` lines exactly (220 seat objectives), 0 defects |
| **§3.2's acceptance, measured** | **holds** | over the **36** `mods/f16/src/missions/*.fbm` with exactly ONE JUDGED unit, that unit being the seat and declaring an objective: `COMPLETED == (exit == 0)`, **36 agreements, 0 divergences** — one fb-gym run each, the JS rule read over that run's own `events.log`. Under the looser filter §3.2 originally spelled (57 missions) there are **4 divergences**, and they are the reason the criterion was corrected |
| campaign select, mission select with the ladder and the unlock, debriefing | **built** (`sim/web/fbmenu.js` + `index.html`); `/` is the menu, `?campaign=`/`&step=`/`?mission=` are links | headless-Chromium frames in `sim/build/player-layer/`: `A-campaign-select.png`, `B-mission-select.png` (o4 fresh = `oLLLLLLLLL`), `4-flying.png`, `5-debrief.png`, `6-ladder-after.png` |
| the unlock rule (B4) | **built and measured in the browser** | `viper-attrition` rung 1 (`intercept-aim120`) picked in the menu and flown to the judge's own line at **t = 291.0 s** (`mission RESULT unit=viper result=TIMEOUT`, `OBJECTIVE kind="kill unit bandit" state=unmet`): ladder `oLLL` → `ooLL`, save `step viper-attrition 1 attempts=1 verdicts=1 completed=0 last=TIMEOUT`, and `reset progress` returns it to `oLLL` |
| provenance keys (§1 check 2) | **built and structural** | a debrief row's source is a CONSTRUCTOR ARGUMENT (`FBRow(src, …)` throws without it) and lands in the DOM as `data-src`; the objective table is asserted equal to the seat's `mission OBJECTIVE` lines — no set grows |
| which mission the browser flies | **built** (`window.FB_MISSION`, sanitised to `[A-Za-z0-9._-]`) | `gpu mission_boot name=intercept-aim120-1` from the menu's own click |
| which MOD the browser plays | **built** (`window.FB_MOD`, same sanitising; the title screen is the front door, [`mods.md`](mods.md) §3.1) | `gpu mod id=f16 mission=/mods/f16/src/missions/payerne-full.fbm` |
| the mission buffer | **fixed**: 8 KB → 64 KB, and a full buffer is now REFUSED | `fb_fetch_text` truncates silently; the largest mission is 26,490 B (`o1-10-mole-cricket.fbm`), so 6 of the tree's missions were being cut — and a cut at a line boundary parses into a SMALLER CAST, i.e. a run against a file nobody wrote |
| **§10, the control path** | **built** | `systems/FBInputSystem` (the slot that was NoOp), `FBModule::HumanInput()` (null for every module without a cockpit), `FBF16Module`'s one-branch seat, `missions/FBOrdnance` (the shared release/gun/damage apparatus) and the keyboard in `clients/FBAppWasm.cpp`. Browser proof below |
| **the browser proof of §10** | **measured in Chrome for Testing, `?mission=attack-ccip`** | one run: `hotas STICK state=taken` (t=12.8) → `gun TRIGGER burstS=0.6 rounds=510` + `CMD_ACK gun_trigger accepted` (t=15.1, three squeezes for one held key, 510 → 378 rounds) → `sms RELEASE station=3 store=mk82` + `CMD_ACK weapon_release accepted` (t=18.6) → **`cmd CMD_REJECT seq=6 target=weapon_release reason=channel_busy`** (t=18.9, the bus refusing the human) → `sms RELEASE_REJECTED reason=hardware_precedence detail="master arm not in ARM"` + `CMD_ACK rejected` (t=24.1) → `CMD_ACK gun_trigger rejected reason=hardware_precedence` (t=25.3) → `stores IMPACT tofS=10.17` (t=29.4) |
| **the damage path in the browser** | **measured, hands off** | same file, nothing pressed: `sms RELEASE` → `stores SEPARATION` → `stores IMPACT` → `damage DAMAGE unit=bunker zone=center rangeM=53.8 fluxJm2=1962.7 warheadKg=87 degraded=4 hits=1` → `damage SYSTEM unit=bunker system=structure state=degraded`. `FBDamageModel` and `FBSystemHealth` both act in the browser and both are on the player's screen |
| **§9, the tactical map** | **built** | §12 and the eight rows below |
| the friendly declared net (precondition 1) | **built**, mission-side | `missions/map-friendly-net.fbm` and `missions/map-emcon-gap.fbm` — the first two missions in the tree whose `net` controller is on the `friendly` team. `modules/FBAirNet.h` is the aircraft's answer to the runner-generated `net_*` keys; no existing mission was touched, and all 78 hostile nets are ground-only and unaffected |
| the force-level merge (§9.1's first missing piece) | **built** | `core/FBForcePicture` — one `Ingest(FBState, pose)` per contributor, six sources, APP-6 affiliation. **`verify-layers`: 320 files, 6 registry readers, 1 simulation-loop driver, 3 restricted headers respected** — unchanged by this round, which is P9's acceptance |
| the 2D map (§9.1's second missing piece) | **built** | `render/FBTacticalMap` into the existing HUD pass; `FBRenderer::SetMapOverlay`. Frames: `sim/build/tactical-map/` (the three named proof frames plus `run.log` and `events.log`; the 61 numbered frames of the sequence are deleted after the reading, because a full sequence is 96 MB and this session's disk has filled three times) |
| **P9's acceptance, measured — "an enemy nobody detected is not on the map"** | **holds** | `map-emcon-gap.fbm`, node `magic`, whole faction radar-silent at spawn. `A-before-first-contact-t35.7.png` prints **CONTACTS 0** and carries three friendly arcs and one RWR bearing line; the hostile `raider` is 15 km inside the frame and is NOT drawn. The event that changes it is `t=40.5 radar RADAR_CONTACT unit=vip1 track=1` — the FIRST of the run on the friendly side — and `B-after-first-contact-t45.9.png` prints **CONTACTS 1** with a quatrefoil labelled `NET 2S`. There is no frame between them with a symbol and no measurement |
| **P10's acceptance, measured — an order goes through the AI, and can be refused** | **holds**, six outcomes in one run | `--order` on the native oracle, `map-emcon-gap.fbm`: (1) t=30.0 `order REFUSED kind=attack reason=nothing_held detail="no radar contact at all"` — the commander pointed at a place and the pilot's own radar was off; (2) t=40.0 `order REFUSED unit=magic kind=emcon reason=no_capability detail="this radar has no silent mode"` — a catalogue row's radar has no EMCON switch; (3) t=40.0 `order ACCEPTED unit=vip1 kind=emcon detail=radiate` → `cmd CMD_ISSUE target=radar_mode` → `cmd CMD_ACK ... accepted latencyS=0.5` → `radar RADAR_CONTACT unit=vip1` at t=40.5; (4) t=61.0 attack → 4.1 s of DED entry → `order ACCEPTED ... phase=Intercept` at t=65.1; (5) t=210.1 `order REFUSED kind=weapons_control reason=no_capability detail="no target addressing on an aircraft"`; (6) t=230.1 `order ACCEPTED kind=abort`, phase back to Route |
| the age vocabulary on the map | **built and drawn** | `C-aged-contact-t76.5.png`: the datum is 12.4 s old, so the symbol is faded to 40 % and carries a DASHED RING of `age × 250 m/s` — how far the thing could have gone since anybody looked. `kStaleS = 3.0` is three PPLI cycles (`kDropAfterCycles × kNetPeriodS`), `kColdS = 12.0` the radar's own coast limit as this tree's logs print it (`coastS=12`) |
| fire authority on an AIRCRAFT | **built** | `FBPilot::MayFire()` gates every weapon-employment post in `FBPilot` (BFM gun, BFM shot, intercept shot, briefed release, briefed gun, both attack releases). `Free` is the built behaviour exactly, so a unit nobody put under fire control is byte-identical. `Hold` refuses, once per episode, as `order WEAPONS_HOLD` |
| **§8 (selection)** | **not built** | this file |

## Gaps

| Piece | State | Anchor |
|---|---|---|
| the per-objective vector `mission OBJECTIVE unit=… kind="…" state=…` | **built** (`E1`, 2026-07-29), one line per declared objective, emitted at the one point every conclusion passes through | 432/432 telemetry files byte-identical, 60 of 137 missions gained exactly 136 lines |
| `UNIT_RESULT` with `result`/`reason`/`decisive` | built | [`missions/output.md`](missions/output.md) |
| the nine objective kinds | built | [`missions/verdict.md`](missions/verdict.md) |
| the campaign layer: ordered missions, three carried facts, a text entry state per step, standalone replay of any step | built | [`missions/campaign.md`](missions/campaign.md), 2 campaigns, both criteria measured |
| two real campaigns as material | built and flown | [`campaigns/o4-gaf-mig29g-dact.md`](../mods/f16/mods/f16/doc/campaigns/o4-gaf-mig29g-dact.md), [`campaigns/o1-bekaa-1982.md`](../mods/f16/mods/f16/doc/campaigns/o1-bekaa-1982.md) |
| the seat's own debrief channels (`eng_*`, 27 columns) | built | [`pilot.md`](pilot.md) §8 |
| a browser that flies, renders and trims | built, partial | [`clients/clients.md`](clients/clients.md) |
| the force's own picture, per flight, from `FBState` blocks only | built | `pilot/FBFlightPicture`, [`formation.md`](formation.md) §3 |
| the ground net's cue as a point with a look age, no identity | built | `core/FBNetReport.h`, [`air-defence-network.md`](air-defence-network.md) |
| the command bus: proposal, four outcomes, closed rejection catalogue, two latency classes | built | [`missions/avionics.md`](missions/avionics.md) §2 |
| fire authority on a channel that can fail (`wcs` + `autonomy`) | built and measured | [`air-defence-network.md`](air-defence-network.md), O1 sortie 09 |
| OSM tiles in the client | built | [`world/terrain.md`](world/terrain.md) |
| **everything in §§1–10 of the Spec** | **not built** | this file |

## Gaps

### From the built preview

| Gap | Detail |
|---|---|
| **PRIMARY AND SECONDARY ARE NOT SEPARATED, and the frontend does not invent the split** | the `.fbm` vocabulary carries no such token (by decision, §2.1) and the `.fbp` briefing of §2.2 is **not built**, so every declared objective is primary (§2.2's default) and the debrief shows the objectives in the mission's own spelling. The consequence is stated on the screen, not only here. This is the one thing a player would notice first and it is deliberately left undone: the split is an ANNOTATION FILE, and writing one per playable mission is authoring work with a `ref` run per rung (§4.1 B) |
| **No difficulty tier and no reference run** | §4.1's design B needs `difficulty 1..5` + a recorded AI run per offered mission, both of which live in the missing `.fbp`. The preview therefore shows the ladder POSITION (`RUNG k`) and nothing else — the `.fbc` order is the only ordinal in the tree, and no number is manufactured beside it |
| **The browser flies a rung STANDALONE — no carry, no campaign clock** | `FBCampaignRunner` is gym-only (§7.6), so rung *k* does not start from rung *k−1*'s `campaign-state.txt` and does not inherit the `.fbc`'s `time`. §4.3's table ("nothing new is needed") holds for the GYM; in the client the carry does not exist at all, and a `--state` equivalent would be the next piece |
| **`UNIT_RESULT` is gym-only, so §3.1 (c) is read off `monitor KO` (B2)** | the divergence has exactly one shape and it is worth naming: a seat that is SHOT DOWN and whose mission does not declare `survive` is §3.1's "mission accomplished, aircraft lost" — `UNIT_RESULT` would say FAIL/TIMEOUT rather than CRASH and the rule would COMPLETE it, while the preview sees the wreck's `monitor KO` and refuses. The preview is therefore never more generous than the rule, only occasionally stricter. Closing it means emitting `UNIT_RESULT` from the wasm frame loop, which is client work, not layer work |
| **§3.2's acceptance restriction was too loose as written, and the measurement says so** | "declares objectives on exactly one unit" is not the right filter — the run's exit code quantifies over every JUDGED unit, and a unit is judged by *a plan or objectives* (`FBMissionBoot.h`). Under the loose filter 4 of 57 missions diverge (`f16-aim9`, `mig29-defend`, `mig29-r27`, `mig29-r73`): the seat's own verdict is SUCCESS with its objective `met`, while the run's exit 3 comes from a SECOND judged aircraft that declares only a route. Under the correct filter — exactly one judged unit — the acceptance holds without exception. §3.2's own prose already said this two sentences later; the criterion is now corrected there |
| **The debrief is JUDGE-ONLY: none of §5.1's own-ship rows exist** | time airborne, fuel, max g, the releases, the `eng_*` engagement block, the zone dwell — all of them are TELEMETRY COLUMNS, and the browser writes no telemetry file (`FBTelemetryBus` has no sink in `FBAppWasm.cpp`). The layer refuses to re-derive them from anything else, so those rows are simply absent rather than approximated. The cheapest close is an in-memory telemetry sink in the client, and it is the natural companion of the `UNIT_RESULT` line above |
| **The log LINE FORMAT is now an interface** | the layer parses `t=… LEVEL tag event k=v` out of `Module.print`. A change to `FBStdoutLogSink`'s spelling silently costs the debrief its input. It fails LOUDLY rather than wrongly — an unparsed line yields "no verdict", never a wrong verdict — but a format change is now a two-place edit |
| **Unlock is one VERDICT, not one COMPLETION (B4)** | a deliberate `[SET]` relaxation of §4.2 for one measured reason: with no release path in the browser every combat rung ends with its `kill` objective `unmet`, so unlock-on-COMPLETED would lock the ladder at rung 1 for 10 of 11 campaigns. The save keeps `verdicts` and `completed` apart, so tightening it back is one predicate |
| ~~**Most rungs cannot be COMPLETED in the browser at all**~~ | **half closed.** Something now leaves the jet, and a released round flies, fuzes, impacts and damages through the same apparatus fb-gym drives. What is still in the way is not the release path but the browser's TICK: on a measuring rig calibrated against the runner's fixed 0.1 s the browser's release lands ~100 m short ([`clients/clients.md`](clients/clients.md) 5.5), so an attack rung can end without its kill for a reason that has nothing to do with how it was flown. The preview banner says PLAYABLE now and names what is still missing |
| **The armament panel is not a cockpit display** — and since the cockpit round it is REDUNDANT with one | the strip at the bottom of the screen shows arm state, selected station, stores carried/released, rounds, hits and combat-effectiveness — all of it off the seat's OWN published blocks, carried by one 1 Hz `hotas armament` log line. Every one of those numbers is now also on the SMS page of the MFD bank, drawn from the same blocks at 20 Hz; the strip row is kept because it survives a device loss and because it is the venue the ATTENTION row sits in, and that duplication is stated rather than tidied away ([`clients/clients.md`](clients/clients.md) 5.2) |
| **The cockpit round produced NOTHING the tactical map (§9) can inherit, and that is deliberate** | the owner's own separation — *"piloten ki sicht und taktische verwaltung sind zwei verschiedene dinge"* — is an architectural one: the cockpit reads the published blocks of ONE unit, the map the fused picture of ONE FACTION. The TAB path was not touched (TAB still toggles the ground albedo, `web/index.html`), no view-mode state machine was introduced, and no fusion object was built ahead of need. **What the map round will still have to build is unchanged from §9.1:** the force-level merge of several flights' pictures, and a 2D map stage in the client. **What it can reuse** is smaller and worth naming: `FBHudGeometry` + the HUD stage's two pipelines draw arbitrary 2D strokes and text into the existing HUD pass at zero extra passes (the MFD bank is the proof), `FBHudEnv` already carries a sub-rect of the frame, the HSD page is a worked example of "own heading up, bearings from published blocks, an age printed beside every symbol", and `web/fbmenu.js`'s log reader is the browser's existing route from a C++ fact to a drawn one |
| **The event feed is a LOG READER, so its selection is a hand-kept list** | `FEED_TAGS` in `web/fbmenu.js` names the twenty `tag event` pairs worth showing. A new refusal spelled by a new box is invisible until it is added there — the same two-place edit the log-format gap below already books, one level finer |
| **The save is per browser origin and is exported only by eye** | the campaign-select screen prints the canonical save text (diffable, one fact per line) and offers a reset; there is no import, and no file. That is enough to inspect and to reproduce, and nothing else reads it |

### From the specification

| Gap | Detail |
|---|---|
| **Most measuring rigs are not playable tasks, and the annotation cannot change that** | a combat rig ends in TIMEOUT *by construction*, which §3.3 handles — but a rig whose seat declares **no** objective and **no** plan is judged `NONE` and has nothing to complete. Those missions can only be offered as free flight or not at all. The playable set will therefore be built mostly from missions written for it, and writing them is mission authoring, not this layer |
| **The objective key may not be unique — UNVERIFIED** | a seat may legally declare two `identify` lines differing only in `range`/`hold` ([`missions/syntax.md`](missions/syntax.md)). If `kind="…"` does not carry the arguments, a briefing cannot name one of them. The cheap fix (`idx=` on the line) would move `events.log` bytes on the 60 missions `E1` was accepted on. **Preferred, and to be checked before anything is built:** the ordinal is recoverable from the ORDER of a unit's `mission OBJECTIVE` lines, if that order is the file's declaration order. This is a **TODO against `FBMissionMonitor::Conclude`**, not an assumption |
| **The client that would show the menus is the client that cannot fly the missions** | §7 items 1, 2 and 6 together. The gym can fly everything and has no UI; the browser has the UI and can neither shoot nor be shot nor run a campaign |
| No briefing prose exists anywhere | every mission's header comment is a *reading rule* for an engineer, not a briefing for a pilot. The `.fbp` `brief` line is a second text with a different audience, and writing 100 of them is real work |
| Difficulty tiers have no reference runs recorded | §4.1's design B requires one recorded AI run per offered mission. O4 and O1 already publish per-step exit codes and fingerprints, so those twenty are nearly free; nothing else is |
| A played run has no fingerprint | §4.3. Stated, not solved — there is nothing to solve, but a tool that compares a played run to a measured one is a category error and must be refused rather than approximated |
| Night is a tier the tree cannot back | 6 of 184 columns move and nothing consumes them (`C3`'s consumer gap). A "night = harder" tier would be manufactured |
| **Rejected: a score, in any form** | not an omission — the owner's decision, and it happens to be the same decision `doctrine-evolution.md` §1.2 reached independently for the fitness: a weighted sum answers *"how many secondary objectives is a primary worth"*, which has no answer, and whatever rate is chosen becomes a standing offer. A list of objectives with three states cannot be farmed because there is nothing to accumulate |
| **Rejected: primary/secondary as a token in `.fbm`** | it would put the ladder into the file the judge parses and the fitness's inputs are generated from, one careless round away from a weighted `M`. It also breaks *"a mission without a new line judges byte-identically to today"* the moment anything reads the token |
| **Rejected: game keys inside `.fbc`** | the campaign file is a measurement artefact whose parser refuses unknown keywords and whose content is an input to the campaign fingerprint |
| **Rejected: the player layer forking a `.fbm` to make it playable** | superseded by the owner's ruling: the layer is a view, so it has no conversion mechanism. Kept here with its reason because the first draft of this file contained it |
| **Rejected: promoting a header reading rule (e.g. `net-blind-cue`'s zero `site TRACK`) to a pass criterion** | it would make the game a second judge over facts the judge did not judge. It stays a debrief line |
| ~~**No force-level picture exists**~~ | **CLOSED** — `core/FBForcePicture`, §12 M2. |
| ~~**No map stage in the renderer**~~ | **CLOSED** — `render/FBTacticalMap` into the existing HUD pass, §12 M5. `FBUnitsStage`/`FBSpritesStage` are still NoOp: the map draws SYMBOLS in 2D and the 3D units remain undrawn, which is `render/units-visual.md`'s gap and not this one |
| ~~**A runtime TASK order does not exist**~~ | **HALF CLOSED, and the half that is open is named.** `FBOrderKind::{Waypoint, Steer, Attack, Abort, Emcon, WeaponsControl}` exist and are carried out. What is still spawn-time-only is the PHASE vocabulary itself: an order cannot say `bfm` or `formation`, only "engage what you hold there" (which enters Intercept) and "break off" (which returns to Route). **Target ADDRESSING is still absent** and is the reason `weapons_control tight` is refused rather than implemented — the price is `air-defence-network.md` §3's, unchanged |
| **The map's ATTACK order is gated on the ORDERED UNIT's own radar, and only its radar** | `FBPilot::ConsumeOrders` correlates the ordered point against `state.Radar` contacts within `kOrderTargetGateM = 4000 m`. A pilot who holds the thing on his IRST, his eye or his datalink alone is refused `nothing_held` — which is stricter than it needs to be and is a defect of omission, not of principle. It is one loop per additional block |
| **The map is ONE node's fusion, not the faction's union** | `FBForcePicture::Ingest` is called once, for the control node. A second call per contributing unit would make it the faction's union — the class already takes it — but the client has no rule for WHICH units may contribute, and "every own unit" would quietly hand the map the picture of a jet that is off the link. Named rather than guessed |
| **The map's registration is verified to ±15 px, not to a pixel** | the boresight was located by correlating two spans of the same tick, and two different terrain LODs are two different images: the masked correlation peaks at 0.40 and its optimum is a plateau. A tight check needs a landmark with a published coordinate drawn on the frame, which is a marker this map does not have |
| **A member's surveillance report is ONE point** | `FBNetReport` carries a single position (its nearest non-friendly-IFF echo), so a fighter holding four contacts contributes one. That is the type's own shape and widening it moves every `net`-carrying mission's bytes |
| **Two anti-cheat boundaries are discipline, not structure, the moment a client can steer** | §10.2: `FBFdmBoot` is reachable from `clients/` with no boot-window check, and nothing forces a UI's switch actions through `FBCommandBus` or its stick through the `fcs/*-cmd-norm` seam. Named with its cheap gate in each case, so it is closed deliberately rather than improvised under time pressure |
| **`eng_*` would be empty for a hand-flown seat** | `pilot/FBEngagement`'s `Note*` methods are called from `FBPilot::Run`. Feeding the recorder from a human's actions belongs in the control-path round, not after it (§10.3) |
| **Withdrawn: an in-game omniscient view** | an earlier draft had `debrief full` as a completed-mission opt-in. The owner's ruling (*"die Karte sieht natürlich nur, was unsere Fraktion auch erfasst hat"*) makes omniscience an engineering tool on the gym side only, where `fb_duel_report.py` already is one |

## Knowledge

- **Why the game may have a ladder where the fitness may not.** The fitness is a SEARCH TARGET: whatever
  exchange rate it declares, an optimiser is a machine for finding the cheapest way to accept it
  ([`doctrine-evolution.md`](doctrine-evolution.md) §1.2). A briefing's ladder is read by a human once,
  after the fact, and optimised against by nobody. The danger is not the ladder — it is the ladder
  entering an input the optimiser reads, which §2.3 makes structurally impossible.
- **Why "the judge re-read, not re-judged" is the whole design.** The collapsed exit code cannot express
  *"primaries met, secondaries missed"*, because SUCCESS quantifies over every declared objective. The
  vector the judge publishes is finer than the code it produces, and `ObjectivesMet()` is *defined* in
  terms of `StateOf`, so the vector and the verdict cannot disagree
  ([`missions/verdict.md`](missions/verdict.md) `E1`). The game reads the finer output. It computes no
  state of its own, which is why P3 is checkable as an equality rather than argued.
- **The measuring-instrument question, answered.** *Do campaigns lose their value as instruments when
  they are "formulated for a game"?* **No — because the game does not replace them.** The player layer
  is a view: same simulation, same judges, same reading rules underneath. Three reasons it holds, and
  one case where the answer flips:
  1. the run's artefacts — the telemetry bytes, `events.log`, the exit code, and every fingerprint over
     them — are produced by a runner that never sees a briefing (check 1 of §1);
  2. rule 7 already forbids a campaign editing a mission; the annotation is one level further out and
     edits nothing;
  3. the two readings use different keys on the same output: the rig reads its header's rule and the
     telemetry, the game reads the judge's vector and the marks.
  It flips **iff** a flown file is edited — an objective block changed for playability, a mission order
  changed for pacing, a cast changed for drama. Then the file stops proving what its header says and
  every published fingerprint dies. Hence the one-sentence rule: **a game may add files and briefings;
  it may never edit a flown one.**
- **Why simplification is one-way, and how that is not a slogan.** "Simplify" would otherwise cover both
  omitting (fewer rows than the log) and inventing (a summary the run does not support). The direction
  is fixed by the postcondition in check 2: every displayed row names its source line, and no displayed
  set is larger than the set it was built from. The precedent is `FBApplyCampaignCarry`, which is allowed
  to delete and *asserts afterwards that nothing grew* — the same assert, on a different set.
- **Why the unmarked-objective default is PRIMARY and not secondary.** With that default the game's rule
  is the judge's rule, and a briefing can only ever RELAX it, one visible line at a time. With the other
  default a silent briefing would inflate completions, and the inflation would be invisible in the file.
- **Why the player standing on the AI's side of the anti-cheat boundary is the enabling decision, not a
  restriction.** He sees through simulated sensors, acts through simulated systems and is judged by the
  same two outside judges — so **nothing has to be relaxed for him and nothing has to be duplicated for
  him.** Every mechanism the layer needs already exists because an AI needed it first: the shared
  picture, the command bus with its rejections, the autonomy fallback under jamming, the objective
  vector. That is why a strategic map plus commanding costs this tree an aggregation and a renderer
  rather than a subsystem.
- **Why entertainment must not touch the completion rule, in one line.** The moment an entertainment
  property ("it was close", "it took long enough") influences whether you passed, it is a score with
  another name — and a score needs weights, which are `[SET]` numbers that silently decide what the game
  means. Selection and grading therefore live in different files and are read by different code paths
  (§8).
- **Why the map's uncertainty vocabulary is not invented.** Three states plus an age is exactly what the
  block bus already publishes (`invalid` / `valid` / `held`, plus `dl_age`, `TgtLookAgeS` and the coast
  clock). A map that invented its own confidence scale would be adding a fact — the thing §1 forbids —
  and would drift away from what the systems below actually did.
- **Why the perception boundary survives one layer up.** [`vision.md`](vision.md) states that anti-cheat
  is a **game** decision: *"a cheating opponent is noticed immediately"*. The dual is that a cheating
  debrief is not noticed at all — the player simply gets better at a mission for a reason he cannot
  name, and the sim's carefully anonymous contacts become identified by a screen he reads afterwards.
  The layer's whole value proposition is that the *representation* is simplified while the *world* is
  not; showing him the world's interior would trade away exactly the thing he is paying for.

---

## The tactical map — the round after the cockpit, and its two preconditions

The owner set the architecture in one sentence: *"du hast die übergeordnete ki ja schon gebaut. ihr
gehört die kartenansicht und ich kann in die sicht jeder einheit wechseln, die von einer piloten ki
gesteuert wird."* That is exactly right and it decides the design:

| view | whose picture it is | already built |
|---|---|---|
| **cockpit** | the PILOT AI of one unit — its own published blocks | yes (this file, the MFD round) |
| **tactical map** | the SUPERORDINATE AI — the connected air defence's controller node | yes, [`air-defence-network.md`](air-defence-network.md), status BUILT |

The map is therefore **not a UI invention**: it renders a picture that already exists and is already
bound by the perception boundary. `FBNetReport` carries a POINT, a look age and **no identity**
(`core/FBNetReport.h`), so a map drawn from it cannot know more than the faction measured.

### Precondition 1 — the player's own faction has never had a net

[MESS, 2026-08-02] **78 missions declare a `net`, and the controller of all 78 sits on the `hostile`
team.** The capability is generic — `net <name>` with `control`, `member`, `period`, `hold`, `wcs` — and
no mission has ever given one to the side the player commands. Without a friendly net the map has no
source, and inventing one in the frontend would be exactly the omniscience this tree forbids.

**So the map round begins with a mission-side change, not a client-side one:** the player's faction gets
a declared net, and what the map shows is what that net's controller fused.

### Precondition 2 — the symbology is a standard, and the standard already encodes our uncertainty

The owner asked for every decision to be visible and pointed at the references. The right ones are
**NATO APP-6 / MIL-STD-2525** (joint military symbology), and they fit this tree better than a
hand-drawn set would, for a reason worth writing down:

**The standard's affiliation vocabulary is graded, and so is our knowledge.** APP-6 distinguishes
PENDING · UNKNOWN · ASSUMED FRIEND · FRIEND · NEUTRAL · SUSPECT · HOSTILE, and it has a status axis
(PRESENT vs ANTICIPATED/PLANNED). That maps onto what this simulation can honestly assert:

| what the tree measured | the symbol it earns |
|---|---|
| valid IFF Mode 4 reply | **FRIEND** — the only positive identification this tree has |
| a radar echo with no reply | **UNKNOWN**, never HOSTILE — silence proves nothing, and that IS the identification problem |
| an RWR bearing without range | UNKNOWN, drawn as a **bearing line**, not a point |
| a visual contact | a TYPE without a range, once angular size allows it |
| a contact nobody has looked at recently | PRESENT fades to a **datum with its age**, per `FBNetReport::TgtLookAgeS` |

**The rule that follows: the map may never draw HOSTILE from IFF alone**, because `sensors.md`'s IFF has
exactly two answers — a valid friendly reply, or silence. A hostile marking would be a claim no sensor
in this tree can support. Where a mission's own declaration makes a side hostile, that is mission text
and may be shown as such; where it comes from a sensor, it may not.

**And "all decisions visible" is the same discipline applied to the command half:** every order the
player gives is a posted command with an acknowledgement or a refusal, and the map shows both — the
intent, the unit that received it, and what the pilot AI did with it. An order that the AI could not
carry out must be visible as a refusal, not silently absent.

