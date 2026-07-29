# The persistent world — snapshots, and the one boundary that must stay sharp

**Status: IDEAS AND CONSTRAINTS, NOT A DESIGN.** Nothing here is scheduled, nothing here may block a
round, and the open question at the end is deliberately left open. The file exists so that decisions
taken for other reasons do not quietly make a persistent world expensive or impossible later.

**The owner, verbatim, and the three sentences set the ambition of this file:**

> „Ich sehe OCEAN eher **offline zum Erstellen von Weltzuständen**, auf denen FlightBox dann aufsetzt.
> OCEAN ist **nicht zur Laufzeit** gedacht, sondern zur **Weltgenerierung**."

> „Ich denke, wir können **später** einen persistenten Server hinzufügen, der in geringer Auflösung den
> Weltzustand **jeden Tag um einen Tag** vorschiebt. **Details sind da egal.** Aber es sind **nur Ideen**.
> Momentan geht es darum, **Optionen offen zu halten**."

> „Aber **persistente Welt-Snapshots, die wir speichern und laden können**, sind sowieso eine gute Idee
> und helfen uns, **die Welt formal zu beschreiben**."

The third sentence is the concrete part and this file's core (§1–§3). The second says the progression
question stays unanswered (§6). The one thing that already has consequences for building today is the
class boundary (§4).

**Why the file sits at the root of `doc/` and not in the mirror:** its subject is not a source
directory. It touches `missions/` (the cut and the carry), `core/` (the state artefact and the two
judges), `units/` (who exists at all), `sensors/` (who may see it) and `tools/` (which is not in the
mirror at all). Same reason [`player-layer.md`](player-layer.md) and
[`doctrine-evolution.md`](doctrine-evolution.md) sit there.

---

## Spec

### 0. The four contracts

| # | Contract | Where |
|---|---|---|
| **PW1** | **A world snapshot is a widening of `campaign-state.txt`, not a new concept** — canonical text, overlay deletes and never adds, environment recorded rather than guessed | §1 |
| **PW2** | **The mission fingerprint is the completeness test of the world description**, and therefore the *procedure* by which that description is worked out | §2 |
| **PW3** | **A snapshot is ground truth and contains nothing derivable and nothing perceptual** | §3 |
| **PW4** | **An actor's class follows MEMBERSHIP in a mission, never distance, detection or attention** | §4 |

### 1. Save and load already exist, with their correctness proof — for a very narrow world

The tree does not need to invent snapshotting. It owns it, with a measurement:

| Piece | What it already is |
|---|---|
| `campaign-state.txt` | a canonical text snapshot written after every mission, ordered by mission declaration order, one fact per line ([`missions/campaign.md`](missions/campaign.md) §4) |
| `fb-gym --mission FILE --state FILE [--carry LIST] [--campaign-time ISO]` | **the load path.** A run born from an external prior situation rather than from its own file alone |
| criterion 2 of the campaign layer | **the load proof.** Step *k* re-run standalone from step *k−1*'s state file must produce the *same mission fingerprint* as the step inside the campaign |
| the measurement | **10/10 on `o4-gaf-mig29g-dact`, 4/4 on `viper-attrition`**, both ground bases, after the clock hole was closed |

So a persistent world snapshot is the **broadening of a proven mechanism** — three columns become more
columns. The three properties that made the proof possible are the ones a wider snapshot must keep, and
they are the whole of PW1:

| Property | Why it is load-bearing |
|---|---|
| **canonical text**, one fact per line, declaration order, never filesystem order | diffable, greppable, hashable — and the ordering is part of the fingerprint's input. `[SET]`: text, because `campaign-state.txt` is text and the reasons transfer unchanged. The row count at which text stops being viable is a measurement that does not exist; when it is taken, it is a decision, not a drift |
| **the overlay may delete and never add** | the moment it can add, the `.fbm` stops being the statement of what was flown, and the binding header rule ([`missions/INDEX.md`](missions/INDEX.md) rule 5) becomes false. The built version is narrower than its own spec — it *only* deletes and asserts afterwards that no count grew |
| **the environment is RECORDED by the run and READ by the replay, never guessed** | `campaign-summary.txt` carries `time`/`elev`/`swiss_dem`/`base`/`threads`; a tree without the record is refused rather than replayed against a default. A snapshot is one more such input and enters that record |

