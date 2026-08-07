# Vision — what Outshine is for

The direction, set by the project owner. This file changes by **decision**, not by building; every
other file in this collection is measured against it. What is being built *right now*, and in what
order, is [`goal.md`](goal.md) — that file outranks this one on anything current, and the owner's own
comments outrank both.

## The one sentence

> **A game engine that runs at 720p60 on PS4- and A18-Pro-class hardware, with the technology of Days
> Gone and Horizon Forbidden West, in which games like Witcher 3, Fallout 4 and GTA 5 can be reproduced
> — optically, in content and functionally — purely through declarative `mods/`. The basis of the fully
> procedural world is OSM, elevation, weather and star data from the tile server. Through LLM
> integration every entity is intelligent and the game world is dynamic.**

It is not a claim. It is **four build decisions**, and each one closes a question:

| Part | What follows | Where it is worked out |
|---|---|---|
| **the world is loaded, not modelled** | terrain, land cover, buildings, vegetation, weather and the night sky come from `fb-tiles`. Every point on Earth is a valid start. No level editor, no authored map, no shipped world | [`world/terrain.md`](world/terrain.md), [`world/weather.md`](world/weather.md), [`render/classification.md`](render/classification.md) |
| **ONE physics system** carries walking, driving, flying and swimming | rigid bodies plus contacts plus one propulsion model per class; aerodynamics as coefficients, not as a table work | [`body-format.md`](body-format.md) |
| **an epoch and decay regulator** dresses the same geometry | a building is a house or a ruin, a road is asphalt or broken-up — same OSM dataset, one selection | [`goal.md`](goal.md) |
| **the actors think** instead of running state machines | an entity's brain is a regulation or an LLM, and it is the reason the perception boundary below stops being hygiene and becomes load-bearing | [`mods.md`](mods.md) |

## The hardware IS the goal, not a constraint

**PS4-class and A18 Pro, 720p60 — 16.67 ms at 1280×720.** Every design decision hangs off that line.
Whatever holds 60 holds 30 with headroom, and the headroom is the point.

The phone is the *budget*, not a fallback: A18 Pro has **~60 GB/s** of memory bandwidth against PS4's
**176 GB/s**, so bandwidth — not compute — is the weak axis, and there is no pleasant surprise waiting
at delivery ([`render/visual-target.md`](render/visual-target.md) §1).

## Two references, and they answer different questions

| Reference | Answers | What is taken from it |
|---|---|---|
| **Days Gone, Horizon Forbidden West** | *what can this hardware carry?* | **the technique** — full field of view, **GPU-driven placement**, **lighting decoupled from geometry**. Not their look. They are the existence proof that the budget above is reachable, and Outshine does it from tile data alone |
| **Witcher 3, Fallout 4, GTA 5** | *what should it look like and play like?* | **the optics and the content.** They reach their impact with **less** detail than a photograph, carried by light and silhouette. GTA 5 additionally names the physics *construction*: a vehicle is a hull on wheels with suspension, tyre grip and a torque curve; a human is a capsule whose locomotion the animation leads |

The technique ceiling is 2015-hardware-class rendering. What that includes and excludes concretely is
[`render/visual-target.md`](render/visual-target.md) §2.1.

## „Optically, in content AND functionally" — the sharpest word is the last one

A mod does not reproduce a *look*. It reproduces the **game**: what is in it, and how it behaves.

| Layer | Reproduced by | The test |
|---|---|---|
| optically | the epoch/decay selection over generated geometry, plus a title's own entity shaders | two mods at the same place differ only in the dial |
| in content | declared actors, entities, usable objects, and the scene | the world underneath is identical; only the cast changes |
| **functionally** | declared bodies, declared brains, declared goals | **a title ships no `.cpp`.** Mechanics are a declaration, not a plug-in |

**The third row is where an engine normally gives up and offers a scripting language.** Outshine does
not: the escape hatch is **LLM function calling over a declared capability surface**, not a bespoke
interpreter ([`mods.md`](mods.md)). A shader for the appearance of a title's own entities is allowed —
appearance is not knowledge.

**And the undeclarables list is therefore the engine's backlog.** Anything a title needs and cannot
declare is a hole in the engine, not a shortcoming of the title. That list is worth more than the
content that produced it.

