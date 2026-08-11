# Vision — what Outshine is for

The direction, set by the project owner. This file changes by **decision**, not by building; every
other file in this collection is measured against it. **The owner's own comments outrank it.**

## The one sentence

> **A game engine that runs at 720p60 within an A18-Pro-class resource budget, with the technology of Days
> Gone and Horizon Forbidden West, at the picture quality of Kingdom Come: Deliverance — reproducible
> — optically, in content and functionally — purely through declarative `scenarios/`. The basis of the fully
> procedural world is OSM, elevation, weather and star data from the tile server. Through LLM
> integration every entity is intelligent and the game world is dynamic.**

It is not a claim. It is **four build decisions**, and each one closes a question:

| Part | What follows |
|---|---|
| **the world is loaded, not modelled** | terrain, land cover, buildings, vegetation, weather and the night sky come from `fb-tiles`. Every point on Earth is a valid start. No level editor, no authored map, no shipped world |
| **ONE physics system** carries walking, driving, flying and swimming | rigid bodies plus contacts plus one propulsion model per class; aerodynamics as coefficients, not as a table lookup |
| **an epoch and decay regulator** dresses the same geometry | a building is a house or a ruin, a road is asphalt or broken-up — same OSM dataset, one selection |
| **the actors think** instead of running state machines | an entity's brain is a regulation or an LLM, and it is the reason the perception boundary below stops being hygiene and becomes load-bearing |

**Two words carry the split that makes this buildable.** A **scenario** is a declared world — a place, a
clock, a weather, what runs. It brings no `.cpp`. A **generator** is engine code that turns what the core
knows into content: vegetation, buildings, infrastructure, water. Generators are exchangeable by
construction, because they all read the same naked world. The target is **wasm on Chromium and Edge**;
A18-Pro-class hardware is the resource budget, not a delivery target.

## The bar: a world sandbox at Unreal level, out of tile data alone

**The measuring stick for everything visible.** Not "good for a procedural engine", not "good for one
developer" — the picture is held against the impression a commercially built world sandbox leaves, and
the epoch dial does not buy an exemption.