`[SET]`: **one file per epoch, never mutated in place.** A flown artefact is never edited — the rule
already holds for `.fbm` and `.fbc`, and it is what keeps every published fingerprint alive.

### 2. The fingerprint is the completeness test — and that makes "describe the world formally" a procedure

This is the point of the whole file.

> **What must be in the snapshot for a standalone run to reproduce the fingerprint IS the world state —
> by definition. A missing field does not need to be foreseen: the replay diverges and names itself.**

So the world description is not designed up front. It is **worked out**, by a loop that already exists:

1. put a candidate field into the snapshot;
2. run the load proof (`tools/fb_campaign_verify.py replay`) over a campaign;
3. read the divergence — the diff names the file and the column;
4. repeat until it holds.

**The precedent, and it is exactly this:** `C0` was accepted on `viper-attrition`, which declares no
`time`. The first campaign that did — O4 — replayed **9 of 10 steps DIVERGED**. The cause was a state
field nobody had missed: `FBMissionCarry` had carried the campaign clock since the layer was built, and
`fb-gym --mission --state` had no way to receive it, so every standalone step of a clocked campaign ran
under no sky at all. The procedure found a missing field before any human noticed one was missing. A
wider world will hand it more of the same, and that is the reason to trust the procedure rather than a
format draft.

Two consequences worth stating plainly:

- **A field that changes no byte of any run is not world state.** It is decoration, and it belongs in
  whatever tool wants it — not in the snapshot.
- **A run whose fingerprint is not reproducible from its snapshot has a hole, and the hole has an
  address.** That is a stronger statement than any review of a format table.

### 3. What a snapshot must NOT contain

| Refused | Reason |
|---|---|
| anything **derivable** from what is already in it | two truths that can disagree. The tree's own instance: `ObjectivesMet()` is *defined* in terms of `StateOf`, so the vector and the verdict cannot diverge ([`missions/verdict.md`](missions/verdict.md)) |
| anything a unit could only know **through its sensors** — a track, a contact list, a threat picture, an identification | a snapshot is **ground truth**. Whoever turns it into a situation picture has walked around the perception boundary. The picture is built at runtime from published `FBState` blocks and nowhere else ([`sensors.md`](sensors.md); [`player-layer.md`](player-layer.md) §9.1: *"a map that reads the registry is not a map, it is the truth with a map's icon set"*) |
| a verdict, a score, a difficulty | the judges judge once, at runtime; a stored verdict is a second judge |

### 4. The class boundary — the one statement with consequences today

Two classes of actor, and the difference is total:

| | inside the mission window | outside |
|---|---|---|
| Physics | JSBSim, full resolution | a coarse row |
| Sensing | the six channels with their limits | none |
| Judges | `FBFlightMonitor` and `FBMissionMonitor` rule on it | none |
| Rate | 0.1 s | coarse |
| Count | dozens | millions |

**The rule, sharp:**

> **An actor is in the fine class if, and only if, the mission DECLARES it. Membership is fixed before
> the first tick and nothing at run time may change it — not distance, not a sensor detection, not a
> camera, not a player's attention. A coarse actor is not a dormant fine actor: it has no FDM, no
> module, no entry in `FBUnitRegistry`, and therefore cannot be detected, shot or judged.**

The only admitted growth of the fine cast is the one that already exists and is a child of a fine
actor: a released store spawned as a full unit at the end of a tick ([`missions/runtime.md`](missions/runtime.md) §6).

**Why membership and not distance.** The alternative — thin out what nobody is near — breaks two
principles at once, and both are already written down:

- *Determinism* (CLAUDE.md principle 4): if the culling radius, the frame rate or the viewer changes the
  result, the coupling is a bug.
- *The perception boundary*: **"out of range" is a statement about a sensor, never about the world.**
  A world that stops computing what is unobserved has made the observer a physical quantity.

**How a violation is checked** — four tests, all in shapes the tree already runs:

