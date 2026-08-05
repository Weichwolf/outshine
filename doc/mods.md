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

One declaration, and the sim never learns which reader it is in. That is the same boundary as everywhere
else in this tree: the GUI is a client on the simulation, never a second truth.

**And a protocol (§2.1) adds a third reader that was not available before.**

> Owner, 2026-08-05: *„die Missionen können damit auch als Demo in der WASM-App laufen."*

| Third reader | Who flies | What it is for |
|---|---|---|
| **replayed** — the WASM app, mission **+ trace**, no model | the recording | the demo — and, unexpectedly, a test the other two cannot perform |

Every mission ships as an attract-mode demo for free, which is how these four titles did it in 1994 and
costs nothing here: the sim is deterministic given its inputs, so mission + trace reproduce the recorded
run exactly, this time rendered.

**The unexpected part is the test.** `fb-gym` and the WASM app have never had a shared checkable artefact
— the gym has no renderer and the browser has no baseline. Now they do: **run the same mission with the
same trace in both, and the telemetry must be identical.** If the browser diverges, the defect is in the
client, and it is named by the tick where the hash sequences part. That is the first differential between
the two clients over a whole mission rather than a frame.

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

### 2.2 The same shape spans simulation and role-playing

> Owner, 2026-08-05: *„das geht von der Simulation bis zum Rollenspiel. Auch Rollenspiel-Dialoge und
> Entscheidungen lassen sich so abbilden."*

This is the genericity claim's sharpest test, and the mapping is exact rather than approximate:

| Simulation | Role-playing | Same artefact |
|---|---|---|
| a radio call | an NPC's line | pool entry |
| a mission's course | a conversation | trace |
| a wingman's decision to engage | an orc's decision to swing | a call on the module's declared list |
| the mission goal + reading rule | the quest and its completion | `.fbm` |
| „Fox two" reused across titles | a greeting reused across NPCs | one pool entry, shared |

Nothing is bolted on. A dialogue system is a protocol whose outputs happen to be sentences, and a quest
is a mission whose goal happens to be narrative — which is why this belongs here rather than in some
future `dialogue.md`.

**Two differences, and both are real:**

1. **A conversation branches; a flight does not.** A mission's trace is one ordered sequence. A dialogue
   forks at every player choice, so its trace is a **DAG, not a list** — again Git's shape, commits with
   parents. The pool is untouched by this; only the trace's type changes.
2. **The hashed input gets harder, not easier.** In a sim, the state that matters is physical and
   naturally bounded: position, fuel, contacts, damage. In an RPG it is *narrative* — what this character
   knows, what was already said, how the relationship stands — and narrative state has **no natural
   bound**. Everything ever said is potentially relevant.

So the role-playing case does not validate the design; it **stresses** it, at exactly the point already
marked as the hardest in `## Gaps`. That is the useful result: the input-selection question must be
answered with the RPG case in view, or it will be answered in a way that only works for aircraft.

**And that fixes the shape of an output**, which had been left vague until now:

> Owner, 2026-08-05: *„ein Response hat dann immer: was sagt der Akteur und was macht der Akteur
> (function calling)."*

Every pool output is a **pair — `say` and `do`** — and both may be empty:

| | `say` | `do` |
|---|---|---|
| a wingman calling a shot | „Fox two" | `ReleaseWeapon(AIM-120, trk 3)` |
| a silent break turn | — | `SetBank(-80)` |
| an NPC refusing | „I'll not speak of it." | — |
| an orc | a grunt | `Swing(club, target)` |

This is why the sim and the RPG need no separate machinery: **a line without an action is a
conversation, an action without a line is a manoeuvre, and both together are the normal case.** It also
keeps the anti-cheat property exactly where it was — `do` is a call on the declared list and can be
nothing else, while `say` reaches no state at all.

### 2.3 Two implementations, one interface

> Owner, 2026-08-05: *„im Prinzip kannst du einen OpenRouter-kompatiblen LLM-Service erstellen, der aus
> der DB die korrekten Antworten heraussucht. Es geht nur darum, das LLM deterministisch zu simulieren
> und die Rückgaben tunen zu können. Das läuft extrem schnell für das Gym."*

