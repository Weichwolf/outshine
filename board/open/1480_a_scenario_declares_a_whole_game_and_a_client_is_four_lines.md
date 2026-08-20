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
- [ ] **Every layer of the table above is optional and its absence is a named default**, never a
      silent one: a scenario that declares no generator gets no generated content and says so

## The API this implies, and the one property it must have

- [ ] **`include/outshine/` is the whole of what a client sees.** No internal header is reachable and
      no internal type appears in a signature -- a handle and value types, the way SDL and GL spell one
- [ ] **create -> load -> run -> destroy**, which is Unreal's `PreInit · Init · Tick · Exit` with RAII
      doing the last one
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
