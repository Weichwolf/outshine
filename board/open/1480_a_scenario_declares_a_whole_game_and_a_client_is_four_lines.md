Type: feature
Area: scenario
Tags: scope

**A scenario declares a whole game, and a client is four lines**

The owner's bar: **a complete game is practically four lines of client code and one scenario** -- and a
scenario may be as small as a Tetris board written in HTML or as large as Fallout 4. The client says
where to render, loads the scenario, runs the loop and cleans up; **everything else the scenario
declares.**

```cpp
outshine::Engine engine;
engine.RenderTo(surface);
engine.Load("fallout.scenario");
engine.Run();
```

## What a game of that size has to declare, decomposed by the engine's own layers

*Nothing here is invented: each row is a layer `CLAUDE.md` already names, asked what a scenario must
hand it. **A row a scenario leaves out is a layer that does nothing** -- the generators are configured
by a scenario and used only if it asks for them.*

| layer | what the scenario declares |
|---|---|
| **providers** | which upstreams, each pinned, ranked, and what absence hands over to |
| **Ground** | the field's declared tables: class, ring, water, and the **epoch and decay dial** that dresses the same geometry |
| **generators** | which kinds are registered and each kind's parameters -- species tables, façade rules, road profiles. **May be none** |
| **compositors** | which are on -- terrain, forest, city, traffic, `declared` -- and each one's budget |
| **renderer** | the plan: outputs, content stages, transfer, exposure, precision |
| **content** | glTF assets and their placements for the `declared` compositor; materials and variants |
| **surfaces** | HTML, CSS and scripts: a HUD, a menu, a terminal, a Pip-Boy -- and a whole game where the game IS the surface |
| **actors** | kinds, each with the host capability surface it may perceive and act through, its programme or its model binding, its spawn rule and its tick rate |
| **physics** | one system, and the dial: walking, driving, flying, swimming |
| **clock** | epoch, time of day, rate, weather |
| **input** | bindings from a device to DECLARED ACTIONS, so a client handles named actions and never a keycode |
| **state** | what survives a save: an actor's named values, a quest's state, the world's decay |

## A scenario of that size is not one file, and both references agree on the shape

**Looked up rather than recalled.** RAGE registers content packs in an ordered `dlclist.xml`; each pack
carries `setup2.xml` and a `content.xml` naming what it mounts, with `contentChangeSetGroups` selecting
which of it is active. Bethesda's chain is an ordered list of plugins where a later record **overrides**
an earlier one of the same id.

**So: a scenario is a MANIFEST plus an ordered list of layers, and a later layer overrides an earlier
one by id.** That is what makes a scenario the size of a game authorable at all, what makes a mod a
first-class thing rather than a patch, and what lets one line of a scenario be swapped for a
measurement without rewriting it.

- [ ] **A scenario is a directory with a manifest**, and the manifest names an ordered list of layers
- [ ] **A later layer overrides an earlier one by id**, and what overrode what is publishable -- a
      declaration nobody can trace is a declaration nobody can debug
- [ ] **A change set selects which declarations are active**, so one scenario carries variants without
      a second copy of itself
- [x] **Every layer of the table above is optional and its absence is a named default**, never a
      silent one: a scenario that declares no generator gets no generated content and says so

## The class structure, because a struct of eight fields is a studio shot and not a game

**The first cut of the public `Scenario` carried frame, stands, variant, fps, fill, orbit, one key
light and an ambient colour.** That is the smallest scenario this engine can run and it is what proved
the four lines; **it cannot express a game and must not be mistaken for one.** What follows is the
tree, one class per row of the table above:

```
Scenario
  Identity      name · version · epoch · decay
  Layers[]      ORDERED; a later layer overrides an earlier one by id
  World         origin (lat/lon, or none for a studio) · bounds · weather
  Providers[]   kind · pin · rank · what absence hands over to
  Generators[]  kind · parameters (the kind's own, opaque to the engine)
  Compositors[] kind · budget · on
  Render        outputs · content stages · transfer · exposure · precision · frame
  Lighting      key · environment
  Assets[]      uri · digest · kind · variant
  Placements[]  asset · transform          (the `declared` compositor's)
  Surfaces[]    document · style · programme · rect · z

  Kinds[]       name · inherits · asset · programme · capabilities · attributes · tick rate
  Instances[]   of · id · in · transform · attribute overrides · what it HOLDS
  Regions[]     id · interior or exterior · origin · radius · streams · what it uses
  Doors[]       id · from · to · where
  Volumes[]     id · in · shape · extent · what it FIRES · when
  Sounds[]      id · uri · bus · positional · loops · gain · falloff
  Buses[]       id · into · gain
  Tables[]      id · columns · rows
  Events[]      name · what it carries
  Views[]       id · follows · offset · fov · TIME SCALE

  Physics       the dial: walking · driving · flying · swimming
  Clock         start · rate
  Input[]       a device event -> a NAMED ACTION
  State[]       what survives a save
```

### Where the six new rows came from, which was playing it rather than listing features

*The owner's exercise: a few hours of Fallout 4, written down as what HAPPENS and then decomposed.*

