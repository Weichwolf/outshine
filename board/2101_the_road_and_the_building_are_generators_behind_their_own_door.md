Type: debt
State: active
Area: engine, generators
Tags: architecture, owner
Depends: 2121

# The road and the building are GENERATORS, and the engine keeps only the sequence

**Benchmark** -- Unreal: a `ULandscapeSplineComponent` never edits the landscape's vertices; it
writes a deformation request into the heightmap layer, and foliage asks `GetHeightAtLocation`.
RAGE: terrain is a heightfield the streaming owns, props query it, roads are baked into it at
cook time. **Both agree**: a thing that stands on the ground ASKS the ground and HANDS BACK a
request; it never receives the mesh, and the engine never derives the thing.

## Landed 2026-09-05: the road derivation is `Generators::Corridors`

Every function of the derivation moved as it stood into `src/generators/road/Corridors.{h,cpp}`
(1 285 + 243 lines), one class behind `Shipping::Corridors()`: `Lay(site, geometry, yields,
notes)` takes the ground stack, the frame, the drape and the class field, appends the streets
part, hands back the corridor `Yields` and its measures as `Measure`s the engine publishes
verbatim. `Laying.cpp` 2 155 -> 964 lines, `Grounds` still 560 of them. `Road` and `Street`
read 0 in `src/engine` and `include/`; what the claim still counts there: `Bridge` 3, `Tunnel`
3 (`Asking.cpp`, the OSM way counters), `Building` 5, `Terrain` 8 (`TerrainField` and its kin,
world-tier type names the halo uses), `Seat` 12, `Tree` 5. The nine references bit-identical.

## Where it stood, measured 2026-09-04

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

## Decided 2026-09-05: the road generator adopts CARLA's Digital Twin pipeline, corrected

Read to the source, CARLA's Digital Twin Tool (`CarlaTools/.../OpenDriveToMap.cpp`, MIT) is a
thin driver over two readable bodies, and both are adopted here as the BASELINE of what a road
generator produces:

| stage | CARLA / SUMO holds | outshine takes | corrected because |
|---|---|---|---|
| **type table** | `osmNetconvert.typ.xml`: per `highway=*` the lanes, speed, priority, one-way and permissions (motorway 2 lanes 39.44 m/s prio 14 … residential 1 lane 13.89 m/s prio 3; footway/path 2 m wide, pedestrians only; `railway.*` with `rail`/`tram`) | the same table as a JSON catalogue the way generator reads, one source for lanes, width, speed, class | none -- a declared table where `kLeastRoadM` and per-class guesses stand today |
| **network** | `netconvert`: ways become edges between nodes, nodes joined within a radius, one-way and lane counts resolved, connections computed per lane | the network of board:2133 (road, rail, path in one graph) | none in kind; built per TILE on the pool so a walk streams it, which netconvert never has to |
| **junction shape** | `NBNodeShapeComputer`: each edge's left and right lane boundaries extended by `EXT` (100 m, 50 past four edges), parallel edges within `20°` joined, each boundary intersected with its clockwise neighbour, the cut-back distance = intersection offset + a turn radius (`radius * tan(0.5 * turnAngle)`), corners smoothed to `EXT2 = 10 m` | the junction body as that polygon, meshed once per node; the lanes cut back to its rim | replaces `Shortens` (cap bit by fork angle), `BackOffFor`, the `375 m` lift and the 24-round Jacobi levelling: a junction is ONE polygon at ONE height, solved from the meeting profiles |
| **lane geometry** | OpenDRIVE: a reference line (the inner lane border), `<width sOffset a b c d>` per lane, `<elevation>` profile, marks solid/broken per lane position, `driving`/`sidewalk`/`biking`/`rail` lane types | the corridor model: reference line + lane widths + grade profile + lane types, subject-free at the seam | elevation from the DEM's drape along the line and level across (board:2121), not from the OSM z which is absent |
| **mesh** | `MeshFactory`: sample each lane every `vertex_distance` (0.5 m) across `vertex_width_resolution` (8), straight lanes as two vertices, marks as quads (`solid` at the resolution, `broken` stepped `resolution * 3`), sidewalks extruded 6 vertices a sample, walls to `wall_height`, chunks of `max_road_length`, then `MergeAndSmooth`: a 100-round Laplacian (λ 0.5) over an R-tree of all vertices | the sweep by lanes at a vertex distance derived from the LOD (board:2123), marks and sidewalks as CARLA cuts them, chunked by tile | NO relaxation: the seam between two chunks is the same profile evaluated twice, so it meets by construction; CARLA smooths because its heights come from a heightmap sampled after the fact |
| **buildings** | `ProceduralBuildingUtilities`: footprint from OSM, height from OSM or by area, style by size | already the building generator's (board:2138 fronts it to the street) | -- |
| **terrain** | a 12 800 m landscape from a heightmap or a synthetic deformation | the DEM lattice of board:2115 | CARLA's twin is flat where OSM is silent; the Earth is not |

What "real time" means here and CARLA never needed: netconvert and the mesher run once per
map over the whole extract. Here the same three stages run PER TILE on the pool, in declared
order, with the junction polygons of a tile's nodes computed from the edges of the tile and its
halo (a node's shape needs every edge that meets it, so the halo is one edge length deep),
and the frame places finished corridor pieces the way it places baked buildings (board:2122).
That is the item's `Corridor` seam filled with CARLA's content.

Order: the type table and the network first (they replace `DesignLane`'s guesses and feed
board:2133), then the junction polygon (it deletes the four passes above), then the lane mesh
with marks and sidewalks, then the per-tile split.
