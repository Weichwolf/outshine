Type: issue
Area: generators, render, clients
Tags: architecture, measured

# The world composition path has a consumer

`CLAUDE.md`'s CURRENT class diagram draws a path and colours all of it GREEN -- right
responsibility, right layer:

```
Ground --> Forest & Buildings & Water & Infrastructure
Forest & Buildings & Water & Ribbon & Subject --> DrawList
DrawList --> SubjectDraw --> Renderer
```

**Nothing outside `src/` walks it.** Measured over the whole tree:

| node | files in `src/` | files in `test/` | files in `tools/` + `apps/` |
|---|---|---|---|
| `Forest` | 4 | 2 | **0** |
| `Buildings` | 9 | 2 | **0** |
| `WaterField` | 4 | 0 | **0** |
| `Infrastructure` | 3 | 1 | **0** |
| `DrawList` | 5 | 4 | **0** |
| `RegionForge` | 3 | 0 | **0** |

The one client in the tree that assembles a world picture -- `apps/driver/stills`, 768 lines --
**builds its own terrain grid, its own far ring and its own road ribbon in its own C++** and
calls none of the six. `apps/driver/window` does the same in 513 lines.

The result, measured: a still from the driver's seat at km 16.8, 35.5 and 59.7 of the
Munich--Hamburg route shows a grey ribbon on a flat green plane under a gradient sky. No
buildings, no vegetation, no ground texture, no road markings, no kerbs. And that picture is
**correct for what is declared**: `apps/driver/f31.scenario` contains no `<world>` element at
all -- no sphere, no ground, no sky, no clock, no weather. It declares a render size, one fixed
light, one asset, one vehicle, two views, a player and five key bindings.

## Sharpened by walking the call graph: the path ends at a dead facade

The six nodes are not merely unreached from outside `src/`. Walked inward, every one of them is
reached by **exactly one** thing:

| node | what in `src/` names it, other than itself |
|---|---|
| `Forest` | `src/clients/Sim.{h,cpp}` |
| `Buildings` | `Sim.{h,cpp}`, plus `BuildingField.cpp`, `OsmLayer.h`, `World.{h,cpp}` |
| `WaterField` | `Sim.cpp`, `World.h` |
| `Infrastructure` | `Sim.h` |
| `RegionForge` | `Sim.h` |

And `Sim` itself:

```
$ grep -rln '"Sim.h"' src test tools apps
src/clients/Sim.cpp
test/render/outshine/world/AWorldStandsUpWhereItIsDeclared.cpp
```

**798 lines of facade, included by its own translation unit and one test.** Nothing else in the
tree. Every real client -- `apps/driver`'s four cases -- reaches `outshine::Sim::AssembleDrive`
and `DriveTick`, the FREE systems that replaced Journey, and never touches `class Sim`.

So the world composition path is not unused by accident. It hangs off a facade the tree already
routed around, and the CURRENT diagram's red on `Sim` -- *"a hand-wired god facade the component
model replaces"* -- understates it: it is a god facade whose only caller is a test written to
call it.

## Why this is an architecture finding and not an app backlog item

Three of the tree's own rules are involved, and each is broken in a different direction:

| rule | what is true instead |
|---|---|
| *content = data, engine = verbs; scenarios declare, the engine behaves* | the app computes the world in C++; the scenario cannot declare one |
| *a diagram that lies about the tree is itself a finding* | the CURRENT diagram draws six edges into `DrawList` that no consumer walks. Green there means "right layer", not "reached" -- and nothing distinguishes a sound layer from an unreached one |
| *board:1573: a driver binary that computes grading, rings, relays or cameras in its own C++ is engine work wearing a tool's clothes* | 1281 lines of exactly that, in two files |

A layer with no consumer is not proven. `Forest`, `Buildings` and `Water` may be correct, may be
half-written, may not compile against a real draw list -- nothing in the tree says. Their green
is an architectural judgement about their SHAPE and it has been read, by everyone including this
queue, as a statement that the world gets drawn.

## What will be true

- [ ] A scenario can DECLARE a world -- the sphere, its ground, and which of the surface fields
      it wants drawn -- and the engine composes it. No client builds terrain or ribbon geometry
      in its own C++.
- [ ] `apps/driver` loses its geometry construction to the library, and what stays is the
      declaration plus the loop.
- [ ] Proving test: a still from a declared world, taken through the composition path, in which
      buildings and vegetation are present because the scenario asked for them. Negative
      control: the declaration removed -> they are absent and the picture says so.

## What board:1806 settled, and what it did not (2026-08-24)

Six of the path's nodes now have twins that prove something about them --
`OsmField`, `StreetField`, `WaterField`, `BuildingField`, `RegionForge` and (from the same
walk) `Frustum`. Three doors narrowed to make that possible, and `OsmField::Accept` in
particular means the whole ground stack is now reachable from a proof that fetches nothing.

**That answers "is it checked". It does not answer "is it reached".** A layer with twins and no
consumer is a proven layer nobody calls, which is a smaller defect than an unproven one and
still a defect. The measurement stands exactly as filed:

```
$ grep -rln '"Sim.h"' src test tools apps
src/clients/Sim.cpp
test/render/outshine/world/AWorldStandsUpWhereItIsDeclared.cpp
```

## The box the diagram owes

`CLAUDE.md`'s CURRENT class map colours by ARCHITECTURE -- green is "right responsibility in
the right layer". Nothing in it distinguishes:

| | |
|---|---|
| a node that is sound AND reached | `ReferenceLine`, `Rig`, `SkyStage` |
| a node that is sound and **reached only through a red facade** | `Forest`, `Buildings`, `Water`, `Infrastructure`, `RegionForge` |

Both are green, and a reader -- including this queue for two rounds -- takes green to mean the
world gets drawn. The map is the architect's to write (owner directive, same session), so the
form is proposed here rather than applied: a node whose only path to a client runs through a
node this map colours RED is itself in question, and the map should say which nodes those are.

`harness/claims/EveryNodeTheMapDrawsIsNamedByAProof` is the walk that made this visible and is
the natural place for the second question to be asked from.

- [x] The CURRENT diagram distinguishes a node that is SOUND from a node that is REACHED, or
      the reached-ness is a separate claim the gate walks. **Half**: the proof question is a
      claim now; the reach question is stated above for the map's owner.
