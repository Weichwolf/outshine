# Mods — a scenario is a very large test specification

> Owner, 2026-08-05: *„daraus folgt auch, dass die `mods/` komplett deklarativ sind. `mods/` sind einfach
> sehr grosse `test/` Spezifikationen."* · *„Szenarien sind `mods/` auf die Outshine-Engine."*

Spec-first. Built so far: `mods/f16/`, the one scenario the engine used to be — see `## State`.

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
| **watched** — the WASM app, live model, no player | the AI, deciding now | *„eher Twitch"* — the 2026 deliverable. Not deterministic and not claimed to be |

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
take it**, and the reason is not the one an earlier revision gave. It is *not* that code could cheat —
the guards are type-level, not location-level: `FBFdm`'s private ctor names one friend and a mod cannot
add itself to that list. The real reasons are two. **Deployment:** data is generatable without a
compiler, cannot crash the engine, and does not force a WASM rebuild. **Genericity:** a mod that ships
its own rotorcraft physics proves the engine is a framework, not a motor — a helicopter must be a
*declaration* (segments, joints, force sources), never a class. So a mod that would need `.cpp` is a
signal that **the engine lacks a capability**, which is exactly the undeclarables list in §2.

Shaders are the exception that proves it: a shader sees only its declared bindings, cannot link, cannot
call, cannot reach the registry — the same structural bound as the LLM's tool schema. A mod may ship
shaders for the appearance of its own entities, because appearance is not knowledge.

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

**And the cache is per entity — obligatory, not optional.**

> Owner, 2026-08-05: *„ja du hast recht, aber jede intelligente Entität muss Prompt-Caching verwenden."*

Which closes the trap for good, because it means **there is no shared context object at all — only
shared text.** Each entity holds its own cached prefix:

```
[cached, per entity — long, stable, grows with the run]
  the GM's world frame          copied in, identical wording for everyone
  who this entity is            identity, allegiance, temperament
  what it CAN                   its declaration list — the same one the regulator binds
  its orders and doctrine
  its own history               what it has seen, said and decided so far
[varying — short]
  its latest perception delta
```

Three consequences:

1. **Leakage is structurally impossible, not merely avoided.** A shared context is an object two entities
   could both read. Copied text is not — entity B's cache cannot contain what only A perceived, because
   nothing ever put it there.
2. **Cost falls where it actually hurts.** The expensive part of a tactical call is the long stable
   preamble, and it is cached; the part that changes each call is a short delta. Without this, live play
   would re-send an entity's whole identity every few seconds.
3. **History belongs in the prefix, so caching grows with the run.** An entity that has been flying for
   ten minutes has a longer cached prefix and a *cheaper* call than at spawn — the opposite of the naive
   shape, where a growing conversation gets steadily more expensive.

**The check must be structural, not editorial.** A prompt is a string, and a string can contain anything —
so the entity's suffix must be *built from the same perception structs the regulator reads*, never from
the registry. If the code that renders a prompt cannot see the registry, a GM cannot leak through it. That
file therefore belongs under the six-file rule, and `verify_layers.py` must count it.

### 3. What a mod directory contains



> Owner, 2026-08-05: *„`mods/` haben ihr eigenes `doc/`."* · *„`mods/common/` würde ich lieber nicht.
> Dann lieber Abhängigkeiten auf andere Mods. Das ist ja auch der Mod-Gedanke."*

**No shared bucket.** An asset lives in the mod that needed it first; others declare `depends`. A
`common/` becomes a junk drawer, while an explicit dependency stays honest about who needs what — and it
makes `mods/f16` a dependency for anything wanting an F-16 or a MiG-29 rather than a special case.

Measured: only **5 of 93** assets across the four titles are shared at all
([`asset-inventory.md`](asset-inventory.md)), so a shared bucket would have been mostly empty anyway.

**A mod is a whole triad, not a directory with documentation attached.** It carries its own `doc/`,
`src/` and `test/`, and they mean inside a mod exactly what they mean outside it:

