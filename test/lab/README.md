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
| `roads/CASES.md` + `roads/synthetic.py` | every case of infrastructure x terrain the planet has, in isolation: the bed builds terrain (a height function sampled at the DEM's posting and read bilinearly) x network (OSM-tagged ways), snaps the nodes as the weave does, solves the map, builds the junction surfaces, checks I1-I6 and the finiteness of every height, and writes a picture per case | the map's profile as in band.py, plus: a JUNCTION is the through way's surface extended over netconvert's node polygon and every minor leg warped into it over 20 m [SET, RAS-K]; a DECK is stiff (x10) and holds its own grade (4 % [SET, RAA/RAL] or its abutments' chord), constrained to clear the water or the road below by 4.5 m [SET]; a RAMP is a designed structure -- no grade fidelity to the terrain, the class's grade relative to it [SET, RAL/RASt], and its fill TAPERS monotonically to grade without changing sign (two passes: the first reads the sign, the second holds it) | measured 2026-09-05: 33 cases green -- C0 0 m, C1 1e-8, the DEM band held where the DEM is the authority, junction steps 0.000 m at T, X, roundabout, ramp and dual carriageway on flat, 15 % along, 15 % across and a crest (the control: the same bed before the warp read 0.08-2.20 m, which is what Heidelberg-e556bec1 showed the owner); a deck clears water by 20 m and a road below by 4.6-7.5 m; a tunnel's cover never negative; the six pathologies (duplicate nodes, zero length, a 0.5 m gap, a duplicate way, bridge=yes with no landing, 400 nodes 10 cm apart) all finite with grades under 0.2 | nothing yet: the bed grows first (tile seams, the welded mesh, map against structure, the ground's cut and fill, buildings), then `Path::Network` and the structure stage (board:2101) |

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

