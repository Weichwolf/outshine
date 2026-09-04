Type: debt
State: open
Area: engine, world, render, generators, base
Tags: architecture, owner

# Every name is the one a reader EXPECTS, and none is this tree's own metaphor

**Benchmark** -- Unreal: `AddComponent`, `SetActorLocation`, `GetWorld`, `BeginPlay`. RAGE:
`rage::grcDevice::SetRenderTarget`, `fwEntity::GetTransform`, `CPhysical::SetVelocity` -- the
same plain verbs under a prefix. Filament: `Engine::create`, `Scene::addEntity`,
`RenderableManager::Builder::build`; Cesium: `Cesium3DTileset::updateView`. Every one of them is
guessed right the first time. **Both agree, and the door's two bodies with them**: a method is a
plain verb and a class the noun of what it holds; the vocabulary is the engineer's, never the
author's.

## Where it stands, measured 2026-09-04

```
  engine   Hands, HandsPiecesOver, HandsThePavingOver, Wears, WearsPieces, Framed, Forgets,
           Restand, Grounds, Paves, Lays, Models, Grows, Bakes, Lands, Posts, Opens, Digests,
           Carries, Stands, Watches, Focuses, TellsWhatCrossed, TellsTheRelief ...
  world    Footprints().Next/Take/Accept, Ingested, Settle, Overflowing, Drained, Restand
  render   HandTables, HandPlacements, HandStreams, Retable, Cross, Crossing, Borrows,
           Bound, Room, Grow, WearPieces, CastsBelow, ShadowedBy
  generators  Cover, Yield, Lay, Wants, Mesh, Shaped, Shapes, Finish, WholeOf, RowCut
```

Measured cost: the session that closed board:2122 and opened board:2115 spent more tool
reads looking up what a name did than on the design itself -- `Hands` alone was read at five
sites before its meaning (place into the renderer) was certain. The owner's ruling, 2026-09-04:
**the names must match what the reader (and the model that writes here) expects.**

## The solution

One sweep per tier, the compiler as the oracle (rename the declaration, let the errors name
the callers), the digests unmoved because a rename moves no byte:

| today | expected |
|---|---|
| `Hands(x)` / `HandsPiecesOver` / `HandTables` | `Place(x)` / `AttachPieces` / `UploadTables` |
| `Wears(surfaces)` / `WearPieces` | `SetSurfaces` / `SetPieceSurfaces` |
| `Framed(frame)` / `Into(live)` | `SetFrame` / `AttachTo` |
| `Forgets(tile)` | `Remove(tile)` |
| `Restand` / `Grounds` / `Paves` / `Lays` | `Recenter` / `BuildGround` / `BuildRoads` / `BuildTerrain` |
| `Posts` / `Lands` / `Bakes` | `Post` / `Collect` / `UpdateBakes` |
| `Ingested` / `Drained` / `Overflowing` | `Complete` / `Idle` / `OverBudget` |
| `Cross` / `Crossing` | `Upload` / `Upload` (the record) |
| `Borrows` / `Bound()` / `Room` / `Grow` | `IsBorrowed` / `Residency()` / `Reserve` / `Grow` |
| `Cover().Yield/Lay` | `Terrain().Press/Build` |

A name from the references' vocabulary wins over a plain one where both fit (`Residency`,
`Placement`, `Batch`, `Cluster`, `Page`).

## What will be true

- [ ] No method in `src/` is a verb a reader has to look up: a case lists every public method
      name against a small allowed vocabulary of verbs and the tree's nouns, and reads 0 outside it
- [ ] The nine references unmoved after every sweep (a rename moves no byte)
- [ ] Negative control: rename one method back to a metaphor and the case names it