The obvious implementation gives the engine two paths — *„if replaying, read the trace; else call the
model"* — and that branch is the defect. It is a second execution surface, it is only exercised in one
mode, and the two halves drift.

**The fix is not a service — it is the tree's own layering.** One interface, two implementations, chosen
at construction:

> Owner, 2026-08-05: *„na du kannst schon zwei Implementationen vom LLM-Interface in C++ machen. Einmal
> online und einmal Database."*

```
FBActorVoice                 the interface: request → {say, do}
  FBActorVoiceOnline           OpenRouter-compatible HTTP; used when recording and in played runs
  FBActorVoicePool             reads the pool by hash; in-process, no network at all
```

> Owner, 2026-08-05: *„Gym und Demo-Mode initiieren einfach Database-Mode, und Real Play braucht
> OpenRouter."*

**The client constructs it; core never chooses.** Same rule as `FBFdmBoot`, the single state writer named
only by `missions/` and `clients/`:

| Client | Constructs | Reaches a model |
|---|---|---|
| `fb-gym` | `FBActorVoicePool` | never |
| WASM, demo | `FBActorVoicePool` | never |
| WASM, played | `FBActorVoiceOnline` | yes |

And this turns the guarantee from a promise into a **link-time gate**: `fb-gym` does not link
`FBActorVoiceOnline` at all, so `nm build/fb-gym` must show zero HTTP-client symbols — measured exactly
like the existing *„zero Dawn/WebGPU symbols"* gate. A gym that could reach a model would fail to build
that way, rather than being trusted not to.

The caller holds an `FBActorVoice&` and cannot tell which it has — the same shape as
`FBCore → Interface → Default → Override` everywhere else. What is forbidden is an `if (replaying)`
**inside the caller**; a second implementation behind the interface is the opposite of that.

`FBActorVoicePool` is the better choice for the gym for reasons a service could not give:

| | in-process pool | local HTTP service |
|---|---|---|
| cost per call | a hash lookup | a round trip, even on loopback |
| processes to start | none | one, and it can fail |
| WASM | links and runs | cannot host a server |

The OpenRouter compatibility stays where it earns its keep: **`FBActorVoiceOnline` speaks it**, so
recording is the real thing and any provider works. And recording is that same class writing each pair as
it passes — one component, not a proxy to operate.

Four things follow, and the second is why it is worth building rather than special-casing:

1. **Extremely fast.** A hash lookup instead of a round trip; the gym keeps running at simulation speed
   rather than model speed, which is the difference between a campaign in seconds and one in hours.
2. **The gym cannot drift from live behaviour**, because there is nothing to drift *from* — the request
   the gym sends is byte-identical to the one live play sends. This is the same structural argument the
   tree uses everywhere: make the wrong thing unrepresentable rather than forbidden.
3. **The returns are tunable.** Editing an entry makes a unit decide differently on the next run — a
   lever for experiments that touches neither engine code nor a model. That is the doctrine-evolution
   loop applied to language instead of gains.
4. **Recording is not a separate tool.** `FBActorVoiceOnline` writes each pair as it passes; the pool it
   fills is what `FBActorVoicePool` later reads.

The hard rule from §2.1 lands as a **property of the class, not a policy in the caller**:
`FBActorVoicePool` has no fall-through to write — it holds no client, no URL and no socket. A miss
returns an error and the run fails. The gym cannot reach a model because the object it holds has no way
to reach one.

### 2.4 System prompts, the rung boundary, and the Game Master's trap

> Owner, 2026-08-05: *„im Database-Mode natürlich ohne System-Prompt. Im Online-Mode braucht jede
> intelligente Entität einen System-Prompt. Aber LLM-Intelligenz nur für taktische Entscheidungen und zur
> Kommunikation mit dem Spieler. Ich kann mir auch eine Art Game Master vorstellen, über den alles geht.
> Prompt Caching, Kontext."*

**No system prompt in database mode.** `FBActorVoicePool` looks up bytes; a system prompt would be one
more thing to keep in sync with a recording that already contains its effect. It is not that the prompt is
optional — it is that the *lookup* has no use for it. `FBActorVoiceOnline` owns it entirely.

