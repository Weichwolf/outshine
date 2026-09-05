# test/lab -- where a non-trivial answer is PROVED before it is implemented

**Python proves correctness; the C++ in `src/` makes it real-time.** A structural or numerical
question that cannot be answered by reading a reference is answered here first: the inputs are
the engine's own (the same OSM, the same DEM tiles, decoded by numpy), the solution is written
with the scientific libraries (numpy, scipy, shapely, trimesh, networkx, osmnx), and a PROOF is a
check that goes red -- a constraint violated, a residual above tolerance, a negative control that
passes. Only then is the algorithm written in C++, and the lab's result is the oracle the C++
answer is measured against.

Environment: `python3 -m venv test/lab/.venv && test/lab/.venv/bin/pip install numpy scipy shapely
matplotlib trimesh mapbox_earcut networkx pillow osmnx`. Data lands under `$TMPDIR/outshine-lab/`
and never in the tree. Run an experiment as `test/lab/.venv/bin/python test/lab/<dir>/<exp>.py`.

## Inventory

| experiment | question | solution | proof | to C++ |
|---|---|---|---|---|
| `roads/profile.py` | what does the DEM say along every OSM way, and where are the outliers? | sample terrarium z14 at every node, grades per class | measured 2026-09-05 at OldTown: 3 340 ways; steepest 0.568 on a path, secondary 200 of 910 segments over its 7 % (94 of them under 12 m long) -- the outliers are aliasing of a coarse DEM against short segments, plus real steps | the measurement itself is `Path::Network::Elevate`'s (board:2101) |
| `roads/band.py` | the road profile: one height per OSM node, smooth where the DEM cannot resolve, faithful where it can, a bridge's deck free of the river bed | a QP (cvxpy/OSQP): minimise `sum w (z-dem)^2 + l^4 sum kappa^2` with `w = 0` on a bridge's or tunnel's interior nodes, `l = 2 DEM postings` (12.4 m at zoom 14, 49 N), the band `|z - dem| <= 4 m` hard [SET: Copernicus GLO-30 LE90 < 4 m], the class's design grade a soft slack that is REPORTED, never forced (netconvert's warning) | measured 2026-09-05 at OldTown: band met to 1e-9, curvature RMS 0.0095 -> 0.0013 1/m, `|z - dem|` max 4.15 m (a deck), grade p99 0.248; controls: the band shut returns the DEM to 1e-5, an isolated 2 588 m way with the band open fits a line (curvature 1.2e-6); with `l` = 0 the hilltops are cut to the band, which is why `l` is the DEM's and not a taste | the normal equations, see the next row |
| `roads/band_iter.py` | the same profile as a C++ can compute it, deterministically; measured at Heidelberg too: the direct solve equals the QP to 0.87 mm; with a grade-fidelity term (mu = l^2) added, the active set of a HARD band oscillates at Heidelberg, and the band model comparison (fixed / slope-scaled / none) gives the same profile to the millimetre at both places -- so the band goes and the direct solve is exact by construction. A C++ port was written and taken out again the same day: on the engine's own graph (versatiles ways, points snapped to nodes two metres apart) it read 30 m/m from a centimetre of station and 1e14 from a deck component with no abutment -- the cases a synthetic bed has to hold BEFORE the port (`roads/synthetic.py`, next) | the normal equations `(W + l^4 K^T K) z = W dem` by a sparse DIRECT solve (scipy SuperLU here, Eigen's SimplicialLDLT in C++), the band by an active set of at most 8 rounds | measured 2026-09-05 at OldTown: the direct solve equals the QP to 0.00000 m with no bound active in one round, 0.04 s for 12 552 nodes; Jacobi-preconditioned CG is NOT the tool: 1 024 iterations still 0.31 m off, the system's condition is `l^4 / ds^4` | `Path::Network::Elevate` (board:2101): a sparse LDLT over the node graph |
| `roads/CASES.md` + `roads/synthetic.py` | every case of infrastructure x terrain the planet has, in isolation, as MAP and as MESH: the bed builds terrain x network, snaps and dedupes as the weave does, solves the map, meshes the legs and the junction regions, and checks I1-I9 | the map as in band.py plus the junction and ramp rules; the MESH: a ribbon per drawn span with a vertex at every break of the cross-section (the crown) and TWO rows at every polyline vertex (one per segment, the wedge between them closing the corner), the station step from the chord's sagitta per segment (L = sqrt(8 tol / k)), the junction REGION (its polygon plus every minor leg's warp band) meshed on a grid whose cell follows from the crossfall and the tolerance, all welded on a quantised key | measured 2026-09-05: 61 of 61 green -- the drawn surface stands within 1e-14 m of the analytic one on flat ground, 0.16 mm over a crest, 6.9 mm over a DEM cliff, 5 mm on noise; no edge carries more than two faces; the roundabout is one junction (netconvert's node joining, the ring whole, its surface an annulus). Four defects the mesh check found and named: a forward tangent collapsed the section at a way's end (3.1 cm), a mitred row spans two directions and no subdivision shrinks its twist (4 cm = grade x offset x tan(theta/2)), one curvature for a whole way took the step from its flattest stretch, and a way drawn through a junction put two surfaces in one place | nothing yet: joining, then the tile seam and the ground's cut and fill, then `Path::Network` and the structure stage (board:2101) ; and two products beside the map: the EARTHWORKS (batter at 1:1.5 filling and 1:1 cutting, the toe by a root find because the batter's height changes with the terrain, a retaining wall above 3 m) and the SEAM (a tile's own solve against the whole's). Measured: the seam's error falls like exp(-halo / l) with l the smoothing length -- 411 mm with no halo, 21 mm at one l, 4.8 mm at two, 1.3 mm at four -- so a tile must fetch TWO SMOOTHING LENGTHS (four DEM postings, 100 m at zoom 12) of neighbouring network or its roads cannot meet, and no welding afterwards repairs it |
| `buildings/synthetic.py` | an OSM footprint plus its tags plus the DEM, made into a CLOSED body that stands on the ground | the roof is a DISTANCE FUNCTION and nothing else: a hipped roof of equal pitch is z = tan(pitch) x dist(p, boundary), whose ridge set is the polygon's straight skeleton and whose level sets are its inward offsets, so the mesh puts a vertex on every ridge by sampling those offsets and bisecting for the last one; a gable clamps the distance across one axis, a pyramid normalises it, a skillion is one plane, a mansard two pitches in series, a dome a circle over it. The wall's TOP follows the roof's edge (a gable end rises to the ridge) and its FOOT follows the ground, buried by 0.5 m [SET]; the pad is the footprint's HIGHEST ground corner, so a building cuts into the hill and never floats, and over water it stands a freeboard above it | measured 2026-09-05: 16 of 16 cases green -- seven footprints (rectangle, L, U, courtyard with a hole, round, thin, tower) on five grounds (flat, 15 %, 40 %, terraces, water) with seven roof shapes; every body watertight (0 open edges, 0 edges over two faces), positive volume, no gap between wall and ground, and every ridge within 5 cm of its own arithmetic (tan(pitch) x inradius, the inradius found by bisection on the inward offset -- a 40 x 40 grid read it half a cell short and the apex 0.13 m low) plus the FACADE as a grammar: bays that divide each wall exactly, storeys from `building:levels` (OSM's rule: `height` is the whole building, levels count the storeys under the roof -- reading it wrong left a three-storey house with one row of windows), an entrance in the middle bay of the street side, a French door wherever a balcony stands, and a blind bay where the roof cuts the cell. Four rungs: L0 the mass, L1 the same geometry with the facade as material parameters, L2 the reveals and the cornice, L3 the balconies -- monotone and checked, with L1 costing no geometry so it shares L0's draw. Every case is drawn as a SHEET (plan with poched walls and dimensions, two elevations with the roof's silhouette and the ground line, a section through the ridge) | nothing yet: the building stage in C++ |

## The sheets

Every case of both beds is drawn as an ENGINEERING SHEET and the whole set stands under
`build/shots/lab/` as `NN_case.png` (roads) and `BNN_case.png` (buildings). The title of each
carries what it is, where it stands and -- for a building -- its type and epoch.

A road sheet is DIN 1356 / RAS-Q: LAGEPLAN with the alignment, both carriageway edges, the
junction surfaces, the earthwork toes, stationing along the ROUTE, a north arrow and a scale
bar; LAENGSSCHNITT along the whole route with terrain, DEM, gradient, cut and fill hatched
apart, grades in percent, the structures marked with their span division and their piers drawn;
three QUERSCHNITTE with the crossfall, the kerb, the batters and their slopes, the fill and cut
heights and the toe's reach -- and for a deck the superstructure instead, a slab of its span's
depth with parapets, piers and the clearance dimensioned. A building sheet is a plan with the
wall poched and dimensioned, two elevations with the openings, the balconies, the storey levels
and the roof's true silhouette, and a section through the ridge.

The sheets are the review: reading them as an engineer found and fixed a junction shape that
was a triangle pointing away from its minor leg (two opposite legs are parallel and have no
corner between them -- the edge LINES bound the surface instead), a longitudinal section drawn
along one way instead of the route (a bridge is three ways and the section showed a plateau
where the valley is), a 280 m bridge given a single 14 m deep span (it is six fields of 47 m),
a deck's cross section drawn with a 30 m embankment into the river, a building's elevation
with no roof at all (the walk into the building used the outward normal) and gable ends rounded
off by a grid, an industrial hall with a house door instead of a loading gate, and a sheet
whose title block and cross section disagreed about what "Damm" means.

## Open questions -- the labs still to run

Each becomes a row above when its experiment exists. Research briefs with references land in
`research/` first (who has done it and proven it, readable repos, papers, what SDL_GPU reaches).

| area | question | first reference to read |
|---|---|---|
| roads, structure | a road as geometry: lanes from the map's profile with cross-slope, kerbs, junction polygons from the net, seams closed at tile borders, vertices snapped, no cracks; bridges as decks between abutments | CARLA `OpenDriveToMap`/`MeshFactory` (0.5 m vertex distance), SUMO `NBNodeShapeComputer`, OpenDRIVE elevation/superelevation |
| buildings | what CARLA's OSM plugin and blosm do that a plain extrusion does not: facades, roofs, window grids; which of it is CPU geometry and which GPU detail | `research/world.md` |
| every OSM structure | rails, walls, fences, power lines, street furniture -- CPU geometry vs GPU detail per kind | `research/world.md` |
| ground cover | grass, soil, litter, stones, sand, snow at PS4-class budgets: instancing, compute culling, splatting, virtual texturing | Ghost of Tsushima GDC 2021; `research/world.md` |
| water | ocean (FFT), lakes, rivers with flow, shorelines, refraction/reflection through compute + raster | Tessendorf; `research/world.md` |
| clouds and sky | Nubis-class volumetric clouds driven by real weather; Hillaire 2020 sky | `research/world.md` |
| materials | parameterised PBR with weathering, layered, LOD; what Filament does | `research/light-materials-life.md` |
| lighting by the clock | sun, moon, stars by astronomy; exposure in physical units; night from OSM lamps | `research/light-materials-life.md` |
| seasons and place | phenology, snow, wetness from climatology and live weather; signs, driving side, building styles by region | `research/light-materials-life.md` |
| minds | NPC driving (CARLA traffic manager, SUMO, IDM), walking (Recast/Detour, ORCA), flying (JSBSim), LLM-driven agents with replayable event logs | `research/light-materials-life.md` |
| terrain | the lattice's level boundaries, skirts, and a DEM's error band as a first-class number; stitching real DEM levels | Cesium quantized-mesh, CDLOD |
| determinism | a proof that two runs of one declaration give one byte stream: where threads join, in what order; a fuzz over scheduling | RAGE replay, Unreal automation screenshots |
| physics | vehicles on the analytic driving surface: Jolt's wheeled vehicle, contact with the profile, sub-centimetre agreement between the map and the structure | Jolt `VehicleConstraint` |
| streaming | what a tile costs to fetch, bake and place at each rung; the cache budget that holds Shibuya under 512 MB (board:2104) | Cesium's SSE scheduler |
| audio | engine notes from the machine, ambience by place and hour, occlusion | Steam Audio, Resonance |
| save and replay | scenario + snapshot + event log as one schema (board:2151); a save restored renders the next frame bit-identical | Unreal `DemoNetDriver` checkpoints |

