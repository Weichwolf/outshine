Type: feature
Area: world
Tags: perf, instrument

**0.2 The request and its priority**

- [x] One scheduler, one queue, one cache, one in-flight cap for fetch, decode, mesh and DAG (`world/TilePool.h`)
- [x] A declared class order over the queue ahead of distance (`world/TilePool.h:118`, DAG < fetch < mesh)
- [x] Distance measured from a camera the scheduler is told rather than one it reads (`world/TilePool.cpp:196`, `TilePool::Camera`)
- [x] In-flight cap equal to the transport's own concurrency (`world/TilePool.h:112`, `InFlightCap() == Threads_.size()`). The **count is 4, not 6** — `world tilepool threads=4 inFlightCap=4` in 399 of the 420 run logs of 2026-08-11, the rest 1–3 — so the cap is below the six connections per origin the browser allows, and the pool is CPU-bound in the mesh build (`meshCpuMsPerTile` 237.29, 16.9 tiles/s over 4 threads) rather than transport-bound (`httpMsPerGet` 4.45). The invariant is met; the number chosen for it is not the one this line claimed
- [x] The picture's work list ranked by distance and view direction (`world/World.cpp:172-183`)
- [ ] Two admission caps, not one: how many may be in progress and how many may **start** per update — CryEngine holds 32 and 4 (`e_StreamCgfMaxTasksInProgress`, `e_StreamCgfMaxNewTasksPerUpdate`); we have one cap, `World::kMeshBuildsPerPass = 2` (`world/World.h`), and it bounds *installs* rather than *starts*: `World::AdmitMesh` spends it only in the `Ready` arm, so a pass the pool cannot answer asks every candidate (`meshCapped` 217 against `meshWanted` 2 029 402 over `demo/crossing`, 0.011 %)
- [ ] The in-cone boost additive and bounded, so a turn cannot invert the whole order — the reference adds a capped 0.5 to a 10-point importance and documents 1.0 as producing thrash; ours multiplies by 20 (`world/World.cpp:180`, 1.0 against 0.05)
- [ ] Priority recomputed on a declared time slice, not every frame — the reference caps the whole update at 0.4 ms (`e_StreamPredictionUpdateTimeSlice`) and re-sorts at most every 100 ms
- [ ] Priority recomputed only after the camera has moved more than a declared distance (`e_StreamCgfGridUpdateDistance`), so a stationary observer costs nothing
- [ ] Prefetch from camera motion: priority evaluated at the camera advanced by its own velocity over a declared horizon — the reference's is 0.5 s (`e_StreamPredictionAhead`) plus a near/far distance pair. Nothing in `world/` or `clients/Sim.cpp` reads a velocity today
- [ ] The prefetch horizon scales with measured headroom and the residency radius does not — converge sooner on a bigger machine, converge to the same frame
- [ ] A request cancellable: a tile that left the target cut before its bytes returned is dropped rather than decoded and meshed
- [ ] Why the browser pool saturates at three useful threads while native reaches 16.4 tiles/s — TOOL. A property of this design, not of the platform, and unmeasured
