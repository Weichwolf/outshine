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
