# Mods — a scenario is a very large test specification

> Owner, 2026-08-05: *„daraus folgt auch, dass die `mods/` komplett deklarativ sind. `mods/` sind einfach
> sehr grosse `test/` Spezifikationen."* · *„Szenarien sind `mods/` auf die Outshine-Engine."*

Spec-first. Nothing here is built; `## State` stays empty until it is.

## Spec

### 0. The claim, and why it is not a metaphor

A test in this tree is data: `{subject, claim, measure, expect, source, tier}` — see
[`testing.md`](testing.md). A mission is already the same shape and has been for months:

| A test declares | A mission declares |
|---|---|
| `measure` — a harness plus its arguments | units, positions, loadout, weather, time of day |
| `expect` — a value with a band | the goal, and the reading rule in its header |
| the runner compares, the test does not judge itself | `FBMissionMonitor` judges from its **own plan copy**, never from the sim |

**Same artefact, three orders of magnitude apart.** `sim/test/sensors/visual.json` asserts one number;
`mods/comanche/missions/01.fbm` asserts that a helicopter, flown by an AI over real terrain, reaches a
valley and comes back. Neither contains a program. Both are judged by something that cannot be talked
into agreeing.

So the consequence is structural, not rhetorical: **`mods/` lives under the same rule as `test/`, and
running a campaign IS running the acceptance net.** The 100 % coverage the goal demands is not a chore
beside the four titles — the four titles are the coverage.

### 1. Where the equation breaks, and how it is repaired

**A test has a verdict; a game has an outcome.** A test must be binary and reproducible. A game must be
*losable* — a player who dies in mission 3 has not falsified the engine. Collapsing the two would make
every defeat a red gate, which is nonsense.

The repair already exists in the tree and is the strongest evidence the idea is right:

| Same `.fbm`, two readers | Who flies | What the verdict means |
|---|---|---|
| **scripted** — `fb-gym`, headless, deterministic | the pilot AI | **this is the test.** Byte-identical over `--threads 1/2/4`; a changed result is a regression |
| **played** — the WASM app | the human | this is the game. The judges still run; their verdict is the player's score, not the engine's |

One declaration, two consumers, and the sim never learns which one it is in. That is the same boundary
as everywhere else in this tree: the GUI is a client on the simulation, never a second truth.

### 2. The consequence that turns this into a work plan

If a mod is **fully** declarative, then anything the four titles need that cannot be declared is not a
mod problem — **it is the engine's backlog, and the list of undeclarables IS the Stufe-A plan.**

That makes the four campaigns a measuring instrument for genericity rather than four content jobs. Known
undeclarables today, from the four titles:

| What the title needs | Why it is not declarable yet |
|---|---|
| Comanche: a rotorcraft | the body format ([`body-format.md`](body-format.md)) spans it in principle; nothing implements it |
| Armored Fist: a tracked vehicle on terrain | contacts and drive torque are declared in the format, absent in code |
| Delta Force: a man on foot | ditto, plus a segment tree with foot contacts that come and go |
| all three: ground seen from ten metres | terrain, buildings and foliage at that scale |
| all four: a per-title HUD | the HUD is C++ today, not a declaration |
| all four: mission briefing text and objectives | `.fbm` has a reading rule, not a briefing |
| `FBSystemId` is a closed 14-entry aircraft enum | blocks every non-aircraft downstream |

**The list is the deliverable of the first round**, not the campaigns. A campaign that required a patch
to the engine is a campaign that measured a hole — and the hole is worth more than the mission.

### 2.1 Where declaration ends: no script language, function calling instead

> Owner, 2026-08-05: *„eine Scriptsprache bauen wir nicht ein, aber LLM-Integration mit Function
> Calling. Jede Einheiten-KI kann später LLM integrieren."*