## Four data kinds from the tile server, and nothing else

| Kind | What it makes possible |
|---|---|
| **OSM** | roads, rails, buildings, water, land use — the vector truth the classification chain arbitrates with |
| **elevation** | the DEM the mesh and every ground query stand on |
| **weather** | wind, cloud cover per étage, cloud base, visibility — atmosphere as consequence, not decoration |
| **stars** | the real night sky at true altitude and azimuth, from one ephemeris |

Nothing is preloaded and the engine ships no world. A title that wants a place picks **coordinates**,
not a map file.

## LLM integration — and the boundary that keeps it honest

Every entity is intelligent, and the world is dynamic because of it. That is a capability statement.
The architectural statement is the one under it, and it is the most important sentence in this file:

> **The old rule *„Outshine knows everything, a mod knows only what it knows"* was anti-cheat hygiene
> for scripted opponents. With thinking actors it becomes the load-bearing boundary of the whole
> architecture.**

Concretely, and each half is checkable rather than promised:

| Rule | Why it must hold for a brain specifically |
|---|---|
| **A brain sees only what its sensors deliver** | an LLM handed world state is omniscient by construction, and an omniscient opponent is not a hard opponent — it is a broken one. There is no prompt that un-tells a model something |
| **A brain acts only through simulated systems** | otherwise a model that can name an outcome can cause it, and the simulation stops being the arbiter |
| **A contact carries no identity** | a visual contact does not even carry a distance — only a TYPE, once the angular size gives it away. Identification is a channel, not a lookup |
| **Whatever builds a prompt may not read the entity registry** | this is the concrete hole to guard. A Game Master that assembles context from ground truth leaks perfect knowledge through a string, and no amount of instruction fixes it |

The payoff is not fairness, it is **readability**: an actor that acts on the information its kind
actually has makes mistakes a player can read as mistakes.

**Nothing enforces this today.** The judges, the monotone health register and the friend-locked tick
surface went with the simulation layer on 2026-08-07, and `verify-guards` — the gate that proved them by
trying to break them — went too. The one boundary still standing is the layer gate's `RESTRICTED` table
over `units/UnitRegistry.h`. Whatever spawns and steps a body under [`body-format.md`](body-format.md)
has to re-earn those shapes before the first brain is connected, not after.

## Epoch and decay — discrete, and a selection

**Three epochs × three decay steps. A selection, not a blend.** That retires the interpolation question
instead of deferring it, and it forces every step to be defensible on its own, which is checkable.

The dial reaches materials, vegetation density, building state and road surface. It **may not** reach
geometry or identity — the same dataset has to stay the same dataset, or the claim that two mods differ
only in the dial is untestable.

**Not built.** `epoch` and `decay` appear nowhere in the code; the two indices are to be threaded
wherever a material sits and read nowhere yet.

## The acceptance: one place, three epochs

**Hameln / Emmerthal / Grohnde on the Weser.** Not an example — the test of the central claim. If each
mod had its own location, every difference could be blamed on the location instead of the declaration.

| Mod | Epoch | The real anchor, all on the same river |
|---|---|---|
| 1 | pre-industrial (Witcher 3) | Hameln's Weser-Renaissance old town on its medieval street plan |
| 2 | present (GTA 5) | the same town, OSM raw, no filter |
| 3 | decay (Fallout 4) | **Grohnde nuclear power station**, 8 km upstream, real and shut down |

**Measured 2026-08-06 against the running `fb-tiles`:** terrain **65.26 … 234.70 m** ASL across ~15 km
(Weser floodplain up to the Süntel) — real relief, not a flat field. The Hameln z14 vector tile is
**152 998 bytes** and carries street names, addresses and POIs.

**Mod 2 comes first,** because epochs 1 and 3 are *transformations of the raw state* and you cannot
filter what you cannot yet show. The measuring bench that stands in for it while the engine is built is
`mods/demo/scene.json` ([`goal.md`](goal.md)).

## What „cut for a machine" actually means

Scenario and genre in, playable game out. Human-facing engines optimise for viewports, drag-and-drop,
visual scripting, asset marketplaces and the speed of a human's eyes and hands. None of that helps here.

