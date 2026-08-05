# Vision — what OUTSHINE is for

The direction, set by the project owner. This file changes by **decision**, not by building; every
other file in this collection is measured against it.

## The one sentence, since 2026-08-05

> Owner: *„aus FlightBox wird **Outshine** als Game Engine mit **Ocean** als Weltzustandsgenerator.
> Darauf optimiert, damit **du** daraus Spiele und Spielwelten generieren kannst, die der Benutzer dann
> spielen kann. Alle bisherigen Game Engines sind auf menschliche Entwickler und Artists zugeschnitten.
> Outshine und Ocean schneidest du komplett auf dich zu. Ich will dir ein Szenario und Genre geben und
> du musst damit ein AAA-Spiel bauen können."*

**Outshine is a game engine cut for a machine to build games with; Ocean generates the worlds it plays
in.** The input is a scenario and a genre. The output is a game a person plays.

FlightBox is not abandoned — it becomes **the first title and the reference bench**: the one place where
fidelity is measured hard enough that the engine underneath can be trusted with anything else.

### What „cut for a machine" actually means

This is not a slogan; the difference is concrete, and this tree already discovered most of it the hard
way. Human-facing engines optimise for viewports, drag-and-drop, visual scripting, asset marketplaces
and the speed of a human's eyes and hands. None of that helps here. What helps:

| | Human-facing engine | Outshine |
|---|---|---|
| **Authoring** | GUI editors, DCC round-trip | **everything declarative and textual** — a body, a scene, a mission, a goal is a file that can be written, diffed and generated (`.fbm`, `.fbc`, glTF from a build script) |
| **Correctness** | playtesting, an art director's eye | **machine-checkable gates** — `verify-layers`, `verify-guards`, `verify-models`, ten harnesses, byte-exact regression over 296 missions |
| **Numbers** | a designer's feel, tuned in an inspector | **provenance on every number** — derived, measured or `[SET]`. A machine cannot „just know" why the span is 9.144 m; it must be able to re-derive it |
| **Quality** | taste | **oracles with declared bands** — believability as a written list of what a knowledgeable person checks, each with a number ([`body-format.md`](body-format.md) §4) |
| **Iteration** | hot reload for a human | **determinism**, so a change's effect is attributable at all (CLAUDE.md principle 4) |
| **Assets** | modelled by hand | **generated parametrically from sourced dimensions**, with an adversarial critic that measures the result against blueprints and photographs |
| **Review** | code review, art review | **an adversary per producer** — the modeller/critic pair, and the same shape for every other producing role |
| **Vocabulary** | rich frameworks, deep inheritance | **small declared catalogues.** A closed enum that must be edited in seven files is hostile to generation; a declaration a module fills is not |

The last row is the one this tree paid for today: `FBModule`'s twenty mandatory slots, `FBSystemId`'s
closed fourteen and `FBObjectiveKind`'s closed ten are exactly the places where „add a creature" turns
into an editing chore. **The engine is cut for a machine precisely where a machine can generate into it
without editing seven files.**

### The staging, and what separates the tiers

> Owner: *„Wir steigern uns. Dieses Jahr A, nächstes AA und in zwei Jahren AAA. Das ist also ein
> langfristiges Projekt."*

The tiers are not adjectives. Each needs a measurable line, and this file is where they get one:

| Tier | The line it must cross |
|---|---|
| **A** (2026) | A scenario and genre produce a **playable, coherent world**: entities that see each other, act on goals, take damage, and a run that concludes. Believability list A holds for the bodies that exist. One genre. |
| **AA** (2027) | **Breadth and persistence** — several genres from the same engine, a world that survives being left and re-entered (the observation collapse of [`persistent-world.md`](persistent-world.md) §4a), actors with inner state, and assets at a fidelity a player does not question. |
| **AAA** (2028) | **The generator carries it** — scenario in, game out, with the quality gates passing without a human in the loop; the engine's own critics catch what a studio's QA would. |

### What does not change

Everything in the *how*. Spec first. No cheating — an actor sees only through its sensors and acts only
through simulated systems. Every number with an origin. Rejected approaches stay with their
measurements. Every round updates State, Gaps and journal. These are not FlightBox habits that Outshine
inherits; they are **the reason a machine can be trusted to build with it at all.**

---

## Superseded — the FlightBox vision (until 2026-08-05, kept as record)

