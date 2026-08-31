Type: task
State: open
Area: engine, generators
Tags: layering, naming, benchmark

# `Picturing` dissolves into the THREE things both references name

**Benchmark** — **Unreal**: `UWorld` holds the world, `ULevel` a chunk of it, `FScene` is what the
renderer sees, and GENERATION lives outside all three, in PCG. **RAGE**: `CGameWorld` holds it,
`fwSceneGraph` is traversed, `gtaDrawable` is what draws, and the map pipeline feeds them from
outside. **They agree**: derive, hold, hand over — three things, three places.

`Picturing.cpp` is **2 574 lines** doing all three, which is why it needed a name neither reference
owns. A name is a promise; this one promises a thing that is not one thing.

## THE HOW

| what it does today | where it goes | after whose example |
|---|---|---|
| derives geometry from OSM — roads, buildings, water, ground cover | `src/generators/`, one `Generates` per kind, behind `include/Generate.h` | Unreal's PCG; RAGE's map pipeline |
| holds the world and places what was made | the engine, beside `Assembly.cpp` (251 lines) | `UWorld` / `CGameWorld` |
| hands the renderer what it draws | the scene handover already in `src/scene/` | `FScene` / `fwSceneGraph` |

**The derivation goes first**, because it is the half that makes the other two small — and because
board:2083's rule cannot be met until it does: a corpus validator that links `libgenerators.a` and
nothing else is only possible once the derivation is in it.

## What will be true

- [ ] `Picturing.cpp` no longer exists; nothing is renamed in place
- [ ] `src/engine` names no street and no building — 93 subject nouns today, and
      `TheEngineNamesNoSubject` goes green by the move rather than by editing the claim (board:2079)
- [ ] Each generator stands in `Ships` and answers `make(Ask, Geometry&)`
- [ ] Negative control: the places suite draws the same pictures, digest for digest, across the
      move. A refactor that changes what is drawn changed more than where the code lives

## Not covered

What the geometry IS. This moves code and renames nothing about the world; board:2087 and board:1499
own how a road is derived, and this item only decides where that derivation lives.
