Type: task
Parent: 1538
Area: render
Tags: scope

**The subject unit splits along its seams, and the plan's table drives the dispatch**

Review round 1, class-design core: `SubjectDraw` is six responsibilities in one type (1654
lines) -- shader text, shader compilation, a 160-pipeline permutation table, buffer staging and
texture upload, BVH construction, and encode. Unreal splits these into shader source / vertex
factory / mesh draw commands / scene proxy; RAGE into grmShader / grmGeometry / drawlist.

And the renderer's dispatch is three hand-written switches (`Renderer.cpp` Executable /
Configure / EncodeStage) that must each be edited per stage -- the exact drift that left six
declared stages silently unexecuted until board:1549 made the refusal loud. The catalogue's
point is that the TABLE drives everything.

- [ ] shader text moves to its own headers (no macro token-pasting; debuggable source)
- [ ] residency (buffers, BVH, textures) separates from encode
- [ ] a stage registry keyed by `StageRow` replaces the three switches
- [ ] one member-naming convention in `Renderer.h` (trailing underscore or none)
- [ ] `FetchStars` leaves `TerrainLoader.h` -- stars are not terrain

---

**Closed, all five boxes.** (1) The shader text stands in `SubjectShader.h` as named constants.
(2) Residency separates: `SubjectResidency` owns the nine stream buffers, the staging ring and
its crossings, the BVH buffers, the stream ledger and the texture upload chain; `SubjectDraw`
keeps pipelines, surface slots, batching and encode; `SubjectTypes.h` carries the shared
declaration types. (3) The `kExecutors` table replaces the three switches -- a stage without a
row refuses by name. (4) One member convention in the renderer. (5) `FetchStars` deleted --
nothing called it. Proving tests: `render/outshine/shader` 42/42, `render/outshine/scenario`
6/6 on a quiet device (frame budget p99 at 42.5% of 16.67 ms), library entire at 140 objects.
What remains red on the map is 1538's instancing/cull hole and 1574's glass clone -- their own
items.