| | Human-facing engine | Outshine |
|---|---|---|
| **Authoring** | GUI editors, DCC round-trip | **everything declarative and textual** — a body, a scene, a goal is a file that can be written, diffed and **generated**. The language is JSON |
| **Correctness** | playtesting, an art director's eye | **machine-checkable gates** — `verify-layers`, `verify-trees`, `verify-types` — plus a rendered frame or a number for every claim |
| **Numbers** | a designer's feel, tuned in an inspector | **provenance on every number** — derived (with the formula), measured (with the measurement), or `[SET]`. A machine cannot „just know" a dimension; it must be able to re-derive it |
| **Quality** | taste | **critics with declared bands** — `botanist`, `architect`, `art-director`, `sim-critic`, each judging a subject rendered alone before it enters the scene |
| **Iteration** | hot reload for a human | **determinism**, so a change's effect is attributable at all |
| **Assets** | modelled by hand | **procedural first, Blender only where procedure does not reach** ([`render/visual-target.md`](render/visual-target.md) §2.2) |
| **Vocabulary** | rich frameworks, deep inheritance | **small declared catalogues.** A closed enum that must be edited in seven files is hostile to generation; a declaration a module fills is not |

The last row is the one this tree paid for: every place where „add a creature" turned into an editing
chore was a closed enum. **The engine is cut for a machine precisely where a machine can generate into
it without editing seven files.**

## Believability, not fidelity

> Owner, 2026-08-05: *„in einem Game Engine ist immer **alles falsch** — die Frage ist, ist es noch
> **glaubhaft**."*

Judged on **three separate axes** ([`body-format.md`](body-format.md) §0.1):

| Axis | What must be credible | Judged by |
|---|---|---|
| **motion** | it moves the way that thing moves | list A where someone would notice; „it does not look wrong" everywhere else |
| **decision** | it acts the way that thing acts | the brain, and the boundary above is what makes its judgement readable |
| **representation** | it looks like that thing | the critics, against a subject rendered alone |

And the physics bar is stated rather than implied:

> **Physics must suffice for graphical representation.**

That is the whole requirement outside list A. Not fidelity, not an error band against some other solver
— *enough that the picture is credible.* **Writing list A, not writing the solver, is the gating work.**

## Fidelity is staggered, and that is a decision

| Subject | Scale | Why |
|---|---|---|
| What the player is inside — the body he controls and what he can touch | **believable on all three axes** | this is where the game happens |
| The world's geometry — terrain, roads, buildings, vegetation | **as loaded** | it is data, not a model; the engine's job is to show it, not to invent it |
| Distant and unobserved actors | **silhouette and motion only, and a brain only where it is looked at** | thinking is the expensive class. What must never depend on the observer is **knowledge** — resolution may, knowledge may not |
| Weather, clouds, night | **atmosphere and consequence** | they change what can be seen and what can see |

The consequence, stated plainly so nobody mistakes it for neglect: a body that fails a specialist's
side-by-side comparison is not a defect; a body that fails list A is.

## The staging

> Owner: *„Wir steigern uns. Dieses Jahr A, nächstes AA und in zwei Jahren AAA."*

| Tier | The line it must cross |
|---|---|
| **A** (2026) | one place, walkable, at **720p60** — terrain, buildings, trees and perennials generated from tile data alone, judged by the critics against the reference. [`goal.md`](goal.md) is the whole of tier A |
| **AA** (2027) | **breadth and persistence** — several genres from the same engine, entities with brains and inner state, a world that survives being left and re-entered |
| **AAA** (2028) | **the generator carries it** — scenario in, game out, with the quality gates passing without a human in the loop |

Tier A is the only one with a written plan. AA and AAA are named so the direction is not lost, not
scheduled.

## What Outshine is not

- **Not a study sim.** What is modelled must be *usable*; what is not modelled is stated rather than
  faked.
- **Not a world it ships.** The world is loaded from OSM, DEM, weather and star data.
- **Not a place where numbers are invented.** A number that cannot be derived, measured or honestly
  declared as `[SET]` does not go in ([`conventions.md`](conventions.md)).
- **Not an engine with a bespoke file format per feature.** Declarations are JSON — schema-checkable,
  diffable, generatable.
- **Not an engine with a scripting language.** Function calling over a declared capability surface, or
  nothing.