What the bar is **not**: Unreal's feature set, its editor, its material graph, or any particular one of
its techniques. Outshine is [cut for a machine](#what-cut-for-a-machine-actually-means) and ships no
authoring surface at all. **The bar is on what the camera sees, and only there** — which is the same
reading as the reference titles below, who reach their impact with less detail than a photograph.

Why that is reachable from tile-server data alone rests on three legs, and each can be attacked
separately:

| Leg | The claim | Why it holds |
|---|---|---|
| **the geometry already exists** | an open world's dominant cost is **authoring**, not rendering. A studio pays artists to build a street grid, a building footprint set and a land-cover map that OSM and the DEM already contain — for the whole planet, not for one shipped square | the density actually delivered at the acceptance location is measured under [The acceptance](#the-acceptance-one-place-three-epochs): named streets, addresses, POIs and real relief, in one z14 tile |
| **the rendering technique is published** | full field of view, GPU-driven placement and lighting decoupled from geometry are **solved and documented**, and Days Gone / Horizon Forbidden West are the existence proof that they fit the budget | the remaining work is integration, not research — which is exactly why a stage names its source in one line |
| **there is no asset budget to lose** | the usual reason a sandbox needs a studio is the texture and mesh pipeline. Outshine has none: appearance is a **function**, and the only rasters allowed are a cache of a computable function or measured data (`CLAUDE.md` principle 2) | the side effect is the larger one — mip dependence, zoom pops, sampling grids and filter artefacts **cannot occur in a function** |

What the legs do **not** cover, stated so the bar is not mistaken for a proof: authored art still buys
**intent** — a silhouette placed by a human because it reads well. Outshine has to earn that from data
and rules instead, and the place where that is decided is the architect's judgement against the
reference photograph.

**The bar and the budget can collide, and the collision is not resolved by wishing.** The budget below
is hard (16.67 ms), the technique ceiling is 2015-hardware-class, and a bar that turns out to be
arithmetically unreachable inside them is **disproved, not voted away**: the disproof is reported with
its measurement, and what happens next is a decision by the owner, not by the round that hit the wall.

## The hardware IS the goal, not a constraint

**A18-Pro class, 720p60 — 16.67 ms at 1280×720.** Every design decision hangs off that line.
Whatever holds 60 holds 30 with headroom, and the headroom is the point.

The phone is the *budget*, not a fallback: A18 Pro has **~60 GB/s** of memory bandwidth against PS4's
**176 GB/s**, so bandwidth — not compute — is the weak axis, and there is no pleasant surprise waiting
at delivery.

**Against the picture reference the trade is exact.** KCD asks 1600×900 at 30 = 43.2 Mpix/s; we ask
1280×720 at 60 = **55.3 Mpix/s, 1.28×**. Compute per pixel comes out at **0.98 — parity**; bandwidth per
pixel at **0.27**. We are asking for their per-pixel arithmetic on **roughly a quarter of their per-pixel
bytes**, which is the same direction the texture-free rule already points: **analytic work is cheap for
us, traffic is not.** The 0.27 is an upper bound on the deficit rather than a measurement — an A18 Pro is
a tile-based deferred renderer with tile memory and a large system cache built to hide exactly that, and
PS4's 176 GB/s is shared with its CPU.

## Two references, and they answer different questions

| Reference | Answers | What is taken from it |
|---|---|---|
| **Days Gone, Horizon Forbidden West** | *what can this hardware carry?* | **the technique** — full field of view, **GPU-driven placement**, **lighting decoupled from geometry**. Not their look. They are the existence proof that the budget above is reachable, and Outshine does it from tile data alone |
| **Kingdom Come: Deliverance** | *what should the **nature** look like?* | **the picture target for terrain, vegetation and light.** Two corrections to why, both found by checking: it is **900p at a missed 30** on a base PS4, not 1080p30 — and its landscape is **not** data-derived but a hand-composed cut-and-paste of three real areas around Sázava, terrain painted in Sandbox, vegetation painted per sector. **The placement is authored; only the representation is copyable.** What survives, and it is enough: its vegetation is the same **temperate central-European** stack as our acceptance places, and its representation is procedural-friendly all the way down — trees generated in GrowFX and baked, grass as merged instances, distance as one runtime-merged billboard, terrain colour split by frequency with the high half explicitly greyscale. Its *built* world does not transfer: a Bohemian village is not modern infrastructure |
| **GTA 5** | *what should the **built world** be, and what can one **do** in it?* | infrastructure and buildings — and the verbs: **walk, drive, fly**. With them the physics **construction**: a vehicle is a hull on wheels with suspension, tyre grip and a torque curve; a human is a capsule whose locomotion the animation leads |

The technique ceiling is 2015-hardware-class rendering — what shipped in those titles, integrated
rather than researched.

## Proven first — an invention needs a reason

**Be inventive, but build on what works.** Experimenting is wanted; reinventing the wheel is not. Where
a problem has been solved several times over, **the established way is the starting point**, and a
deviation from it carries its reason where the deviation is.

| Field | Whose solution is the starting point | What is taken from it |
|---|---|---|
| frame structure, placement, lighting | **AAA open-world titles**, Days Gone and Horizon Forbidden West first | the technique, itemised in the table above — and nothing beyond it |
| vegetation LOD, aggregate and cut-off | **SpeedTree**, and the shipped foliage switches of current engines | where the transitions sit and what they are allowed to cost |
| tags, geometry cleanup, classification | **OSM reference implementations** | the tag semantics and the arbitration order — the meaning of a tag is not ours to redefine |
| a whole globe from vector plus elevation | **Microsoft Flight Simulator** | that streamed vector + DEM data carries a playable world at planetary scale, and that OSM footprints raised by rules are the answer wherever nothing is modelled — which, for an unbounded area, is everywhere |

The rule has a mechanism, not just an attitude: a stage that implements a published technique **names it
in one line, with its deviation**. A successor then reads whether
the stage does the standard thing or whether somebody improvised.

**And the converse is stated, so "proven" does not become an excuse.** Three things here have no prior
art with our constraints, and they are inventions by necessity rather than by taste:

| Invented here | Why nothing can be copied |
|---|---|
| the **epoch/decay dial** over one OSM dataset | engines dress *authored* geometry; nobody dresses a dataset that must stay the same dataset |
| **thinking actors behind a sensor boundary** | scripted AI has no need for the boundary, so no shipped engine has had to hold it |
| **one body format** for furniture, human, wolf, tank, aircraft | each field has its own solver and its own file format; the union is the point |

Everything else is integration. **"Novel" is not a value here; measured is.**

## „Optically, in content AND functionally" — the sharpest word is the last one

A mod does not reproduce a *look*. It reproduces the **game**: what is in it, and how it behaves.

| Layer | Reproduced by | The test |
|---|---|---|
| optically | the epoch/decay selection over generated geometry, plus a title's own entity shaders | two scenarios at the same place differ only in the dial |
| in content | declared actors, entities, usable objects, and the scene | the world underneath is identical; only the cast changes |
| **functionally** | declared bodies, declared brains, declared goals | **a title ships no `.cpp`.** Mechanics are a declaration, not a plug-in |

**The third row is where an engine normally gives up and offers a scripting language.** Outshine does
not: the escape hatch is **LLM function calling over a declared capability surface**, not a bespoke
interpreter. A shader for the appearance of a title's own entities is allowed —
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

> **The old rule *„Outshine knows everything, a scenario knows only what it knows"* was anti-cheat hygiene
> for scripted opponents. With thinking actors it becomes the load-bearing boundary of the whole
> architecture.**

Concretely, and each half is checkable rather than promised:

| Rule | Why it must hold for a brain specifically |
|---|---|
| **A brain sees only what its sensors deliver** | an LLM handed world state is omniscient by construction, and an omniscient opponent is not a hard opponent — it is a broken one. There is no prompt that un-tells a scenarioel something |
| **A brain acts only through simulated systems** | otherwise a scenarioel that can name an outcome can cause it, and the simulation stops being the arbiter |
| **A contact carries no identity** | a visual contact does not even carry a distance — only a TYPE, once the angular size gives it away. Identification is a channel, not a lookup |
| **Whatever builds a prompt may not read the entity registry** | this is the concrete hole to guard. A Game Master that assembles context from ground truth leaks perfect knowledge through a string, and no amount of instruction fixes it |

The payoff is not fairness, it is **readability**: an actor that acts on the information its kind
actually has makes mistakes a player can read as mistakes.

**A unit never touches the world.** It is driven by its brain or by the player, both of which reach only
its own systems; the systems reach only force sources; and the world changes only through physics. There
is no shorter path, and the same chain carries a wolf, a car and a player — which is why the physics is
one system.

**C++ guarantees this, not a checker.** A rule a tool counts is a rule that can be broken and then
reported; a rule the language enforces is one that does not compile. The shapes are the ordinary ones —
a brain that is handed a sensor view and has no name for the world, a system whose only mutating verb
takes a force, a contact type with no field to put an identity in, ownership that makes the wrong call
unspellable rather than merely wrong. **These stand before the first brain is connected, not after**, and
whatever spawns and steps a body has to earn them.

## The setting: post-scarcity, and it is why both references are needed

**A future after scarcity — modern infrastructure and lush nature**, and neither reference carries both.
KCD has the nature and a medieval village; GTA 5 has the infrastructure and a thin, ornamental green. The
setting takes one half from each, which is the whole reason there are two.

**"Lush" is load-bearing, not atmosphere.** It sets the vegetation density, the layering and the ground
cover as the target rather than a sparse plausible scatter — and it is exactly where the open list sits:
crowns that are not tree-shaped, a near crown that reads as flakes rather than one mass, and no
undergrowth at all. A post-scarcity world is one where nature was *let back in*, so the green is the
subject and not the backdrop.

## Epoch and decay — discrete, and a selection

**Three epochs × three decay steps. A selection, not a blend.** That retires the interpolation question
instead of deferring it, and it forces every step to be defensible on its own, which is checkable.

The dial reaches materials, vegetation density, building state and road surface. It **may not** reach
geometry or identity — the same dataset has to stay the same dataset, or the claim that two scenarios differ
only in the dial is untestable.

**Not built.** `epoch` and `decay` appear nowhere in the code; the two indices are to be threaded
wherever a material sits and read nowhere yet.

## The acceptance: one place, three epochs

**Hameln / Emmerthal / Grohnde on the Weser.** Not an example — the test of the central claim. If each
mod had its own location, every difference could be blamed on the location instead of the declaration.

| Mod | Epoch | The real anchor, all on the same river |
|---|---|---|
| 1 | pre-industrial | Hameln's Weser-Renaissance old town on its medieval street plan |
| 2 | present | the same town, OSM raw, no filter |
| 3 | decay | **Grohnde nuclear power station**, 8 km upstream, real and shut down |

**Measured 2026-08-06 against the running `fb-tiles`:** terrain **65.26 … 234.70 m** ASL across ~15 km
(Weser floodplain up to the Süntel) — real relief, not a flat field. The Hameln z14 vector tile is
**152 998 bytes** and carries street names, addresses and POIs.

**Mod 2 comes first,** because epochs 1 and 3 are *transformations of the raw state* and you cannot
filter what you cannot yet show. The measuring bench that stands in for it while the engine is built is
`mods/demo/scene.json`.

## What „cut for a machine" actually means

Scenario and genre in, playable game out. Human-facing engines optimise for viewports, drag-and-drop,
visual scripting, asset marketplaces and the speed of a human's eyes and hands. None of that helps here.

| | Human-facing engine | Outshine |
|---|---|---|
| **Authoring** | GUI editors, DCC round-trip | **everything declarative and textual** — a body, a scene, a goal is a file that can be written, diffed and **generated**. The language is JSON |
| **Correctness** | playtesting, an art director's eye | **the build itself** — a target that omits `render/` is the layering check, and a checker beside it would be a second truth — plus a rendered frame or a number for every claim |
| **Numbers** | a designer's feel, tuned in an inspector | **provenance on every number** — derived (with the formula), measured (with the measurement), or `[SET]`. A machine cannot „just know" a dimension; it must be able to re-derive it |
| **Quality** | taste | **critics with declared bands** — `botanist`, `architect`, `art-director`, `sim-critic`, each judging a subject rendered alone before it enters the scene |
| **Iteration** | hot reload for a human | **determinism**, so a change's effect is attributable at all |
| **Assets** | modelled by hand | **procedural first, Blender only where procedure does not reach** |
| **Vocabulary** | rich frameworks, deep inheritance | **small declared catalogues.** A closed enum that must be edited in seven files is hostile to generation; a declaration a scenarioule fills is not |

The last row is the one this tree paid for: every place where „add a creature" turned into an editing
chore was a closed enum. **The engine is cut for a machine precisely where a machine can generate into
it without editing seven files.**

## Believability, not fidelity

> Owner, 2026-08-05: *„in einem Game Engine ist immer **alles falsch** — die Frage ist, ist es noch
> **glaubhaft**."*

Judged on **three separate axes**:

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
| The world's geometry — terrain, roads, buildings, vegetation | **as loaded** | it is data, not a scenarioel; the engine's job is to show it, not to invent it |
| Distant and unobserved actors | **silhouette and motion only, and a brain only where it is looked at** | thinking is the expensive class. What must never depend on the observer is **knowledge** — resolution may, knowledge may not |
| Weather, clouds, night | **atmosphere and consequence** | they change what can be seen and what can see |

The consequence, stated plainly so nobody mistakes it for neglect: a body that fails a specialist's
side-by-side comparison is not a defect; a body that fails list A is.

## The persistent Outshine server

**A persistent world server is Outshine without the picture** — the same engine, the same world, the
same generators, compiled without `render/`. It is **not built**, and this section says only what has to
stay true so that it remains possible.

**What keeps it possible is a build target, not a rule someone maintains.** Core, world, generators and
physics compile without `-Isrc/render`; if they stop doing so, the server stops existing, and that is a
compile error rather than a note in a document. This is the strongest form the layering rule can take:
the boundary is not asserted, it is *delivered*.

From it follows what "the simulation must exist without a GPU" actually costs. Height at a point, class
at a point, a building footprint, a water level and the occupancy a body collides with all answer on the
CPU with no device present. **Appearance may never be a precondition for physics** — a body swims
because the core knows the water level, not because a water generator drew a surface. Today the counter-
example is exact: the forest returns nothing without a live device, so on a server there would be no
trunk to walk into.

It sharpens two principles rather than contradicting them. *Everything runs in the client* stays true of
the game: the client is complete and owns its simulation; the server is the same program with the
picture removed, not a hub that owns the world while a thin client watches. *Two lean containers* is a
statement about today and will need the owner's decision when the server arrives.

## The staging

> Owner: *„Wir steigern uns. Dieses Jahr A, nächstes AA und in zwei Jahren AAA."*

| Tier | The line it must cross |
|---|---|
| **A** (2026) | one place, walkable, at **720p60** — terrain, buildings, trees and perennials generated from tile data alone, judged by the critics against the reference.  is the whole of tier A |
| **AA** (2027) | **breadth and persistence** — several genres from the same engine, entities with brains and inner state, a world that survives being left and re-entered |
| **AAA** (2028) | **the generator carries it** — scenario in, game out, with the quality gates passing without a human in the loop |

Tier A is the only one with a written plan. AA and AAA are named so the direction is not lost, not
scheduled.

## The way is the goal

**This runs over months and has no acceptance date.** Nothing above is a delivery promise; the tiers are
a direction. What follows from that is the working posture, and it is not softness:

> **A round that learned something is a good round even if it delivered nothing — but only if what it
> learned stays put, with its measurement.**

| | What it means concretely |
|---|---|
| **learned means measured** | a round that ends with an impression ended with nothing. The unit of learning is a number and how it was obtained. An unmeasured round did not learn, it guessed |
| **where it stays** | in the round's report, and in the code if it changed. A measurement is **not** preserved as a document: the starting position moves constantly, and a conserved number misleads a later round rather than saving it one |
| **what it does not license** | a rushed completeness claim. "Done" is a claim about a measurement, and a partial round says which part is left |
| **the failure mode it prevents** | pressure to ship something visible per round, which buys a demo and pays for it in dead paths |

The corollary for a bar as high as the one above: it is reached by a series of measured steps, and the
step that **disproves** a plan is worth as much as the step that implements one.

## Nothing here is a possession

**No format, no directory, no algorithm and no document in this tree obliges anyone to anything.** Only
the goal is in focus. If an existing approach does not bring the picture closer to photography, it goes
— and the rule *"what is replaced disappears in the same round"* exists precisely for that moment.

**This is not a blank cheque, and the distinction is the whole point:**

| Revisable — every one of them a **decision** | Not revisable — the **tools** revision is done with |
|---|---|
| a file format, a directory layout, an algorithm, a stage, a document, an accepted result | the duty to **measure** rather than judge by eye |
| the classification chain, the body format, the epoch dial's shape | the **provenance of every number** — derived, measured or `[SET]` |
| every decision recorded in this file and in `architecture.md` | **deleting what is superseded, in the same round** |

The right-hand column is not sentiment: without it the left-hand column cannot be exercised. A tree that
keeps its dead paths cannot afford to revise anything, because nobody can tell which path fired. A tree
whose numbers have no origin cannot compare the successor to the predecessor at all.

**And a revision is a decision, made where decisions are made.** This file and `architecture.md` change by the
owner's call — not by the round that found the existing shape inconvenient. "Nothing is a possession"
removes the *obligation* to keep something, never the requirement to say what replaces it and to show
the measurement that made the swap worth it.

## What Outshine is not

- **Not a study sim.** What is modelled must be *usable*; what is not modelled is stated rather than
  faked.
- **Not a world it ships.** The world is loaded from OSM, DEM, weather and star data.
- **Not a place where numbers are invented.** A number that cannot be derived, measured or honestly
  declared as `[SET]` does not go in.
- **Not an engine with a bespoke file format per feature.** Declarations are JSON — schema-checkable,
  diffable, generatable.
- **Not an engine with a scripting language.** Function calling over a declared capability surface, or
  nothing.
