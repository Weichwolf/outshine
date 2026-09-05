Type: debt
State: open
Area: engine, generators
Tags: architecture, owner
Depends: 2121

# The road and the building are GENERATORS, and the engine keeps only the sequence

**Benchmark** -- Unreal: a `ULandscapeSplineComponent` never edits the landscape's vertices; it
writes a deformation request into the heightmap layer, and foliage asks `GetHeightAtLocation`.
RAGE: terrain is a heightfield the streaming owns, props query it, roads are baked into it at
cook time. **Both agree**: a thing that stands on the ground ASKS the ground and HANDS BACK a
request; it never receives the mesh, and the engine never derives the thing.

## Where it stands, measured 2026-09-04

```
  src/engine/Laying.cpp                    2727 lines
  subject nouns in src/engine + include/    153  (Road 49, Street 44, Bridge 31, Seat 10,
                                                  Tree 5, Wheel 4, Building 4, Tunnel 4)
  of those in Laying.cpp                     95, EngineHeld.h 28
  TheEngineNamesNoSubject                    RED
```

The road derivation is all `Engine::State`: `DesignLane`, `PaveLane`, `Bridges`, `Shortens`,
`LevelsWhereWaysMeet`, `Crosses`, `RaiseDeckOver`, `EasesRamps`, `Paves` -- and `Paved`
(`EngineHeld.h:454`) is sixty fields, most of them tallies. Only the SWEEP sits behind the seam
(`RoadMesher` -> `generators/road/RoadMesh.cpp`). The seam types `RoadStation`, `RoadGate`,
`Bridge`, `Tunnel` live in `world/ground/` where the engine may see them.

The generic helpers are OUT and reachable: `Refine.h::Divide`, `Census.h`, `Drape.h` (a BVH
now), `geo/PlaceKey.h`, `TangentFrame::CarryIntoTheFrame`. The interface is right too: `Paves`
writes no ground vertex, it hands `Yields` to the press.

## The solution

`Laying.cpp` becomes `generators/road/RoadDerivation.cpp` behind a fourth seam, and the seam
speaks a subject-free vocabulary:

| the engine says | the generator hears |
|---|---|
| `Corridor` -- a swept ribbon with stations and gates | a road, a rail, a canal, a runway |
| `Deck` -- a corridor raised above a crossing | a bridge |
| `Bore` -- a corridor below the ground | a tunnel |
| `Yields` -- an outline and the profile it wants pressed | the deformation request, unchanged |

The engine keeps `Grounds` as the SEQUENCE -- ask the ground, collect the yields, press, compose
-- and nothing else. The tallies in `Paved` become the generator's `Notes()` the frame pulls
(board:2108), so the ledger lines move without the engine knowing their names.

The order follows the lattice: with a projected grid and a CDT (board:2115) the corridor's
levelling and the bridge's ramp easing are a grade along a centreline (board:2121) and not a
relaxation over a mesh, so the derivation that moves is the SMALLER one.

## What will be true

- [ ] `TheEngineNamesNoSubject` is GREEN: 0 subject nouns under `src/engine/` and `include/`
- [ ] The road derivation and the building derivation are generators reached only through
      `include/generate/Generate.h`, registered beside the tree grower
- [ ] `src/engine/Laying.cpp` is gone; what is left of it in the engine is `Grounds` as the
      sequence, under 200 lines
- [ ] No generator receives the ground mesh: a case reaches for it through the door and cannot
- [ ] The pressers are applied in a DECLARED order (`YieldGround` sorts by `YieldM` only; ties
      fall to input order today), and a case that shuffles two generators' yields renders the
      same bytes
- [ ] The generic helpers each carry a case with a vendor oracle where one exists (board:2103)

## What will show I was wrong

`make shots` after the move, all nine digests: identical means the cut was a move; a moved one
names the line that changed behaviour and the move stops until it is understood.