| what happens | what it needs | the row |
|---|---|---|
| wake in the vault, walk out the door | an interior that does not stream, an exterior that does, a transition between | **Regions · Doors** |
| Codsworth speaks, I choose a reply | a surface, a voice line, a programme on the thing that speaks | Surfaces + **Sounds** + Kinds |
| pick up a coffee cup, break it for components | a thing with mass and value, held by another thing | **Kinds · Instances** |
| a raider shoots, I lose health | a number per weapon, a value on an instance, an event | **Tables · Events** |
| a quest stage completes when I reach the porch | a shape that fires a named event | **Volumes · Events** |
| the radio plays, footsteps echo, rain | routed mixing and a positional source | **Sounds · Buses** |
| I aim, time slows | a second camera with its own rate | **Views** |
| I level up and pick a perk | attributes on an instance, read by a programme, against a table | Kinds · Instances · Tables |
| I build a wall and wire power | instances placed at run time, holding one another | Kinds · Instances |
| I save and come back | State |

**THE ENGINE SPELLS NO NOUN OF THAT LIST.** There is no `Quest`, no `Perk`, no `Weapon`, no `Faction`
and no `Dialogue` in this tree, and there must not be: those are content, and each one above is made of
**kinds, instances, attributes, volumes, events, tables and surfaces**. *An engine that spelled `Quest`
would have to be changed to ship a game without quests.*

**One mechanism carries the nouns, which is the shape both references take**: Bethesda's Creation Engine
is records-and-forms with a base record and instance overrides; Unreal is actors-and-components. Here a
`Kind` is the default and an `Instance` overrides it -- `mama-murphy` is a `settler` with `health` 60
where the kind says 100 -- and **what a thing HOLDS is the same relation as where it stands**, so an
inventory and a world placement are one mechanism rather than two.

**Every one of those is optional and its absence is a named default.** A scenario that declares no
`Generators` gets no generated content; one that declares no `World` is a studio; one whose whole
content is a `Surfaces` entry is a game made of a document, which is the Tetris end of the scale.

**The public surface is two headers and not one.** `<outshine/Outshine.h>` is the handle -- create,
load, run, destroy -- and `<outshine/Scenario.h>` is the declaration a client may build in code. *As
few as possible and as many as necessary*: a client that only loads a file needs the first.

## The format is XML, and the reason is that a scenario is CONTENT

**The owner's ruling.** RAGE declares its content in XML -- `dlclist.xml`, `setup2.xml`, `content.xml`
and the `.meta` files beside them -- and a scenario of this size is authored, layered and overridden
exactly the way those are.

**The corpus manifests stay JSON, and that is not two ways to do one thing.** A manifest is an
INSTRUMENT's declaration: what a case renders, at what bounces, against which threshold, written by
whoever is measuring. A scenario is the CONTENT: written by whoever is making the game, layered by
whoever is modding it. Different authors, different lifetimes, different tools.

- [x] **An XML reader in `src/core`, bounded like everything else here**: elements, attributes, text,
      the five entities. No DTD, no namespaces, no external entity -- each refused by name
- [x] **The reader is the same shape as `Json`**: parse once into a flat store, a `Ref` that walks it,
      no allocation per query

## The API this implies, and the one property it must have

- [x] **`include/outshine/` is the whole of what a client sees.** No internal header is reachable and
      no internal type appears in a signature -- a handle and value types, the way SDL and GL spell one
- [x] **create -> load -> run -> destroy**, which is Unreal's `PreInit · Init · Tick · Exit` with RAII
      doing the last one -- built, and proved by a test layer that compiles with `-Iinclude` alone
- [ ] **The client is handed ACTIONS and not keycodes**, because the binding is the scenario's
- [ ] **Where to render is a declaration too**: a size and a target, so the same scenario runs into a
      window, into an offscreen frame a test reads back, or into a browser's pane

## What exists today, so the gap is a measurement rather than an impression

`src/scenario/` carries `Mod`, `Scene`, `Stage`, `Standpoint`, `Studio`, `Animation` and `Fields`, and a
declared world today says: **where on Earth, when, the weather, the eye, and what to render** --
`test/unit/scenario/mods/ardeche/mod.json` is 18 scenes of exactly that. It is the MEASUREMENT view of a
scenario: enough to put a camera somewhere and take a picture, and none of the twelve rows above beyond
`clock` and a fragment of `Ground`.

**`Clients::Live` is the runtime that already stands one up and advances it** -- a declaration, a
subject, surfaces, an atlas, a frame count -- and it is the thing the public `Engine` will be a face
for. What is missing is not a runtime; it is the DECLARATION, and the twelve rows are its decomposition.

## The declaration is complete and the runtime's shortfall is a number

**A scenario in the shape above is authored, read and carried today.** `include/outshine/Scenario.h`
is the tree -- identity, layers, world, providers, generators, compositors, render, lighting, assets,
placements, surfaces, actors, physics, clock, input, state -- in value types a client can build in code
or load from XML.

**WHAT THE RUNTIME DOES NOT YET ACT ON IS PUBLISHED**, which is the difference between a declaration
and a lie. `Engine::Carried()` answers it, and over the test's scenario it reads:

```
1 providers · 1 generators · 1 compositors · 1 surfaces · 1 actors
1 input bindings · 1 persisted values · a world origin · a physics dial · a clock
```

*Every one of those is a row of the table above waiting for its runtime, and none of them is silently
dropped.* The one row the runtime does act on is `assets` plus `render` plus `lighting`, which is the
studio leaf -- and that is now a measured shortfall rather than an impression.

- [ ] The eleven rows above reach a runtime, one at a time, each with the case that decides it