Every declarative engine hits the same wall: a scenario eventually wants *behaviour* that is not a
number. The industry answer is an embedded script language — Lua, GDScript, Papyrus. **This tree does not
take it**, and the reason is not taste: a script language is a second execution surface that can reach
into state, which is exactly what this tree has spent its structure forbidding (`FBFdm`'s private loading
ctor, `FBSystemHealth`'s monotone health, the six files that may see the unit registry). A mod with a
script is a mod that can cheat.

**The escape hatch is a function-calling surface, and it is a surface that already exists.** The owner's
module rule — *„`FBModule` muss dumm sein. Wer davon erbt, sagt, was er kann"* — produces a declaration
list per unit: the things this module *can do*. That list has exactly two consumers:

| Consumer | What it does with the same list |
|---|---|
| the control loop (today) | binds each entry to a control law — the rung ladder from simple regulation upward |
| an LLM (later) | receives the same list **as its tool schema** and calls into it |

One artefact, two readers, and the anti-cheat property is inherited rather than re-argued: **an LLM can
call only what the module declared, which is only what a pilot could do.** No `SetPosition` exists to be
called. The boundary is not a sandbox around the language — there is no language.

This also fixes the scaling the owner named earlier (*„KI kann von einfacher Regelung bis
LLM-Integration skalieren"*): the ladder is not a rewrite at each rung, because the interface at every
rung is the same declaration list. A unit whose AI is a PID today and an LLM tomorrow changes its
*decider*, not its *reach*.

**And the determinism collision is not a compromise — it is a boundary.**

> Owner, 2026-08-05: *„Gym hat nie LLM-Integration."*

An LLM is orders of magnitude slower than a sim tick and is not reproducible, which would collide head-on
with §1's byte-identical gate. It does not, because the two readers of §1 are also the two sides of this
line:

| | Decider | Determinism |
|---|---|---|
| **`fb-gym`** — the test | regulation, or a **fixed protocol** (below); never a live LLM | byte-identical over `--threads 1/2/4`, the gate stands untouched |
| **the played run** | may be an LLM per unit AI | not required and not claimed |

The gate is not weakened, and the LLM is not crippled — they simply never meet. **What both sides share
is the declaration list**, so the anti-cheat property holds identically in each: an LLM in the played run
reaches exactly as far as the regulator in the gym, because it is the same list.

**But the LLM decider is not therefore unmeasurable** — it becomes measurable the moment its decisions
are treated the way this tree treats every other claim.

> Owner, 2026-08-05: *„ich korrigiere mich. Gym kann feste LLM-Protokolle laden."*

A **protocol** is the recorded decision trace of an LLM run: at tick *T*, unit *U* called function *F*
with arguments *A*. Recorded once from a played run, it is thereafter **data** — and `fb-gym` loads it
like it loads a mission, replays it, and stays byte-identical. The live model is never in the loop; its
output is.

That is the same move as `envelope.json`: *an expectation is a datum, not a program.* **A decision is a
datum too.** The rule does not gain an exception; it gains a second application.

| | What is in the gym | Deterministic |
|---|---|---|
| regulation | a control law | yes |
| **a protocol** | **a recorded decision trace** | **yes** |
| a live LLM | — | never |

What this does and does not prove, stated exactly:

- **Does:** the consequences of those decisions in *today's* simulation. A protocol that produced a kill
  last month and produces a crash today has found a regression — and it is a far sharper probe than a
  regulator, because an LLM makes decisions no control law would.
- **Does not:** that the model would decide the same way now. A protocol is a fixed trace, not a live
  agent; re-running the model is a new recording, not a test.
- **Ages usefully.** When the sim changes enough that a protocol becomes nonsensical (the unit is dead at
  tick *T*), the replay diverges — and divergence is the signal, not a failure of the protocol.

### 3. What a mod directory contains

> Owner, 2026-08-05: *„`mods/` haben ihr eigenes `doc/`."*

**A mod is a whole triad, not a directory with documentation attached.** It carries its own `doc/`,
`src/` and `test/`, and they mean inside a mod exactly what they mean outside it:

```
mods/<title>/
  mod.json          identity, which core capabilities it claims, its map style
  doc/              WHAT WE WANT — the original, reconstructed and sourced
    campaign.md       mission order, objectives, the reading rule per mission
    terrain.md        the real bounding box per mission AND why this real place
    hud.md            the original's HUD, element by element, from the manual
    sources.md        every claim with URL + page; manual-vs-wiki contradictions kept, not smoothed
  src/              WHAT WE CAN — declarations only, no code
    units/  modules/  missions/*.fbm  hud/
    protocols/        pool/ (hash→content) + traces/ (hash sequences per mission), §2.1
```

> Owner, 2026-08-05: *„du kannst auch in den `mods/` feste LLM-Protokolle ablegen, die dann offline
> laufen. So können wir LLM-Integration deterministisch testen."*

`protocols/` sits under `src/` and not beside it for the same reason nothing else does: **it is a
declaration.** A trace is data, `fb-gym` replays it offline with no model reachable, and the run is
byte-identical like every other. That the mod has no `test/` is not weakened by this — the protocol is
one more thing the mod *is*, judged by the same monitors as the mission it belongs to.

The property this buys is the one that looked out of reach two paragraphs ago: **LLM integration becomes
deterministically testable.** Not the model — the integration. Whether the tool schema still binds,
whether a decision still reaches its system, whether the consequences still land where they landed.

**And a protocol entry is not limited to function calls — it carries language.**

> Owner, 2026-08-05: *„so können wir auch einfach textuelle/gesprochene Kommunikation speichern. Als
> In-Out-Paare gehasht."*

> Owner, 2026-08-05: *„bzw. gehashte Strings in einer Tabelle und Verläufe mit nur Hashes in einer
> anderen."*

**Two tables, and the split is the whole trick:**

| | Holds | Shape |
|---|---|---|
| **pool** | the content — prompts, replies, radio lines, audio blobs | `hash → bytes`, written once, never mutated |
| **trace** | the course of a run | an ordered list of **hashes only** |

This is Git's own model — objects and the commits that reference them — and it buys four things:

1. **Deduplication.** *„Fox two"* spoken four hundred times is stored once. Audio, which dominates the
   volume, is stored once per distinct utterance rather than once per occurrence.
2. **A trace becomes comparable byte for byte.** Two runs' traces are two hash sequences; the existing
   differential net compares them with the code it already has. A regression shows up as *a mismatch at
   position N*, which names the tick.
3. **A trace is readable without the payload.** It diffs, it fits in a commit, it can be reviewed. The
   pool can be large and nobody has to look at it.
4. **The pool is shareable across mods.** A radio phrase used by three titles has one entry.

One shape covers the content: **`hash(input) → output`**, content-addressed like a Git object. The input is
the situation the model was given; the output is what came back — a function call, a line of radio, or a
rendered audio blob for it. Wingman chatter, ATC, a briefing read aloud, an enemy commander's order: all
the same pair.

Three properties follow, and the third is the one that makes it trustworthy:

1. **Voice costs nothing at runtime and is deterministic.** Speech is synthesised once at record time and
   stored beside its pair. Replay reads a file.
2. **The hash IS the boundary.** One bit different in the situation and the lookup misses — so a protocol
   can never silently answer a question it was not asked.
3. **A miss is a signal, and the two sides handle it differently — this is a hard rule.** In a played run
   a miss may fall through to a live model. **In `fb-gym` a miss is a failure, never a fallback**, because
   the gym must have no path to a model at all; a gym that could fall through would be a gym whose
   determinism depends on a network. The miss is the useful output: it names exactly where the world
   drifted away from what was recorded.

**And there is no `test/`.**

> Owner, 2026-08-05: *„`mods/` haben keine `test`, da die Missionen auch im Gym laufen."*

This is §0 taken seriously rather than half-way. If a mod *is* a test, a `test/` beside it is the same
assertion written twice — and this tree already knows what a duplicated statement does: the two copies
drift, and the one nobody runs is the one that lies.

**Why the engine needs the split and a mod does not:**

| | The subject is | Can it assert about itself? |
|---|---|---|
| `sim/src/` | **code** | No. C++ cannot state its own expected corner speed without becoming the thing that decides whether it passed — measured, that is exactly how seven anchors sat outside their bands behind a green gate |
| `mods/<title>/src/` | **data** | **Yes.** A `.fbm` carries its `measure` (units, terrain, loadout, weather) and its `expect` (the goal, plus the reading rule in its header) in one file, and `FBMissionMonitor` judges it from a copy the mission never sees |

So the rule generalises rather than gaining an exception: **every subject needs its intent in `doc/` and
a proof that cannot be talked into agreeing.** Where the subject is code, the proof must live apart.
Where the subject is already a declaration, the proof is the declaration — and `fb-gym` is the runner.

Consequences:

1. **`verify-trees` must know one bit** — that a mod is two-tree, not three. I claimed the opposite one
   revision ago and it was wrong: the tool cannot be told „every directory needs three" and also be
   right about `mods/`. What it *can* be told without knowing what a mod is: **doc plus proof**, where
   proof is `test/` for a code subtree and a runnable `.fbm` for a declaration subtree.
2. **The engine's `doc/` does not document mods**, and a mod's `doc/` does not document the engine. Same
   boundary as `src/`: a mod may not reach into the engine, and the engine may not know which mod is
   loaded.
3. **The reconstruction has a home.** *Why this real valley for that fictional one* is neither engine
   knowledge nor a verdict — it is this scenario's intent, and `mods/<title>/doc/terrain.md` is the only
   place it can live without polluting the other axis. Same for a manual's page number.

### 4. Acceptance

| Contract | Anchor |
|---|---|
| No engine code per title | `mods/*` contains zero `.cpp`/`.h`; checked by a tool, not by intent |
| A mod is a test | every mission runs headless in `fb-gym` with a verdict, and byte-identically over thread counts |
| The played run cannot cheat | the WASM app consumes the identical `.fbm` and both judges run in it too |
| The triad holds inside a mod | `verify-trees` walks `mods/<title>/` with the same rule |
| The undeclarables are named | each round publishes what the titles could NOT declare — that list is the engine backlog |

## State

Nothing built.

## Gaps

- **`mods/` does not exist**, and neither does the loader that would make `mod.json` mean anything.
- **`verify_trees.py` enforces three trees everywhere** and would therefore report every mod as a hole.
  It needs the doc-plus-proof rule from §3, and today it does not have it.
- **The HUD is C++**, so „HUD per title, declared" has no surface to be declared into.
- **Three of the four titles are not aircraft**, and the body format that would carry them
  ([`body-format.md`](body-format.md)) is spec-only.
- **Whether a briefing belongs in the mission or beside it** is undecided; the `.fbm` header carries a
  reading rule for a machine, not prose for a player.
- **The protocol format does not exist.** §2.1 and §3 rest on recorded in→out pairs being loadable by
  `fb-gym`; nothing records one and nothing replays one. **What exactly goes into the hashed input is the
  whole design** — too much and every pair misses on the next tick, too little and a protocol answers a
  situation it never saw. Undecided, and cheap to decide wrongly.
- **How a played run is judged at all is unwritten** — the judges produce a verdict, but a game needs a
  score, and this tree has never had to say what a good run is as opposed to a passing one.
- **The module declaration list is not machine-readable yet.** §2.1 rests on it being one artefact; today
  it is C++ virtual overrides, which an LLM cannot be handed as a tool schema.
