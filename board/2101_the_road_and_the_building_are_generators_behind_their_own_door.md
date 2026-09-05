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

## Landed 2026-09-05: a way is EDGES between nodes, a junction is ONE polygon at ONE height, and a road follows the terrain

The first CARLA/SUMO stage from the table above, and three defects it uncovered on the way,
each measured before it was touched:

- **edges.** `SplitsEdges` cuts every designed way at its shared nodes (an OSM vertex more
  than one way owns, or a crossing the snap filed), the way netconvert makes edges of ways;
  paving, yields, fit and trim run per edge. OldTown: 13 931 edges from the ways
- **the junction polygon.** `ShapesJunctions` is `NBNodeShapeComputer` reduced to what a
  height field needs: the legs at a node sorted by their angle 10 m out (`kAngleLookaheadM`,
  SUMO's `ANGLE_LOOKAHEAD`), every boundary ray -- centreline ± half width, from the ONE
  centre (the mean of the roots; netconvert joins nodes to one point, this tree's snapped
  crossings stood up to 10 m apart and their boundaries missed each other by 97 m) -- cut
  against its clockwise neighbour's, legs within 22.5° or past 160° of each other uncut
  (SUMO's parallel and continuation rules), the cut plus a 4 m radius (`kJunctionRadiusM`,
  SUMO's default junction radius), capped by the leg's length. The polygon's height is the
  DRAPE at the centre, or the deck's `EndM` where a leg is a bridge. The body is the same
  sealed fan, its gates now on the rim; the legs' rim stations take the rim's height; a
  node with two continuing legs (past 160°) gets no body. OldTown: 6 074 junctions, 686
  continuations, deepest cut 31.8 m
- **the approach.** `DeckOrRamp` fades the rim's OFFSET from the leg's own profile over
  `|offset| / gradient` metres (the class gradient, 10 % where a class states none,
  `kSteepestApproach` [SET, the steepest a car road gets]) -- the profile keeps its shape
  and the junction pulls only its end. Deleted: `Shortens` and its cap bit (`BackOffFor`,
  `SharpestForkFor`, `kTrimMostWidths`), the Jacobi levelling (`RelaxMeetings`, 24 rounds
  that never converged and shifted whole lanes), `LevelsDeckOrApproach` (which raised every
  station of a lane to the CHORD between its end heights -- a road across a valley filled to
  the earthwork bound), and the gates at lane ends

Three defects found by the measures on the way, and where they stood:

1. **the corridor's yield was measured against the ground MIRRORED across the frame's east
   axis.** `PaveLane` sampled the drape at `{E, -S}` where `GroundUnder` and the drape both
   speak south-positive; the yield and the verge relief of every corridor piece compared the
   road with the ground on the other side of the origin (`Laying.cpp` before the move, the
   same three lines). Measured at OldTown: 87 000 corridor pieces "worth a stamp" -> 15 000
   once the sample stood on the road; the 60-90 m yields of board:2121's note were this
2. **`RoadMesh::Design` clamped the grade to the class bound and INTEGRATED from the first
   station**, so a road on a hill steeper than its class floated off the ground for the rest
   of its length (the 40 m "rim lifts"); its crest relaxation (24 passes) then smoothed a
   whole hill into an arc. The profile follows the terrain now, as RAGE's, CARLA's and
   OpenDRIVE's do; `Design` left the seam, the class gradient is a measure and no longer an
   edit
3. **the junction rays took the 100 m chord** and two legs 30° apart at the node read as
   parallel 100 m out; the angle and the ray share the 10 m lookahead now

```
  OldTown, measured 2026-09-05             before      after
  corridor pieces that press the ground    87 016      15 119
  above the graded plane before the press  17.8 m      3.4 m
  below it, filling, before the press      26.6 m      10.6 m
  the most a rim lifts a road              --          11.2 m
  the longest approach                     --          112 m
  the deepest cut                          --          31.8 m
  everything Paves did                     1 235 ms    814 ms
```

Two more things the gate found before this landed, both measured:

- **the junction body and the pressed ground were COPLANAR, and the picture was not
  deterministic.** The polygon's yield pressed the lattice to the deck's height, the body's
  top stood at the same height, and which triangle won a pixel depended on the order the
  lattice instances rasterised: Heidelberg rendered four different digests in eight runs
  (`git stash` of the change: eight of eight identical). Bisected by switching each stage
  off: the bodies alone or the yields alone were deterministic; both together were not. The
  references' answer (RAGE, Unreal, CARLA) is that the terrain is the SUBGRADE and the
  pavement lies on it: the yield presses to the deck less a pavement lip
  (`kPavementLipM` 0.05 m [SET, an asphalt course over its base]), the body keeps its
  0.30 m of sealed depth below the deck, and nothing is coplanar. Eight of eight identical
  after
- **the body's normals were seeded up and summed across creases**, never normalised, and
  four float sorts on a bearing decided its fan with no tiebreak (board:2148 carries the
  audit's list). The body is flat faces with their own vertices now, each normal written
  unit length from a known winding, and every sort on a bearing breaks its ties by edge and
  end. Pure black at Heidelberg 11 -> 0; the near-black under 20/255 that remains (164 in
  the reference, 473 after) is the wedge of board:2144 between two houses, which a pressed
  node beside it widened -- counted there

**And the lesson the owner named before the numbers did: CARLA shapes every junction because
CARLA is offline.** The first cut shaped all 95 830 junctions of Shibuya's ring and its heap
read 1.1 GB. A road is held to the same rule as a building (`Unseen(size, focalPx, awayM)`:
a feature narrower than a pixel at its distance is not built): a way whose width projects
under a pixel is left to the ground's class, and two-leg nodes are continuations with no
body. Measured 2026-09-05:

```
                             Shibuya                 OldTown
  ways left to the ground    67 954 of 79 674        3 045 of 5 419
  junctions shaped           88 858 -> 17 465        5 643 -> 2 476
  everything Paves did       3 553 -> 1 428 ms       792 -> 638 ms
  heap live after the lay    929 -> 666 MB           249 -> 240 MB
  preload waited             9.3 -> 6.8 s
```

## Decided 2026-09-05: the road is TWO products -- a MAP that is information, and a STRUCTURE that is its picture

Decided with the owner. The engine has one kind of thing a generator hands it: a STRUCTURE
-- a footprint that stamps the ground, a body, a material -- baked per tile on a worker and
placed as a piece in the arena (board:2122) under the tile's level of detail. A paved road is
that: a long thin footprint from kerb to kerb, stamped like a pad but with a grade along it,
and a slab for a body; a bridge is a roof, a body with no stamp under its deck and stamps
only where it touches (abutments, piers). Unreal's spline mesh and RAGE's road bounds are
the same thing beside their buildings; only CARLA separates them, because it is offline.
What stays the road's own: it crosses tile borders and must meet itself there with one
profile (a halo of neighbouring stations, as the terrain's), it is a NETWORK with lanes
and turns (board:2133, board:2134), and a deck's clearance is a crossing's question. So the
road generator keeps netconvert's semantics and hands STRUCTURE pieces to the buildings'
pipeline -- the next stage of this item, replacing the corridor bodies and the whole-ring
lay.

Decided with the owner, and the separation is the design: the generator's two products are
kept APART.

| product | what it is | level of detail | who reads it |
|---|---|---|---|
| **the MAP** | the network: nodes, edges, lanes, widths, one-way, turns, right of way, crossings and their clearance -- netconvert's output as data | none; whole, held as data | wayfinding (board:2133), the driver (board:2134), a mind's `walk(to)` (board:2142) |
| **the STRUCTURE** | the picture of a piece of the map: footprint stamp, slab, kerb, marks; a bridge a roof | the tile's rung, baked and placed like a building (board:2122) | the renderer, the press |

The structure DERIVES from the map, per tile, and never the other way round; a junction's
polygon is computed from the map's edges at that node and becomes structure. Nothing in the
map knows a vertex; nothing in the structure knows a turn.

**Seen by the owner in the final Heidelberg picture, lower left**: a road on a steep cross-slope
stands on a grey SHELF. It is not a road mesh (a ground road is no geometry yet); it is the
corridor's fill on the downhill side with its 2:3 batter, which a 26 m lattice cannot draw
and so draws as a cliff. And the owner's question follows: once a road IS geometry, how does
it not float? CARLA's answer: there are no projected roads at all; every way is a lane mesh,
sidewalks are lanes of it, and the Landscape takes its heights by line trace FROM the road
mesh (`GetHeightForLandscape`) -- the terrain follows the road, so nothing floats and the
same shelves appear. This tree's answer, by the type table's surface:

| kind | the structure | the ground |
|---|---|---|
| sealed road (asphalt, concrete) | slab, kerb, and a SKIRT: side faces from the kerb down to the ground, as a house's walls reach `FootM` | stamped kerb to kerb only (the subgrade); no batter apron on the lattice any more |
| unsealed way (track, path, bridleway) | none, projected | the class, a shallow rut, no fill |
| paved area (parking, plaza, pedestrian area) | a pad slab with a skirt, a building of no height | stamped flat |
| bridge | a roof: deck without a stamp, abutments as skirts | nothing under the deck |

A road cannot float because its skirt always reaches the ground, however coarse the lattice;
the lattice gets no cliffs because the batter belongs to the structure and not to the
ground. The corridor's `ApronM` and the `kBatterRun` fill of a corridor go with that stage.

## Landed 2026-09-05: the MAP, first slice -- the type table's columns and the network held once

- the streets rules of `vegetation.json` carry `speedMps`, `priority` and `sealed` (SUMO's
  `osmNetconvert.typ.xml` for the first two, origin beside them; `sealed` this tree's own
  column, the line between a structure and the ground's class); `VegetationTemplates::Rule`,
  `StreetField::Way` and `Path::WayClass` carry them through
- `Corridors::MapOf(stack)` lays every way into a `Path::Network` with its lane index as a
  `Tag` and weaves it (nodes snapped within `kNodeSnapM`, edges, loose ends tied); the engine
  holds it as `World.Network` and rebuilds it only when the ways change. OldTown: 5 411 ways,
  27 540 nodes, 62 964 edges, 7 318 nodes where three or more edges meet
- the crossings (the decks' source) are read from the map; weaving sorts the ways into a
  declared order, so the crossings are sorted into one too (lane tags, then positions) --
  the picture had inherited a sweep's order, and a station's snap to "the first crossing
  within 10 m" with it. Moved: OldTown 136 pixels, Heidelberg 0.97 %, ZurichPlan 1.7 %, the
  rest under 0.3 %; looked at Heidelberg's bank road and Zurich's bridge road: junction
  bodies that changed which node they belong to, nothing else

Still to come for the map: its own elevation profile (C1, the driving surface analytic,
errors under a centimetre), the junction polygons computed from it, the lanes as geometry;
`Path::Network` then stops being a routing graph with a tag and becomes the map.