**The rung boundary is now stated, and it is a performance guarantee as much as a design one:**

| Layer | Decider | Why |
|---|---|---|
| flight control, gunnery, evasion geometry | **regulation** — never an LLM | a sim tick is milliseconds; a model is not, and a control law is *better* at it |
| tactical choice — engage, disengage, which target, when to go silent | **LLM** | the layer where judgement beats a gain |
| speaking to the player | **LLM** | the only layer where language is the output |

This is the ladder from *„KI kann von einfacher Regelung bis LLM-Integration skalieren"* given a cut line.
It also removes the last objection to live play: the fast layer never waits on a model.

#### The Game Master, and the trap in it

One actor through which everything passes is attractive for three real reasons: coherence (it knows the
story), cost (one context instead of N), and **prompt caching** (a long stable prefix, hit on every call).

**And it is, stated plainly, the cheating vector this tree exists to forbid.** A GM that sees the whole
world and then tells an entity what to do has given that entity sight through terrain — the entity did
not perceive it, so the sensor boundary is bypassed without a single line of code touching the registry.
`doc/architecture.md`'s six-file perception boundary would still pass its check and the property would be
gone.

**The resolution is to split what the GM shares from what it sees:**

| | Content | Per entity? | Cacheable |
|---|---|---|---|
| **prefix** — the GM's part | rules, world, doctrine, the scenario's tone, the mission's frame | no — **identical for all** | **yes, and this is exactly what prompt caching wants** |
| **suffix** — the entity's part | *its own* sensor picture, its state, its orders | yes | no |

So the GM is **the shared prefix, not a shared observer.** It supplies context that a briefing would have
supplied anyway — things every participant may know — and never the position of a contact the entity has
not detected. The varying part of every call is that entity's own picture, which is the same rule as
`FBUnitRegistry`'s six readers, applied one layer up.

This is not a compromise against caching; it is what makes caching work. The stable prefix is the part
worth caching, and the part that must vary per entity is the part that must not be shared anyway.

**The check must be structural, not editorial.** A prompt is a string, and a string can contain anything —
so the entity's suffix must be *built from the same perception structs the regulator reads*, never from
the registry. If the code that renders a prompt cannot see the registry, a GM cannot leak through it. That
file therefore belongs under the six-file rule, and `verify_layers.py` must count it.

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
3. **The hashed input is the literal prompt, and that is the whole answer.**

   > Owner, 2026-08-05: *„hier geht es ja nur um die Reproduzierbarkeit für Gym und Demo-Mode. Im
   > Spielerbetrieb entscheidet dann eben das LLM."*

   This scoping removes a design problem rather than solving one. Asking *„which parts of the situation
   matter enough to hash?"* would be genuinely hard — bound it too tightly and a protocol answers a
   question it never saw, too loosely and every pair misses on the next tick. **The question does not
   arise**, because a protocol is a recording, not a dialogue system: hash **the bytes that were actually
   sent to the model**. Nothing is selected, so nothing can be selected wrongly.

   It works because the sim is deterministic: the same mission reproduces the same prompt, so the lookup
   hits. When the sim changes enough that the prompt differs, the hash misses — which is precisely the
   divergence signal, arriving for free instead of being engineered.

   *This correction shrinks what an earlier revision called the hardest open question. It was overstated:
   it is only hard for a live dialogue system, and §2.1 does not build one.*

4. **A miss is a signal, and the two sides handle it differently — this is a hard rule.** In a played run
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
  `fb-gym`; nothing records one and nothing replays one.
- **The prompt-rendering file has no home and no guard yet.** §2.4 requires that whatever builds an
  entity's prompt suffix cannot see `FBUnitRegistry` — otherwise a Game Master leaks perfect knowledge
  through a string. `verify_layers.py` counts six perception readers today and would not notice a
  seventh that arrived as a prompt builder.
- **How a played run is judged at all is unwritten** — the judges produce a verdict, but a game needs a
  score, and this tree has never had to say what a good run is as opposed to a passing one.
- **The module declaration list is not machine-readable yet.** §2.1 rests on it being one artefact; today
  it is C++ virtual overrides, which an LLM cannot be handed as a tool schema.