| # | Test | Shape it borrows |
|---|---|---|
| 1 | **Cast conservation.** Fine actors at any tick == declared `unit` blocks + stores released by them. Asserted, not inspected | the campaign overlay's postcondition (*"neither the block count nor the `set`-line count grew"*) |
| 2 | **Viewer independence.** The same mission run headless and with the native oracle attached, camera parked anywhere, must produce **one** fingerprint | the existing `--threads 1/2/4` × repetitions determinism gate, with the viewer as the varied axis |
| 3 | **Surroundings independence.** Two cuts from the same snapshot with the same declared cast but different coarse surroundings must be **byte-identical** | the two-runs-one-line-apart shape ([`player-layer.md`](player-layer.md) §9.5), inverted: here the difference must *not* show |
| 4 | **Structural.** The coarse table's header is `RESTRICTED` in `tools/verify_layers.py` with an **empty** outside-includer list, so no sensor, module or pilot can name it | `core/FBZone.h`, which is exactly that today |

### 5. Where the generator sits — OCEAN is a baker

FlightBox has this pattern three times already: an expensive, dependency-heavy tool runs **offline**,
outside the shipped tree, and the tree consumes its pinned artefact.

| Tool | Artefact | Dependency that never ships |
|---|---|---|
| `tools/bake_swiss_dem.py` (*"not a build target — run only on a change"*) | `assets/swiss-dem-90m.bin` | tile fetching, resampling |
| `tools/gen_air_decks.py` | eighteen JSBSim decks under `assets/aircraft/` | the recipe, in Python |
| the GRIB2 decoder in `tiles/` | `tiles/testdata/wx-…​.wxb`, checksummed | NOMADS, GRIB2 |
| **OCEAN** (`~/Git/ocean`) | a world snapshot | PyTorch, a GPU, DuckDB |

A world snapshot is the same case, and three worries collapse because of it:

| Worry | Why it is moot |
|---|---|
| **Determinism.** A GPU tensor sim over decades is not bit-reproducible across backends | it never runs inside a measured run. It runs once, offline; the **artefact** is the deterministic input, pinned by content hash. Same status the baked DEM has — with the defect that must not be inherited, see §7 |
| **Dependency.** PyTorch in a WASM client is absurd | it is never in the tree. `libfbcore.a` gains nothing, the browser gains nothing, the gates are unchanged |
| **Omniscience.** OCEAN's world state is global and total by construction | that is correct for an **authoring tool** and forbidden at runtime. The snapshot is read by the cutter — a tool — and by the spawn path. §3 and §4 test 4 are what keep it there |

**The division of labour, in one line:** OCEAN knows *who and where* — populations, factions,
settlements, real OSM buildings, decades of history. FlightBox knows *what it can do* — radar horizons,
weapon envelopes, runway headings, catalogue rows, team semantics. Nothing in OCEAN's model maps onto a
SAM belt or a fire-control channel, and nothing in FlightBox's maps onto a family.

### 6. Progressing the world — three options, and this file chooses none

Deliberately unresolved, per the owner. Named with consequences so that none is closed by accident:

