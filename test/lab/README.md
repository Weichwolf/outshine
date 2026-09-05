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
| `roads/CASES.md` + `roads/synthetic.py` | every case of infrastructure x terrain the planet has, in isolation, as MAP and as MESH: the bed builds terrain x network, snaps and dedupes as the weave does, solves the map, meshes the legs and the junction regions, and checks I1-I9 | the map as in band.py plus the junction and ramp rules; the MESH: a ribbon per drawn span with a vertex at every break of the cross-section (the crown) and TWO rows at every polyline vertex (one per segment, the wedge between them closing the corner), the station step from the chord's sagitta per segment (L = sqrt(8 tol / k)), the junction REGION (its polygon plus every minor leg's warp band) meshed on a grid whose cell follows from the crossfall and the tolerance, all welded on a quantised key | measured 2026-09-05: 51 of 51 green -- the drawn surface stands within 1e-14 m of the analytic one on flat ground, 0.16 mm over a crest, 6.9 mm over a DEM cliff, 5 mm on noise; no edge carries more than two faces; the roundabout is one junction (netconvert's node joining, the ring whole, its surface an annulus). Four defects the mesh check found and named: a forward tangent collapsed the section at a way's end (3.1 cm), a mitred row spans two directions and no subdivision shrinks its twist (4 cm = grade x offset x tan(theta/2)), one curvature for a whole way took the step from its flattest stretch, and a way drawn through a junction put two surfaces in one place | nothing yet: joining, then the tile seam and the ground's cut and fill, then `Path::Network` and the structure stage (board:2101) |

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