```
mods/<title>/
  mod.json          identity, capabilities claimed, map style, and `depends` on other mods
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

### 3.1 The browser must be able to pick, and the picking is data too

**Four titles in one WASM app is the goal, so the browser cannot be built for one.** A client that bakes
`/fb/aircraft`, `/missions/<name>.fbm` and `AddUnitModel("f16", …)` has the scenario compiled into it —
the same defect §3 removed from the engine, one layer up.

The rule is therefore the same rule: **the manifest is the only place a directory or a type key is
named**, and it must reach the browser instead of being read at build time and forgotten.

| Half of a mod | How the browser gets it | Root |
|---|---|---|
| manifest, aircraft, meshes | preloaded into the virtual filesystem by `make wasm` | `/fb/mods/<id>/` |
| missions, campaigns | fetched over HTTP from `web/` | `/mods/<id>/` |

Both halves keep **mod.json's own relative directories**, so ONE manifest resolves in both mounts and no
path exists twice. Which mod is a name like which mission is a name: it comes from the player layer
(`window.FB_MOD`), is sanitised, and falls back to the first line of a generated index — so the engine
still names no mod and a build with one behaves like a build with five.

**Meshes are declared, not discovered.** `"meshes"` lists the MODULE REGISTRY KEYS that ship an
airframe: the key is what a unit publishes as its visual type, the sidecar beside it names its own `.glb`
levels, and the build derives the preload list from those two. A second mod with other meshes therefore
needs no line of C++ and no line of Makefile.

### 4. Acceptance

| Contract | Anchor |
|---|---|
| No engine code per title | `mods/*` contains zero `.cpp`/`.h`; checked by a tool, not by intent |
| A mod knows only what it perceives | *„Outshine ist Gott und weiß alles. In den `mods/` ist, was nur kennt, was es kennt."* — owner, 2026-08-05. The engine builds and simulates the world and is omniscient in it; a mod is a participant. Same boundary as `FBUnitRegistry` against the modules, one level up |
| A mod adds no world | Earth — terrain, infrastructure, buildings — belongs to the engine ([`persistent-world.md`](persistent-world.md) §5.1). A mod adds actors, entities, usable objects and a scenario |
| A mod asks, it does not instruct | no mod names an LOD, a triangle budget or a draw call. It declares what exists and what matters; the engine holds 720p30 and spends the rest on quality ([`render/visual-target.md`](render/visual-target.md) §1.1) |
| A mod is a test | every mission runs headless in `fb-gym` with a verdict, and byte-identically over thread counts |
| The played run cannot cheat | the WASM app consumes the identical `.fbm` and both judges run in it too |
| The triad holds inside a mod | `verify-trees` walks `mods/<title>/` under §3's rule — two trees, doc plus proof — and counts those orphans apart from the engine's |
| The client is title-free | no client names a mod, a directory, a mesh file or a type key: `grep` finds them only in `mod.json` (§3.1) |
| The undeclarables are named | each round publishes what the titles could NOT declare — that list is the engine backlog |

## State

**`mods/f16/` exists and the engine is asset-free.** Everything FlightBox flew out of `sim/` is one
scenario: 127 model files (`src/aircraft/`, incl. `MODEL-DELTAS.md`), the mesh recipe (`src/models/`),
415 `.fbm` (`src/missions/`, 296 of them in the regression glob), 12 `.fbc` (`src/campaigns/`) and the
two baked blobs a mission names (`src/data/`). `sim/assets/` and `sim/missions/` are gone.

**The loader is the smallest one that carries.** `mods/f16/mod.json` names six roots plus three names
(`meshes`, `sandbox`, `default_mission`); three readers resolve them and no path exists twice:
`sim/src/missions/FBMod.h` (C++ — clients, harnesses), `sim/tools/fb_mod.py` (the tool tree) and the
`sed` calls in `sim/Makefile` (the WASM preload and the `web/` copy). `fb-gym --mission payerne-full` is
a mod-relative NAME; a `.fbm`/`.fbc` suffix still means a path. No registry and no capability
negotiation — `--mod DIR` is the only switch a native run needs.

**§3.1 holds in the browser.** `make wasm` preloads every `mods/*/mod.json` it finds under
`/fb/mods/<id>/` with the manifest's own relative directories, copies missions and campaigns to the same
relative paths under `web/mods/<id>/`, and writes `web/mods/index.txt` (also preloaded as
`/fb/mods/index.txt`). `FBAppWasm` loads that manifest TWICE — once against the preloaded root for
aircraft and meshes, once against `/mods/<id>` for the mission URL — and `web/fbmenu.js` opens on a title
screen whose cards, campaign directory and free-flight mission all come out of the manifest. Measured in
the browser build itself: `gpu mod id=f16 aircraft=/fb/mods/f16/src/aircraft mission=/mods/f16/src/missions/payerne-full.fbm`,
`render unit_model type=f16 lods=4 parts=22 trisTotal=173330`, and the run flies — pilot phase
`Preflight → Takeoff → Route`, 2 000 m AGL at t=102 s. An unknown id stops the boot with
`mod_load_failed reason="cannot open /fb/mods/<id>/mod.json"` instead of flying something unasked for.
Measured across the move: all 296 missions byte-identical, `payerne-full --threads 1/2/4` on
`6e24090b7e861aa7`, ten harnesses unchanged, `test-air` at 5 outside band, `verify-trees` at 20 orphans.

**A SECOND MOD IS IN THE BROWSER, and §1's `watched` reader exists.** `mods/f22` (eight sorties, its own
DEM, `depends f16`) loads, flies and is WATCHABLE at `?mod=f22&mission=<name>&view=chase`. Measured in
headless Chromium against real WebGPU, one screenshot per sortie: `gpu mod id=f22 … models=/fb/mods/f22/../f16/src/models`,
`render unit_model type=f16 lods=4 parts=22 trisTotal=173330`, terrain and units drawn in six of the
eight (the other two are moonless-night sorties, below). Two things had to be true and one was not:
`"meshes"` is NOT inherited through `depends` — a root is a place a borrower needs, this list is what
goes into THIS title's download — so f22 declares `"meshes": "f16"` and the borrowed files stay in the
lender's preload rather than being copied under the borrower's id.

**`verify-trees` has §3's one bit.** Two scopes, two rules, one exit code: the engine's three trees by
path congruence (unchanged, **20 orphans**) and each mod by DOC PLUS PROOF — `doc/`, `src/`, at least
one runnable `.fbm` under it, and no `test/`. Every orphan line names its scope (`engine` / `mod:<id>`)
and shows three state columns for the engine against two for a mod, so the rule is visible in the
output. **4 mod orphans** today, all one shape: `f22`, `comanche`, `armored-fist`, `delta-force` have no
`src/`. Path congruence is deliberately NOT applied inside a mod — §3's own directory picture is four
flat texts against five declaration roots, so `doc/campaign.md` has no `src/campaign/` to match and the
rule would report 78 holes in `mods/f16/src/aircraft/**` that nobody wants filled.

**The engine is asset-free but not TYPE-free, and that is now a number.** `make -C sim verify-types`
(`sim/tools/verify_types.py`, rc=1 on purpose until it reaches 0) counts every mention of a named
aircraft type under `sim/src/` and breaks it down by how expensive the mention is to remove: today
**1 127 mentions of 11 types in 114 of 339 files** — 48 `dir` (two module trees), 434 `symbol`,
7 `key`, 51 `text`, 6 `value` (a type's number in generic code, a curated table because no regex can
see a number that does not spell its type) and 581 `comment`. Ordnance and ground types are a separate
inventory and printed on their own line, not folded in. What is left is two costs: the two module trees
(`sim/src/modules/f16/` + `sim/src/modules/mig29/`, 48 files of engine C++ about two aeroplanes) and the
6 `value` sites, which are the only class with nowhere to move to.

**The cast moved out, and `core/` kept the shape.** `core/FBAircraft.h` was the densest type file in the
tree (18 airframes, 148 mentions); it now declares only WHAT a flown airframe has and names none. The
rows are `mods/f16/src/catalogue.fba`, a sixth manifest root (`"catalogue"`, a FILE and not a
directory — one declaration), read once at boot by `missions/FBCatalogueBoot.h` into
`core/FBAircraftCatalogue`, which the caller MOVES into `Modules::FBRegisterAirModules`. Three
consequences beyond the count:

* **A module cannot enumerate the cast.** There is no global catalogue and no find-by-name any more; a
  factory hands its `FBAirModule` ONE row, so no catalogue aircraft can learn what else exists —
  Prinzip 3 held inward, in the type system rather than in a comment.
* **The damage-zone macros are gone, not moved.** A layout was `FB_AIR_ZONES(Mig21, 14.7)` — keyed by
  type name — and is now DERIVED from the row's declared span and length by `FBAircraftCatalogue::Add`.
  The fragility ladder and the four zones' system content stay in the engine because they are shared by
  every row: a ladder per row would express a difference nobody measured. `FBSystemHealth` is untouched
  and keeps its single friend — a layout is inert data that no mutator ever sees.
* **What did NOT move, and why:** the ladder's provenance comment still names
  `modules/f16/FBF16Damage.cpp` and `modules/mig29/FBMig29Damage.cpp` (4 of the file's mentions, counted
  as `comment`). Those numbers are a verbatim copy of the two flown modules' tables, and a copy whose
  source is unnamed is drift waiting to happen — CLAUDE.md's "jede Zahl trägt ihre Herkunft" outranks
  the counter.

**§2.1's premise is**: the per-unit declaration list exists as runtime data
(`sim/src/modules/FBCapability.h`, one table, twenty rows of `(accessor, C++ type, wire name)`), the
`FBModule` base demands none of it (one pure virtual left, `Run`), and `fb-gym --caps` emits it as
`<module> <capability> <c++ type>` — the artefact a tool schema is generated from. What is missing for
§2.1 is the other half: a call surface per capability (a verb list with parameters), not just the slot
inventory. See [`architecture.md`](architecture.md) §The layering pattern.

## Gaps

- **Four of the five mods are `doc/` only** — `f22`, `comanche`, `armored-fist`, `delta-force` carry no
  `src/`, so `mod.json` and the loader have exactly one subject today.
- **`mods/f16/src/` is not fully declarative yet.** `src/models/` holds python build recipes, and
  `modules/f16` + `modules/mig29` are engine `.cpp` — §4's „zero .cpp" holds for the mod directory and
  not yet for the scenario. They move when the capability list carries them declaratively.
- **The mission and model texts still name their old paths.** 296 `.fbm` headers and the model `.xml`
  banners quote `sim/missions/…` / `sim/assets/aircraft/…`; not one byte of either was touched, because
  a move that edits a flown model or a committed mission is no longer a move.
- **A mod's `doc/` is not itself checked for completeness.** `verify-trees` sees that `doc/` exists, not
  that §3's four texts are in it — `mods/f16/doc/` carries `campaign.md` + `missions.md` and owes
  `terrain.md`, `hud.md`, `sources.md`. A fixed file list is checkable; whether it should be fixed for
  every title is not decided.
- **`mod.json` is unchecked by the gate.** Four titles have none, so the loader could not see them even
  once their `src/` exists. It is identity, not triad, which is why `verify-trees` stays out of it.
- **ONE airframe mesh exists in the whole tree**, and it is why a watched campaign is half-empty:
  `mods/f16/src/models/` ships `f16` and nothing else, so of the 59 units the eight f22 sorties spawn,
  **22 are drawn and 37 are not** (per sortie: 2/4, 2/6, 3/8, 2/6, 4/8, 2/9, 3/8, 4/10). Everything the
  campaign fights — `mig23`, `mig29`, `f15c`, `kc135`, `e3`, `sa2`, `sa18`, `p18`, `zsu23`, `zu23`,
  `target_soft/hard` — publishes a type the renderer has no model for and is invisible, missiles
  excepted (their smoke trail is the only evidence a hostile exists). This is the mesh half of the gap
  below: there the registry lacks the type, here the type lacks the mesh.
- **Two of the eight watched sorties are a black frame, and it is astronomy, not a defect.** `c01m07`
  (1996-03-23T19:30Z) and `c01m08` (1996-03-24T20:00Z) fly at 02:30/03:00 local: sun 53.1°/46.5° below
  the horizon, moon 48.5°/44.8° below it at phase 0.22/0.31 (`core/FBEphemeris.h`, measured). Nothing in
  the scene emits light except missile plumes, and no night sortie in this campaign is watchable until
  aircraft carry lights or an afterburner is in frame.
- **A mesh key is a MODULE key, and modules are engine C++.** §3.1 lets a second mod declare its own
  meshes without touching code — but only for a type the engine's registry already builds. A
  `comanche.asset.json` draws nothing until `FBModuleRegistry` has a `comanche`, which is the same hole
  §2 already names one level down.
- **The browser carries EVERY mod's aircraft and meshes in one `gpu.data`.** With one mod that is 13.5 MB
  and correct; with four it is one download for four titles, and nothing streams a mod on demand. The
  fetched half (missions, campaigns) already does, so the split exists — the preloaded half does not use
  it.
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