| Option | Mechanism | Consequence |
|---|---|---|
| **A — offline epochs** | OCEAN re-runs from epoch *N* plus the folded-in mission results and emits epoch *N+1* as a new file | non-monotone progress (rebuilt, reinforced, reorganised) is expressible, because a new file is not an overlay. Price: the world moves in batches and not while anyone plays; and *N* campaigns flown from one epoch produce *N* conflicting deltas, so the fold-in needs a declared order the way `.fbc` declares mission order |
| **B — the campaign layer carries it** | widen the existing carry beyond the three monotone facts | nothing new to build and the fingerprint keeps working. Price: the carry test (monotone · already observed · has a declaration to land in) admits only monotone facts, and widening the overlay to *add* is the one change that destroys the `.fbm`'s authority. Fits a campaign; cannot express a world that heals |
| **C — a persistent server, one day per day, low resolution** *(the owner's own idea, explicitly an idea)* | a service advances the coarse state in wall-clock time | the world is alive between sessions, which is the point. Price: the coarse advance becomes a *live* input, so a run's provenance is a moment in a server's history rather than a file — and the tree's replay discipline rests entirely on inputs being files. The mitigation is known and is the same one `--elev tiles` needs and does not have: the server must be able to emit and be handed a pinned snapshot, or nothing flown against it is replayable |

The three are not exclusive. B is the built mechanism *inside* one campaign and would stay whatever else
is chosen; A and C differ in who advances the coarse state and whether the input is a file.

### 7. What must not be built shut

The actual assignment. Per candidate: open today or already closed, and the cost of keeping it open.

| # | Question | Today | Cost of keeping it open |
|---|---|---|---|
| **1** | Does the mission file assume a **closed world**, or can it be a window into a larger one? | **Open.** A `.fbm` declares its complete cast and knows nothing else — which is precisely what a *cut* produces. The cut belongs in a tool, before the parser | **zero today.** One rule to hold: never add a keyword meaning *"and everything else that exists"*. A generated `.fbm` is still a file, so the standalone-replay criterion survives |
| **1a** | …but can a window hold **dozens** of fine actors? | **Half closed, and it is an authoring constraint today.** `FBMissionRunner` ends the whole run at the first flight-monitor K.O. (`FirstFlightKo`); O1 lost a strike sortie at t = 237 s to a duel 90 km away | naming it. A world-cut window inherits it, and the fix — per-unit termination — is a real decision in `missions/`, not a tweak. Until then a window must separate its engagements in time, as O1 does |
| **2** | Is the campaign **state file** extensible, or is its format frozen? | **Open, additively.** Canonical text, parsed by `core/FBCampaignState`; new row kinds are an additive parse change | any added row moves every existing campaign fingerprint. Cheap if re-baselining happens **once, deliberately**, with the state file carrying a format version — expensive if it happens per round, because then no two published hashes compare |
| **3** | Does the **fingerprint** break when a run is born from a world state instead of a file? | **Open.** The mission fingerprint is over telemetry + normalised `events.log` + exit code; the *environment* is recorded beside it. A snapshot is one more environment input and fits that shape unchanged | **content-hash the snapshot from day one** and record the hash in `campaign-summary.txt`. Otherwise it inherits the DEM defect verbatim — `swiss_dem <path>` is a path, so re-baking makes two different grounds call themselves one ([`missions/campaign.md`](missions/campaign.md) Gaps). Hashing the snapshot properly makes fixing the DEM the same one-line habit |
| **4** | Are **unit ids and callsigns** viable across missions? | **Partly closed, and this is the sharpest one.** Identity is the callsign: 1–24 chars, unique *per mission*, and it names the telemetry file, the log attribution and the carry key. Measured consequence already: a controlled variant must not share a callsign with its control | a world of thousands needs stable identity across cuts. Two ways, both real work, neither taken here: the cutter **mints** deterministic callsigns from world ids and records the mapping in the generated file's header; or the state file gains an id column beside the callsign. Doing neither means world identity silently equals whatever the last cut named things |
| **5** | Can the runner take an **initial situation from outside**? | **Open, and already built** — `fb-gym --mission FILE --state FILE [--carry LIST] [--campaign-time ISO]`, proven by criterion 2. This is the single most load-bearing thing that is already right | every future state field arrives through **that same flag pair**, never through a client default. The clock hole is the precedent for what happens otherwise: a field that existed in the data model but had no way in produced nine false divergences |
| **6** | Can ground truth reach a **sensor**? | **Open and structurally guarded.** Six registry readers, pinned in `verify_layers.py`'s `RESTRICTED` table; the gate fails on a seventh and prints the number | put the snapshot/coarse-table header in the same table with an empty outside-includer list, the way `core/FBZone.h` already is. Cost: one table row, paid before the first line of code, not after |

---

## State

**Nothing built, and nothing scheduled.** What exists that a persistent world would consume:

| Piece | State |
|---|---|
| `campaign-state.txt` — canonical text snapshot, three carried facts | **built**, 2 campaigns, both criteria measured |
| the load path `fb-gym --mission --state --carry --campaign-time` | **built**, and it is the proof of criterion 2 |
| the overlay that only deletes, with its postcondition | **built**, narrower than its own spec |
| the environment record (`time`/`elev`/`swiss_dem`/`base`/`threads`) | **built**; the DEM is recorded by path, not by content |
| `tools/fb_campaign_verify.py replay` — the procedure of §2 | **built**, and it has already found one missing state field |
| geographic anchors: `zone` cylinders, `net` blocks, the nine-row ground catalogue, the eighteen-row air catalogue, OSM through `fb-tiles` | **built** |
| the perception boundary at six readers, gate-enforced | **built** |
| OCEAN as a working offline generator | **exists**, `~/Git/ocean` — and emits nothing FlightBox-shaped: its DuckDB schema knows `npcs`, `pois`, `events`, and no unit, base or site |

**What does not exist, explicitly:** no coarse tick · no world state above the campaign · no class
boundary in code · no cutter · no epoch artefact · no writeback path beyond the three carried facts.

**The "millions" figure is a claim about the COARSE class only** — a table with no physics, no sensors
and no judge. Where the *fine* class tips is being measured right now by a parallel round
(`sim/tools/fb_scale_bench.py` over `sim/missions/scale/`); no number is quoted here, and none is
invented. The only cast-related number the tree owns today is about threads, not size: 4 units reach
1.49×/1.53×/1.77× at 2/3/4 threads on an A18 Pro ([`missions/runtime.md`](missions/runtime.md)), and it
answers a different question.

## Gaps

| Gap | Detail |
|---|---|
| **The snapshot is not designed, on purpose** | §2 says it is *worked out* by the replay procedure. Any field table written before that procedure runs is a guess wearing a table's clothes |
| **Identity across cuts is unsolved** | §7 item 4. The callsign is the only identity, it is mission-scoped, and it is the carry key. This is the first thing that will hurt |
| **`FirstFlightKo` bounds a window's size** | §7 item 1a. Per-unit termination is a decision in `missions/`, not a detail |
| **The environment record hashes nothing** | §7 item 3. Inherited today for the DEM; a snapshot makes the same defect much larger |
| **The progression question is open by decision** | §6. Three options, no recommendation, per the owner: *"momentan geht es darum, Optionen offen zu halten"* |
| **Rejected: promoting an actor to the fine class at run time** | §4. It would make the observer a physical quantity and break the determinism principle and the perception boundary in one move. Kept here with its reason so nobody re-derives it as an optimisation |
| **Rejected: importing OCEAN's runtime layer** | its rumour propagation, LLM voicing and live quest evaluation are a *second* truth computed beside the simulation. FlightBox's rule is one truth with views over it ([`vision.md`](vision.md)); OCEAN's runtime half stays in OCEAN |
| **Not addressed: the player layer's unlock rule** | today unlock follows the `.fbc` order ([`player-layer.md`](player-layer.md) §4.2). In a persistent world it could instead follow the *situation* — which is OCEAN's own "quests are constraints on the world state, not scripts". Two prices, named as an outlook only: generated missions have no human-written header reading rule, and difficulty is **declared with a recorded reference run, never computed**, so every generated mission would owe one AI run before it may be offered. This file changes nothing in `player-layer.md` |

## Knowledge

- **Why `campaign-state.txt` already IS a coarse world state.** Three columns — a unit's alive bit, a
  ground target's alive bit, a store count — geographically anchored by the mission files that consume
  it, ordered canonically, hashable, and load-proven. Widening it is a change of degree.
- **Why the fingerprint is the right completeness test.** It compares *everything the run produced*
  against *everything a reconstruction produces*, so it cannot be satisfied by an incomplete description
  that happens to look plausible. It found the campaign clock; it will find the next one.
- **Why the offline/runtime split is the same one the tree already makes three times.** DEM, air decks
  and the weather fixture all put an expensive dependency outside the shipped tree and pin its output.
  The only new thing about a world snapshot is its size.
- **Why "millions" and "dozens" are not a contradiction.** They are two different objects. The coarse
  row is a fact about the world; the fine actor is a physics integration with six sensor channels and two
  judges. Calling both "a unit" is what makes the sentence sound impossible.
