Type: task
State: open
Area: engine, generators
Tags: layering, naming, benchmark

# `Picturing` dissolves into the THREE things both references name

**Benchmark** — **Unreal**: `UWorld` holds, `FScene` is what the renderer sees, PCG generates,
outside both. **RAGE**: `CGameWorld` holds, `fwSceneGraph` is traversed, the map pipeline feeds them
from outside. **They agree**: derive, hold, hand over.

## How

| today, in one file | goes to | after |
|---|---|---|
| derives from OSM: roads, buildings, water, cover | `src/generators/`, one `Generates` per kind | PCG / map pipeline |
| holds the world, places what was made | the engine, beside `Assembly.cpp` | `UWorld` / `CGameWorld` |
| hands the renderer what it draws | `src/scene/` | `FScene` / `fwSceneGraph` |

## Why

`Picturing.cpp` is **2 574 lines** doing all three, which is why it needed a name neither reference
owns. The derivation moves FIRST: it is what makes the other two small, and board:2083's validator
cannot link `libgenerators.a` alone until it has.

## What will be true

- [ ] `Picturing.cpp` no longer exists; nothing renamed in place
- [ ] `src/engine` names no street and no building — 93 today; `TheEngineNamesNoSubject` goes green
      by the move, never by editing the claim (board:2079)
- [ ] Each generator stands in `Ships` and answers `make(Ask, Geometry&)`
- [ ] Negative control: the places suite draws the same pictures, digest for digest. A refactor that
      changes what is drawn changed more than where code lives