**A tactical air-combat game on real physics** — as correct as necessary, as accessible as possible.

The physics is JSBSim; FlightBox is the world around it — global terrain, renderer, HUD, controls. The
aircraft is the F-16, flown as the pinned vanilla JSBSim model. Two quality axes count: **correct
rendering** and **realistic F-16 flight behaviour**.

## The staggered scale

Not everything is modelled to the same depth, and that is a decision, not a shortfall:

| Subject | Scale | Why |
|---|---|---|
| The F-16 | **exact** | it is the product |
| Sensors, weapon envelopes, the course of an engagement | **believable** | this is where the game happens |
| Enemy aircraft | **hit their envelope, nothing more** | you usually do not see your opponent |
| Turn-fight fidelity | **subordinate** | see above — the weapon systems matter more than the aircraft carrying them |
| Weather, clouds, night | **tactical layers, not decoration** | they change what you can see and what can see you |

The consequence is stated plainly so nobody mistakes it for neglect: a MiG-29 that fails a knife-fight
comparison is not a defect; a MiG-29 with the wrong envelope is.

## Two classes of mission

| Class | What it is | Rule |
|---|---|---|
| `sim/missions/*.fbm` | **measuring rigs for the gym** — the control loop works with these | they stay exactly as strict as they are: declared spawn, declared objectives, machine-readable verdict |
| Missions for humans | scenarios, later and looser | a scenario layer **over** the `.fbm` format (roadmap R9), not a second dialect |

The owner put the split in one sentence, and it decides more than it looks like:

> **The gym** — for training and simulating — has scenarios with **thousands of actors**, in which
> complex engagements are computed and systems are optimised, improved, tested, and the AI made better.
> **The game is simplified missions with entertainment value.**

Two consequences follow, and both are load-bearing.

**FlightBox computes; the game is a VIEW of what was computed.** The playable part does not replace the
measurement and does not run a second truth: same simulation, same two judges, same reading rules
underneath. Simplification is therefore **one-directional** — the player layer may read and leave out,
never add what the run did not contain and never change anything below it. The tree already has that
shape once: the campaign carry may delete a line and never add one, secured by a postcondition rather
than by intent ([`missions/campaign.md`](missions/campaign.md)).

**Scale is a gym question the game does not inherit.** Thousands of actors is a target for the
simulation side; a playable mission is small, and that is a property rather than a shortcoming. A rig
that flies 400 seconds without anyone being hit can be valuable as a measurement and useless as a game
mission, with neither of the two being wrong.

## Anti-cheat is a game decision

The anti-cheat structure — the pilot sees only through simulated sensors, writes only through
simulated controls, cannot repair itself, is judged by two incorruptible outside judges — is load-
bearing for the *game*, not just for the engineering:

**a cheating opponent is noticed immediately.** An enemy that always knows where you are is not a hard
opponent, it is a broken one. That is why the boundaries are drawn structurally (include graph, private
mutators with a single friend, anonymous contacts, two-valued IFF) rather than by convention — see
[`sim/sensors.md`](sensors.md) and [`sim/core.md`](core.md).

The same reasoning applies to the pilot AI: it flies with the information a pilot has (radar picture
with age, RWR without range, a datum instead of the truth about a lost contact), which is what makes
its mistakes readable as mistakes.

## What follows from the physics choice

| Principle | Consequence |
|---|---|
| Do not rewrite physics — JSBSim is the truth | own code only at the seams: FDM adapter, control, renderer |
| JSBSim runs **in** the client | no wire protocol between physics and picture; they are the same process |
| The pinned model is the reference, not the real jet | a validated model property (e.g. ~190 °/s roll rate) is accepted, not a defect |
| The sim runs as fast as sensible | the maths is deterministic; if wall-clock speed changes the result, the coupling is a bug |
| Nothing is preloaded | every tile on demand → **every point on Earth is a valid start** |

## What FlightBox is not

- Not a study-sim of every switch in the cockpit: what is modelled must be *usable*, and what is not
  modelled is stated rather than faked.
- Not a physics rewrite: a number that cannot be derived, measured or honestly declared as `[SET]`
  does not get invented (`conventions.md`).
- Not a fleet of aircraft: other modules exist to prove the plug-in mechanism and to give the F-16
  something to fight. The F-16 is the product.
