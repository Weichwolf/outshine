# The world's look: buildings, ground cover, water, clouds -- what is proven, what is readable, what fits the target

Research report, 2026-09-05 (rev. 2: compute-driven geometry re-examined). Target: A18 Pro (PS4
class), 720p60 = 16.7 ms/frame, 921 600 px, SDL_GPU only. Every number carries its origin:
**measured** (by the cited body, on the named platform), **derived** (shown), **[SET]** (a
proposal of this report), **unverified** (repeated from a secondary source; the primary was not
read). Board items this feeds: 2137 (grass, clouds, fire), 2138 (a building reads its street),
2140 (clouds in the atmosphere), 2145 (water lid), 2111 (forest), 2122 (pieces), 2123 (LOD),
2124 (no rebuild inside a frame), 2152 (SPIR-V).

**The correction this revision carries.** Revision 1 said "no tessellation" and refused or
deferred several techniques on that ground. That was wrong about the API: SDL_GPU has no
tessellation STAGE, but a compute pass that writes a vertex/index/instance buffer and an
indirect-argument buffer, followed by `SDL_DrawGPUIndexedPrimitivesIndirect`, IS tessellation
on this API -- Ghost of Tsushima's grass is exactly that on a PS4 that had a tessellator and
did not use it. And the fragment stage tessellates in its own way (parallax occlusion,
interior mapping, relief, decals). What is unreachable is exactly three things: mesh shaders,
the hardware tessellation/geometry stages, and ray-tracing units. Section 0 lists what the API
offers, verified against `SDL_gpu.h` and the SDL3 wiki, and every verdict below is re-cut on it.

Conventions: CPU GEOMETRY = vertices a mesher emits on a worker into the piece residency
(board:2122), built once per rung change. COMPUTE GEOMETRY = vertices/instances a compute
dispatch writes on the device, per frame or per rung change, drawn indirectly. GPU DETAIL =
per-pixel work in the material (normal/parallax/decal/procedural/interior mapping). Lab =
`test/lab/<dir>/<exp>.py` per `test/lab/README.md`, inputs the engine's own (OSM via the same
fetch, terrarium z14, the weather provider's `Scenario::Weather`: `CloudCover`,
`CloudLow/Mid/High`, `CloudBaseAglM`, `WindDeg`, `WindMs`).

What the tree holds today (grep 2026-09-05): `src/generators/building/` (`BuildingShape.h`:
`RoofKind {Flat, Gable, Hip, Shed, Mansard, Sawtooth, Dome}`, `BuildingUse {Outbuilding, House,
Terrace, Block, Hall, Tower, Spire}`; `FacadeUv.h`; `RoofSurface`; `StructureBake`),
`src/generators/flora/` (a tree grower, no grass), `src/generators/water/` (`Water.h`,
`WaterDepth.h`; a lid and a carved bed -- board:2145), `src/render/stages/Medium*` +
`AerialPerspectiveStage` (Hillaire 2020's LUTs), **`SubjectCullStage` (three dispatches: cull
-> scan -> compact, frustum + Hi-Z occlusion from `DepthPyramidStage`, Unreal's one-texel
cluster-error LOD test, ONE `SDL_DrawGPUIndexedPrimitivesIndirect` for every subject piece)**,
`GroundLattice` (33x33 lattice pages, 512 resident, skirts of 16 steps). Shaders are `.msl`
today; board:2152 moves them to GLSL -> SPIR-V. No cloud, no scatter, no particle, no
compute-expanded ribbon. `src/world/weather/WeatherProvider.h` carries the cloud layers.

---

## 0. Compute-driven geometry on SDL_GPU

### 0a. What the API offers (verified: `include/SDL3/SDL_gpu.h` main, wiki `CategoryGPU`, `SDL_CreateGPUShader`, `SDL_CreateGPUComputePipeline`, `SDL_BeginGPUComputePass`, `SDL_DrawGPUIndexedPrimitivesIndirect`, fetched 2026-09-05)

| primitive | SDL3 names | facts |
|---|---|---|
| compute pipeline | `SDL_CreateGPUComputePipeline(SDL_GPUComputePipelineCreateInfo{num_samplers, num_readonly_storage_textures, num_readonly_storage_buffers, num_readwrite_storage_textures, num_readwrite_storage_buffers, num_uniform_buffers, threadcount_x/y/z})` | workgroup size is fixed at pipeline creation; resource COUNTS are fixed at creation (no bindless) |
| compute pass | `SDL_BeginGPUComputePass(cmd, storage_texture_bindings, n, storage_buffer_bindings, n)` · `SDL_BindGPUComputePipeline` · `SDL_BindGPUComputeSamplers` · `SDL_BindGPUComputeStorageTextures` · `SDL_BindGPUComputeStorageBuffers` · `SDL_PushGPUComputeUniformData` · `SDL_DispatchGPUCompute(x,y,z)` · `SDL_DispatchGPUComputeIndirect(buffer, offset)` · `SDL_EndGPUComputePass` | read-write resources are named at `Begin`; "reads and writes in compute passes are NOT implicitly synchronized" -- a dispatch that reads another's output needs a NEW pass; a texture read and written in one pass needs `COMPUTE_STORAGE_SIMULTANEOUS_READ_WRITE` and "only specific texture formats" support it |
| storage buffers | usage `SDL_GPU_BUFFERUSAGE_{VERTEX, INDEX, INDIRECT, GRAPHICS_STORAGE_READ, COMPUTE_STORAGE_READ, COMPUTE_STORAGE_WRITE}` (OR-able: one buffer may be `INDEX \| COMPUTE_STORAGE_WRITE`, as `SubjectDraw.cpp:1071` does) | a buffer written by compute is drawn from directly; graphics stages get storage READ only (`SDL_BindGPUVertexStorageBuffers`, `SDL_BindGPUFragmentStorageBuffers`) -- no vertex/fragment-stage writes, so all GPU geometry generation is in compute |
| storage textures | usage `SDL_GPU_TEXTUREUSAGE_{SAMPLER, COLOR_TARGET, DEPTH_STENCIL_TARGET, GRAPHICS_STORAGE_READ, COMPUTE_STORAGE_READ, COMPUTE_STORAGE_WRITE, COMPUTE_STORAGE_SIMULTANEOUS_READ_WRITE}`; `SDL_BindGPUVertexStorageTextures`, `SDL_BindGPUFragmentStorageTextures` | compute writes textures (noise volumes, flow maps, Hi-Z, grass tiles); vertex stage can READ a storage texture (a height field in VS without a sampler) |
| indirect draw | `SDL_DrawGPUPrimitivesIndirect(pass, buffer, offset, draw_count)` · `SDL_DrawGPUIndexedPrimitivesIndirect(pass, buffer, offset, draw_count)`; commands tightly packed `SDL_GPUIndirectDrawCommand{num_vertices, num_instances, first_vertex, first_instance}` / `SDL_GPUIndexedIndirectDrawCommand{num_indices, num_instances, first_index, vertex_offset, first_instance}` (5 x u32, asserted at `SubjectDraw.cpp:1219`) | `draw_count` is a CPU-side number: there is NO indirect COUNT buffer, so a compute-decided draw list is drawn with a fixed maximum and the rejected slots zeroed (`num_instances = 0`), which is what `SubjectCullStage`'s compact does |
| indirect dispatch | `SDL_GPUIndirectDispatchCommand{groupcount_x,y,z}` | a dispatch sized by a previous dispatch's output (blade count -> draw args -> a second pass) |
| uniforms | `SDL_PushGPU{Vertex,Fragment,Compute}UniformData(cmd, slot, data, size)`; 4 slots per stage | small per-draw constants; no descriptor indexing |
| binding model (SPIR-V) | vertex: set 0 = samplers, storage textures, storage buffers (in that order, consecutive `binding`s from 0), set 1 = uniforms; fragment: sets 2/3; compute: set 0 = samplers + read-only storage, set 1 = read-write storage, set 2 = uniforms | "no gaps in the set" -- shaders declare a FIXED table; the tree's `subjectBindings.msl` is that table |
| shader stages | `SDL_GPUShaderStage {VERTEX, FRAGMENT}` only; formats `SPIRV, DXBC, DXIL, MSL, METALLIB, PRIVATE` | **no tessellation control/evaluation, no geometry, no mesh/task stage**; the wiki names ray tracing and mesh shaders as "bleeding-edge ... not planned for the near future" |
| copy / readback | `SDL_BeginGPUCopyPass`, `SDL_UploadToGPUBuffer`, `SDL_DownloadFromGPUBuffer` via a transfer buffer | a one-frame-late readback (57 KB of RVT feedback, a blade count) is cheap and asynchronous |

### 0b. What is NOT there (and what to do about it)

| absent | consequence | the substitute |
|---|---|---|
| tessellation / geometry / mesh stages | no on-chip amplification between VS and raster | compute writes the amplified vertices to a storage buffer ONCE per rung change, or per frame where they animate (grass); memory traffic replaces on-chip bandwidth -- the bound is bytes/frame, tabled in 0c |
| indirect count buffer | `draw_count` is fixed on the CPU | draw a fixed-capacity list with zeroed slots (exists); or ONE indexed indirect draw whose index buffer compute compacted (exists) |
| bindless / descriptor indexing | one material = one fixed binding table | texture ARRAYS (`SDL_GPU_TEXTURETYPE_2D_ARRAY`) indexed by a per-instance integer: the ground's 8 layers, the facade styles, the decal atlas are each one array |
| subgroup / wave intrinsics | not exposed as a property; SPIR-V `GroupNonUniform` capabilities compile through SDL_shadercross to MSL `simd_*` and DXIL waves, but nothing GUARANTEES it | **assume absent**: scans and reductions in shared memory (`SubjectCullStage`'s scan already does this) |
| atomics | the header and wiki say nothing (unverified either way). Buffer atomics (`OpAtomicIAdd` on a storage buffer) are plain SPIR-V and every backend has them; texture atomics are format- and backend-dependent (Metal: r32 only, Apple family dependent) | buffer atomics for counters (blade count, visible-node count) -- but check with a CASE on this device; NEVER texture atomics; a histogram is a per-workgroup tally + a second pass |
| implicit sync inside a pass | a dependent dispatch is a data race | one pass per dependency edge: the cost is a barrier, not a copy |
| a vertex-stage storage WRITE | no transform feedback | irrelevant once compute writes the buffer |

### 0c. Per technique: dispatches and bytes per frame ([SET] unless stated; bandwidth bound derived at PS4's 176 GB/s = 176 MB per ms)

| technique | when | dispatches | bytes written | bytes read by the draw | ms bound (bandwidth) |
|---|---|---|---|---|---|
| GPU cull + LOD select (EXISTS) | per frame | 3 (cull, scan, compact) + 1 indirect draw | 20 B x draws; compacted index list | index list | ~0.05 |
| grass blades | per frame (wind, camera) | 1 per visible tile ring (~16) or 1 over all; + 1 indirect draw per LOD | 300 k x 32 B = 9.6 MB instance data + args | 9.6 MB in VS (no vertex stream; index -> t along the Bezier) | 0.11 write + 0.05 read |
| road ribbon subdivision (centreline + profile -> lanes, kerbs, marks UV) | per RUNG change of a tile, not per frame | 1 per changed tile | at 0.5 m over 3 340 ways (OldTown, `roads/profile.py`) ~ 200 km -> 400 k sections x 8 verts x 32 B = 102 MB resident; per changed tile ~1/64 of it = 1.6 MB | only visible tiles' vertices | amortised < 0.05 |
| terrain lattice node select + skirts (CDLOD in compute) | per frame | 1 select (512 pages -> instance list, morph factor) + 1 indirect instanced draw of one 33x33 patch | 512 x 16 B = 8 KiB | height from a storage texture in VS (page 35 x 35 x 2 B = 2.4 KiB per page) | < 0.02 |
| water CDLOD grid | per frame | 1 select (<= 4 096 nodes) + 1 indirect instanced draw of a 32x32 patch; displacement from the FFT cascades in VS | 64 KiB | 3 cascades x 256^2 x 8 B = 1.5 MB | < 0.02 |
| FFT ocean (3 x 256^2) | per frame | spectrum 3 + Stockham 2 x 8 x 3 = 48 + Jacobian 3 = 54; one pass per dependency = 18 passes | ping-pong 3 x 65 536 x 8 B x 16 passes = 25 MB traffic | -- | 0.14 |
| polyline ribbons (fence, rail, wire, railing) | per rung change | 1 per changed tile | 20 k visible segments x 4 verts x 16 B = 1.3 MB resident | visible part | amortised |
| facade LOD0 detail (cornice, entrance, balcony strips from the parameter buffer) | per rung change (near ring) | 1 | ~50 near buildings x 200 verts x 32 B = 320 KB | same | ~0 |
| clouds | per frame | 1 march (320x180) + 1 upsample + 1 shadow map | 4 x 460 KiB history | -- | ~0.02 (compute-bound instead: 4e) |
| RVT feedback | per frame | fragment writes page ids at 1/8 res; 1 compute tally (or a 57 KB readback one frame late); N page render passes into the atlas | 160 x 90 x 4 B = 57 KB | atlas 4 096^2 BC7 = 16 MiB resident | ~0 for feedback; page renders are the real cost (2d) |

The frame-path rule (CLAUDE.md: nothing on the frame path allocates or blocks; board:2124: a
rebuild never happens inside a frame) reads, for compute geometry: **a per-frame dispatch
writes into a buffer allocated at preload with a fixed capacity, and a per-rung-change
dispatch is the streaming scheduler's work (board:2132), bounded per frame the way uploads
are.** The capacity is a declared ceiling, and a count that exceeds it is a loud refusal.

### 0d. What moves from CPU geometry to compute geometry -- the revised split

| element | rev. 1 verdict | rev. 2 verdict | why |
|---|---|---|---|
| grass, reeds, crops | compute (unchanged) | compute per frame | animated, camera-dependent density |
| road lane ribbon, kerbs, sidewalk edge | CPU mesher | **compute per rung change** from the CPU's centreline + grade + profile (the solver of `roads/band.py` stays CPU) | the expansion is linear and embarrassingly parallel; the CPU keeps the GRAPH (board:2133) and the profile, the device keeps the triangles; 102 MB of road vertices never cross the bus |
| terrain lattice | CPU-selected pages, VS-displaced | **compute node select + morph** (CDLOD), skirts as before | the select is O(pages) per frame, on the device with the Hi-Z it already has |
| water surface | CPU plane / grid | **compute CDLOD + VS displacement from cascades** | same as terrain |
| fence, rail, wire, railing | CPU strips | **compute ribbons** from polylines (camera-facing for wires) | camera-facing needs per-frame orientation anyway |
| building mass + roof | CPU | **CPU** (unchanged) | a straight skeleton is sequential and irregular; built once, and one building is ~100 tris |
| facade windows, sills, interiors | fragment | **fragment** (unchanged) | zero vertices is still the cheapest tessellation |
| cornice, entrance, balconies at LOD0 | CPU at LOD0 | **compute per rung change** from the facade parameter buffer | it is a split-grammar expansion over the near ring only |
| bridge piers, pylons, lamps, stones | CPU instanced pieces | CPU pieces, **compute placement** (HZD) where density rules apply | a placement is a scatter, and scatter is compute |
| placer density textures, flow maps, weather map | CPU | CPU on a worker OR compute at preload; either, deterministic | not on the frame path either way |
| clouds | compute | compute | -- |

Board:2124 is honoured, not bent: a per-rung-change dispatch is a rebuild the STREAMER
schedules against its byte budget (board:2132), and the frame only ever draws what a previous
frame's dispatch finished.

### 0e. Lab experiments for compute geometry

| id | question | inputs | solution | PROOF | negative control | to C++ |
|---|---|---|---|---|---|---|
| 0.1 `gpu/road_ribbon.py` | is a compute expansion of the road ribbon bit-identical to the CPU mesher's? | OSM ways at OldTown, `roads/band.py` profile | numpy: the expansion as a pure function of (node, next node, profile) with no neighbour state -- the form a kernel needs; compare to `RoadMesh` output through the door | every vertex equal to 1e-6 m; same triangle count; no dependence on segment ORDER (permute the ways: same bytes) | make the kernel read its neighbour's output: order-dependent -> differs | the kernel's per-section function |
| 0.2 `gpu/cdlod.py` | does compute node selection + morph keep the terrain C0 across LOD boundaries at every camera position? | terrarium z14, a camera path | CDLOD (Strugar 2009): select by distance, morph the odd vertices toward the parent; check across 10 000 camera positions | max gap between neighbouring nodes of different rungs < 1e-6 m; the skirt never shows (no pixel of skirt colour in a Blender render at 720p) | morph off: gaps of a full height step | the select rule, the morph constant |
| 0.3 `gpu/cull_oracle.py` | does the GPU cull (exists) accept exactly the set a CPU cull accepts? | the piece spheres and the Hi-Z of one frame read back through the door | numpy frustum + Hi-Z test | identical accept sets; every rejected piece is invisible in the reference picture (no pixel of it) | disable the parent-error half of the LOD test: cracks between rungs appear (pixel count > 0) | none -- an oracle for what exists |
| 0.4 `gpu/ribbon_wire.py` | how many segments and what width does a camera-facing wire need under 1 px error? | `power=line` spans; camera at 1.7 m | catenary `y = a cosh(x/a)` sampled, projected | chord error < 0.5 px; width clamped to >= 1 px so a wire never vanishes | 4 segments: > 1 px | segment count as f(span, distance) |
| 0.5 `gpu/capacity.py` | what fixed capacities do the per-frame buffers need at every reference place? | every `make shots` place | count blades, nodes, ribbon segments per place at the worst camera | the maximum over places x 1.5 [SET] fits the declared buffers; a place that exceeds is a red case, not a resize | halve the capacity: Jura goes red | the constexpr ceilings |

---

## 1. Buildings from OSM

### 1a. Who has done it and proven it

| body | what it does with a footprint | proof |
|---|---|---|
| **CARLA Digital Twin** (0.9.15, 2023) | OSM -> `OSM2ODR` (SUMO netconvert) -> `.xodr`; roads from `MeshFactory` (`vertex_distance=0.5`, `vertex_width_resolution=8`, `simplification_percentage=50`, 15 for lane marks -- `OpenDriveToMap.cpp`); buildings: footprint + height from OSM, styled by dimension (tallest -> office; smaller -> commercial/residential by footprint area), facade "cladding" of windows/doors/balconies from a library of fascia MESH pieces | shipped tool; docs say "experimental, not production ready", 2x2 km in ~10 min |
| **CARLA Procedural Building tool** (`BP_Procedural_Building`) | rectangular building = levels x array of facade meshes repeated at random along each level, corner meshes, door meshes on the ground floor, "walls" (a plane replacing a side). LOD = an IMPOSTOR: `ProceduralBuildingUtilities.cpp` captures 4 orthographic views to an atlas, `GenerateImpostorGeometry()` makes 4 quads; `CookProceduralBuildingToMesh()` merges the pieces | shipped |
| **CARLA's Houdini path** | `UW_HoudiniBuildingImporter` blueprint + `HoudiniImporterWidget.cpp` (only sub-level/tiling logic in C++; the geometry comes from a Houdini Digital Asset) | NOT readable: HDA is binary, Houdini Engine is commercial; users report it broken (issue #7262) |
| **OSM2World** (Java, LGPL-2.1) | the most complete open OSM->3D: 26 roof classes, windows as geometry or texture, doors, wall surfaces, indoor, street furniture, power lines | runs on the whole planet at osm2world.org; the de-facto reference for tag semantics |
| **blosm / blender-osm** (prochitecture; GPL source for customers, CC0 textures) | extrusion, roof:shape incl. hipped via straight skeleton (`bpypolyskel`, GPL-3), tileable facade textures with UV per level, lit windows for evening | commercial add-on, widely used for archviz |
| **Kendzi3D** (JOSM plugin, BSD-3 per OSM wiki -- LICENSE file 404, unverified) | roof table (`roof:shape` + `roof:orientation`), `BuildingParser.java`; discontinued | the JOSM editor's 3D preview |
| **OSMBuildings** (JS, BSD-2 main, mixed) | extrusion + dome/pyramid/skillion/gabled roofs, colour from tags | serves 3D tiles worldwide |
| **Esri CityEngine** (CGA, commercial, not readable) | the industrial form of Müller 2006: split grammar on the extrusion (`split(y)` floors, `split(x)` tiles, `comp(f)` faces, `repeat`) | the product behind most film/VFX city fill |
| **Müller, Wonka, Haegler, Ulmer, Van Gool 2006** | CGA shape: mass model + split grammar for facades; the Pompeii result | SIGGRAPH 2006, ToG 25(3):614-623 |
| **Insomniac, Marvel's Spider-Man (2018)** | interior mapping (van Dongen 2008) behind every window: rooms are a per-pixel ray cast into a cubemap, ZERO geometry | shipped PS4 1080p30 (secondary sources; unverified) |

### 1b. Readable repositories

| repo | licence | read this |
|---|---|---|
| `github.com/carla-simulator/carla` | MIT (engine); UE side under UE EULA | `Unreal/CarlaUE4/Plugins/CarlaTools/Source/CarlaTools/Private/OpenDriveToMap.cpp` (`GenerateRoadMesh`, `CreateTerrainMesh` with `GridSectionSize=256`, `GetHeightForLandscape`, `GenerateTreePositions`), `ProceduralBuildingUtilities.cpp` (impostor bake), `MapGeneratorWidget.cpp`, `ProceduralWaterManager.cpp`; `LibCarla/source/carla/road/MeshFactory.cpp` |
| `github.com/tordanik/OSM2World` | LGPL-2.1 | `core/src/main/java/org/osm2world/world/modules/building/`: `BuildingModule.java`, `BuildingPart.java`, `LevelAndHeightData.java` (the height/levels/roof-height resolution rules -- the ONE place the tag arithmetic lives), `ExteriorBuildingWall.java`, `WallSurface.java`, `GeometryWindow.java`, `TexturedWindow.java`, `Door.java`; `roof/`: `Roof.java`, `RoofWithRidge.java`, `GabledRoof`, `HippedRoof`, `HalfHippedRoof`, `SkillionRoof`, `MansardRoof`, `GambrelRoof`, `SaltboxRoof`, `SawtoothRoof`, `PyramidalRoof`, `DomeRoof`, `OnionRoof`, `RoundRoof`, `ComplexRoof` (ridge/edge ways from tags), `HeightfieldRoof`. Also `StreetFurnitureModule`, `PowerModule`, `BarrierModule`, `BridgeModule`, `RailwayModule` |
| `github.com/vvoovv/blosm` (branch `release`) | GPL (customers) | `building/` roof generators, `renderer/` facade UV per level |
| `github.com/prochitecture/bpypolyskel` | GPL-3 | straight skeleton -> hipped roof faces with heights; port of `Botffy/polyskel` (MIT) |
| `github.com/kendzi/kendzi3d` | BSD-3 (unverified) | `kendzi3d-buildings-josm/.../building/parser/BuildingParser.java`, roof table |
| `github.com/OSMBuildings/OSMBuildings` | BSD-2 (+MIT, +GPL-3 fragments) | `src/` roof shapes, colour-from-tags |
| van Dongen, "Interior Mapping" | paper, CGI 2008 | `proun-game.com/Oogst3D/CODING/InteriorMapping/InteriorMapping.pdf` |

Not readable: CityEngine, Houdini HDAs, Insomniac's shader, RAGE's building tooling, Nanite.
Unreal's source is readable under EULA but is not open -- TECHNIQUE, not structure.

### 1c. Papers

| title | authors | year | venue | url |
|---|---|---|---|---|
| Procedural Modeling of Buildings | Müller, Wonka, Haegler, Ulmer, Van Gool | 2006 | SIGGRAPH / ToG 25(3) | dl.acm.org/doi/10.1145/1141911.1141931 |
| Interior Mapping: a new technique for rendering realistic buildings | van Dongen | 2008 | CGI 2008 | proun-game.com/Oogst3D/CODING/InteriorMapping/InteriorMapping.pdf |
| Simple 3D Buildings (tag schema) | OSM community | 2012- | OSM wiki | wiki.openstreetmap.org/wiki/Simple_3D_buildings |
| Straight skeleton | Felkel, Obdržálek | 1998 | SCCG | (cited via polyskel; not fetched) |
| GHS-OBAT: open building attribute table | Florio et al. (JRC) | 2025 | Data in Brief | sciencedirect.com/science/article/pii/S2352340925004780 |
| GlobalBuildingAtlas: polygons, heights, LoD1 | TUM | 2025 | arXiv 2506.04106 | arxiv.org/html/2506.04106v1 |

### 1d. What the data gives (measured, taginfo 2026-09-05)

| key | occurrences | share of `building=*` (706 987 687) |
|---|---|---|
| `building:levels` | 42 571 979 | 6.0 % |
| `height` | 27 029 147 (all objects) | <= 3.8 % |
| `roof:shape` | 9 652 298 | 1.4 % |
| `roof:shape` top values | gabled 56.3 %, flat 21.0 %, hipped 10.1 %, pyramidal 3.1 %, skillion 2.9 %, half-hipped 1.4 % (= 94.7 %) | |

Derived: **94 % of footprints carry no height and 98.6 % no roof shape.** Height and roof are
a GENERATOR's answer (CLAUDE.md: an untagged height cannot be checked against a truth outside
the tree), seeded by `building=*`, footprint area, the street it fronts (board:2138) and the
region's statistics. External height oracles where a scenario wants them: GHS-OBAT (per
footprint, 2.3 G footprints, epoch/height/function), GHS-BUILT-H (100 m grid, 2018), Overture
buildings (`height`, `num_floors`, `roof_shape` where sources had them), EUBUCCO (Europe).
All are PROVIDERS -- a second source behind the same question.

### 1e. Technique by technique: CPU, compute or fragment

| element | CARLA | OSM2World / blosm | proposal for outshine | tier |
|---|---|---|---|---|
| mass (walls) | extrusion | extrusion, `min_height`, `building:part` | extrusion (exists) | CPU, ~2 tris/edge, once |
| roof | flat / library piece | 26 shapes, straight skeleton for hipped | `RoofKind` exists; add straight-skeleton hip for non-rectangular footprints (lab 1.2) | CPU, once |
| floors / window grid | facade MESH pieces per level | textured quads per window or geometry windows | one facade material with `{levels, bays, style, colour}` per instance; cells from `floor(uv * bays)`; sills/lintels as normal + parallax | fragment, 0 vertices |
| window interiors | none | none | interior mapping: per-pixel ray vs room box into a small generated cubemap array; lit windows at night from the cell hash and the clock | fragment |
| balconies, cornices, entrances | mesh pieces | geometry | **compute** per rung change over the near ring: the split grammar's terminal shapes expanded from the parameter buffer into strips; parallax beyond the near ring | compute near / fragment far (rev. 1 said CPU near) |
| facade colour, material | per style | tags | tag where present, else a style hash by region + `BuildingUse`; styles as one texture ARRAY (0b: no bindless) | fragment parameter |
| LOD | 4-view impostor atlas | none | box + roof at the far rung; the facade material collapses on its own (a window below one pixel is its mip's mean) | CPU: one mesh; fragment: mips |
| ground contact | raycast to landscape | none | footprint stamped flat (board:2121, exists) | CPU |

**The position** (unchanged): CARLA's facade-by-mesh-pieces is the wrong split. 200 buildings
x 6 levels x 30 bays x 2 pieces x ~50 tris = 3.6 M triangles for what a 4-parameter material
draws for free; CARLA needs impostors to survive it. Müller's grammar is the right MODEL, run
per PIXEL for the repeating terminals and in COMPUTE for the few terminals that change the
silhouette near the camera. Geometry from the CPU only where it is built once: mass, roof.

### 1f. The other OSM structures

| structure | OSM source | reference | CPU | compute | fragment |
|---|---|---|---|---|---|
| roads | `highway=*` | CARLA `MeshFactory` (0.5 m samples, marks as geometry), Far Cry 4/5 (marks as decals in the AVT) | graph + grade profile (exists) | **lane ribbon, kerb, sidewalk edge expanded per rung change** (0.1); the CDLOD morph of the ribbon's along-way subdivision with distance | marks, cracks, patches as decals from a decal ARRAY indexed by `dist to centreline` (the ribbon's UV) |
| bridges | `bridge=yes`, `layer`, `man_made=bridge` | OSM2World `BridgeModule` | pier pitch from span (1.4) | deck = the road ribbon lifted; railing ribbon | railing lattice as alpha-tested texture |
| rails | `railway=rail` | OSM2World `RailwayModule` | -- | ballast ribbon + two rail strips | sleepers as normal/parallax |
| walls, fences | `barrier=*`, `height` | OSM2World `BarrierModule` | -- | wall strip; fence posts as instanced pieces at 2.5 m from a compute placement | chain-link/mesh as alpha-tested quad |
| power lines | `power=line`, `power=tower/pole` | OSM2World `PowerModule` (catenary); Far Cry 5 (offline) | pylon = instanced piece | wire = catenary ribbon, camera-facing, width clamped to 1 px (0.4) | none |
| street furniture | `highway=street_lamp`, `amenity=bench`, trees as points | OSM2World `StreetFurnitureModule` | pieces (board:2122); a lamp is a LIGHT (board:2128) | placement | none |

### 1g. Feasible through SDL_GPU

Everything in 1e/1f is compute + raster: per-instance parameters in a storage buffer,
interior mapping and parallax in the fragment stage, decals from a texture array, near-ring
detail from a compute expansion into a fixed-capacity buffer. NOT feasible: Nanite's software
raster path (mesh-shader class) -- but its CULL is here already (`SubjectCullStage`); a
CityEngine grammar re-run per frame (the terminals live in the residency, not in the frame).

### 1h. Lab experiments

| id | question | inputs | solution | PROOF | negative control | to C++ |
|---|---|---|---|---|---|---|
| 1.1 `buildings/heights.py` | how good is a generator's height guess against a truth? | OSM footprints at OldTown/Venice; GHS-OBAT or Overture `height` where tagged | fit `height ~ f(building=*, area, street class, region)` on tagged buildings; predict the untagged | held-out MAE below the DEM's own vertical error; per-class residual unbiased | shuffle the tags: MAE rises to the class variance | the prior table per `BuildingUse` and region |
| 1.2 `buildings/roofs.py` | does a straight skeleton give a valid hip roof for every OSM footprint? | OSM footprints (concave, holes, > 4 edges) | `polyskel` / shapely; faces with heights; compare `RoofKind::Hip` on rectangles | every face planar (< 1e-6 m), roof closed with the wall top, `trimesh.is_watertight`, matches the rectangle formula to 1e-9 | a spiked footprint: skeleton degenerates -> flagged, not flat | the skeleton kernel with its refusal |
| 1.3 `buildings/facade_grid.py` | at what distance does a per-pixel facade equal a meshed one? | levels, bays; Blender | material vs mesh windows at 5/10/30/100 m | image difference under threshold at >= 30 m; silhouette differs only at the roofline | 5 m: over threshold (so the near ring needs 1e's compute terminals) | the near-ring radius, with its origin |
| 1.4 `structures/bridges.py` | where are the piers nobody tagged? | `bridge=yes` ways + DEM | span/pitch from length and `layer`; pier where clearance allows | every pier foot on the DEM, none in `natural=water` unless tagged | clearance off: piers in the river | the pitch rule |
| 1.5 `buildings/facade_terminals.py` | is the compute expansion of cornice/entrance strips a pure function of the parameter row? | 50 near buildings' parameter rows | numpy expansion per row; permute rows | same bytes under permutation; every strip inside the building's box; watertight with the wall (no gap > 1e-6) | let a row read its neighbour: differs | the kernel |

### 1i. Requirements

| requirement | value | origin |
|---|---|---|
| height prior per class | table in the generator, seeded by region | lab 1.1 |
| roof geometry | straight skeleton + `RoofKind`; every roof closed and CCW (board:2148) | lab 1.2 |
| facade material | per-instance `{levels, bays, style, colour}` (16 B); window cells; interior mapping; night lights from the clock; styles in one texture array | [SET] |
| near-ring terminals | a compute expansion into a fixed buffer (320 KB [SET]), rebuilt on rung change | 0c, lab 1.5 |
| building base pass budget | <= 2.0 ms at 720p for OldTown | [SET]; the whole base pass of a PS4 title is ~4-6 ms at 1080p (Courrèges, GTA V) |
| road decals | a decal array; <= 0.5 ms | [SET] |
| memory | parameter buffer 100 k x 16 B = 1.6 MB; room cubemaps 8 x 6 x 64^2 x 4 B = 0.8 MB; road ribbons ~100 MB resident at 0.5 m over a city (0c) -- or 1 m sampling far from the camera, halving it | derived |
| providers | OSM (exists); optional height provider (GHS-OBAT / Overture) | |

---

## 2. Ground cover, performant

### 2a. Who has done it and proven it

| body | technique | numbers |
|---|---|---|
| **Ghost of Tsushima** (Sucker Punch, GDC 2021, Wohllaib) | grass generated ON THE GPU: a compute shader per tile reads terrain height/material/grass-type/clump textures, emits one blade per thread as a cubic Bezier (tilt, bend, facing), instanced indexed draws with NO vertex stream (position from index + instance id), 15 verts/blade near, 7 far, blade folding for double density, view-space thickening edge-on, Voronoi clumps, Perlin wind + per-blade sine, an interaction displacement buffer; shadows as dithered depth impostors + screen-space. **This is compute tessellation on a PS4 that had a hardware tessellator** -- the split this report adopts | "over a million blades ... ~2 ms" -- unverified (secondary summaries); PS4 1080p30 |
| **Horizon Zero Dawn** (Guerrilla, GDC 2017, van Muijden) | GPU run-time procedural PLACEMENT: a node graph of density maps (slope, altitude, biome, noise) evaluated in compute around the player; a dither/sample pattern picks instances | shipped PS4 |
| **Horizon Forbidden West** (SIGGRAPH 2022 Real-Time Live) | custom vegetation system under per-feature budgets | Best in Show; details not published |
| **Far Cry 5** (Ubisoft GDC 2018: Carrier -- world gen; Moore -- terrain; Chen 2015 -- AVT) | OFFLINE Houdini biomes (viability maps, species competition); terrain: **GPU compute LOD/cull/stitch of the height field** (Moore) -- the CDLOD-in-compute this report proposes, shipped; materials by ID with world-space UV cached in an ADAPTIVE VIRTUAL TEXTURE (10 texels/cm over 10x10 km) with decals written into it | shipped PS4 1080p30 |
| **Unreal** | Landscape Grass Type (instances from a weight-map layer, GPU-instanced, no Nanite needed); PCG framework (5.2+, GPU nodes 5.5+); Runtime Virtual Texture; Nanite Foliage (5.7, voxel) -- NOT usable without Nanite | shipped |
| **RDR2** (RAGE) | not readable. Observed (imgeself study): terrain depth pre-pass, POM on terrain from an accumulated footprint/trail displacement map, dense instanced grass | shipped PS4 1080p30 |
| **Heitz & Neyret 2018; Deliot & Heitz 2019; Mikkelsen 2022** | stochastic hex tiling with a histogram-preserving blend -> no repetition | Unity HDRP shipped it |

### 2b. Readable repositories

| repo | licence | read |
|---|---|---|
| `github.com/hanbollar/Vulkan-Grass-Rendering`, `github.com/salaark/Vulkan-Grass-Rendering` | (UPenn course; unverified) | Bezier blades, compute culling (orientation/frustum/distance), indirect draw -- Jahrmann & Wimmer 2017 in Vulkan |
| `github.com/2Retr0/GodotGrass` | MIT (unverified) | Tsushima-style grass in Godot compute |
| `github.com/needle-tools/procedural-stochastic-texturing` | MIT | histogram-preserving tiling + the precomputation |
| `github.com/mmikk/hextile-demo` | MIT | Mikkelsen, JCGT 2022 -- the cheap production form |
| `github.com/AurelienLeandri/VulkanCulling`, `vkguide.dev/docs/gpudriven/compute_culling/` | MIT | compute frustum + occlusion cull -> indirect draw; the tree's `SubjectCullStage` is this |
| Filament `shaders/src/` | Apache-2.0 | `shading_model_standard.fs`, `common_material.fs`; no POM, no triplanar -- write those |

### 2c. Papers and talks

| title | authors | year | venue | url |
|---|---|---|---|---|
| Procedural Grass in 'Ghost of Tsushima' | Wohllaib | 2021 | GDC Advanced Graphics Summit | gdcvault.com/play/1027033 |
| GPU-Based Run-Time Procedural Placement in 'Horizon: Zero Dawn' | van Muijden | 2017 | GDC | gdcvault.com/play/1024700 |
| Procedural World Generation of 'Far Cry 5' | Carrier | 2018 | GDC | gdcvault.com/play/1025557 |
| Terrain Rendering in 'Far Cry 5' | Moore | 2018 | GDC | media.gdcvault.com/gdc2018/presentations/TerrainRenderingFarCry5.pdf |
| Adaptive Virtual Texture Rendering in Far Cry 4 | Chen | 2015 | GDC | media.gdcvault.com/gdc2015/presentations/Chen_Ka_AdaptiveVirtualTexture.pdf |
| Continuous Distance-Dependent Level of Detail for Rendering Heightmaps (CDLOD) | Strugar | 2009 | JGT 14(4) | (not fetched; the method every open ocean/terrain repo above cites) |
| Responsive Real-Time Grass Rendering for General 3D Scenes | Jahrmann, Wimmer | 2017 | I3D | (not fetched) |
| High-Performance By-Example Noise using a Histogram-Preserving Blending Operator | Heitz, Neyret | 2018 | HPG | eheitzresearch.wordpress.com/722-2/ |
| Procedural Stochastic Textures by Tiling and Blending | Deliot, Heitz | 2019 | GPU Zen 2 | eheitzresearch.wordpress.com/738-2/ |
| Practical Real-Time Hex-Tiling | Mikkelsen | 2022 | JCGT 11(3) | jcgt.org/published/0011/03/05/ |
| Dynamic Parallax Occlusion Mapping with Approximate Soft Shadows | Tatarchuk | 2006 | I3D | (not fetched) |

### 2d. Feasible through SDL_GPU (re-cut)

| technique | SDL_GPU | note |
|---|---|---|
| compute-generated blades, indirect instanced draw | yes (0a) | Tsushima's pipeline 1:1 |
| compute placement (HZD) | yes | density maps are textures the placer has; output = instance buffer + indirect args |
| terrain CDLOD in compute (Far Cry 5's Moore) | yes | 1 select dispatch + 1 indirect instanced patch draw; skirts stay (board:2144); rev. 1 called this "tessellation, not available" -- wrong |
| ground material splat by ID + world UV | yes | layers in one texture array (no bindless) |
| stochastic hex tiling | yes | 3 samples per layer |
| detail normal, triplanar | yes | triplanar on slopes only |
| parallax occlusion | yes | 8-16 steps at LOD0 |
| runtime virtual texture | **feasible**: feedback = fragment writes page ids at 1/8 res; a compute tally (or 57 KB readback one frame late, `SDL_DownloadFromGPUBuffer`); pages rendered into a 4 096^2 atlas by ordinary render passes; no atomics needed if the tally is per-workgroup + second pass | rev. 1 deferred it for cost; rev. 2: **still deferred, but by POLICY not capability** -- the engine has no texture policy yet, and a VT is the texture policy. Revisit when decals-on-ground and prop blending are measured as a need |
| Nanite foliage | no | software raster + mesh-shader class |
| hardware tessellation of terrain/grass | no stage | not needed: Tsushima and Far Cry 5 did it in compute |

### 2e. CPU vs compute vs fragment

| cover | CPU (worker, once) | compute | fragment |
|---|---|---|---|
| grass, reeds, crops | 4 R8 tile textures (height, class, density, clump) from class + DEM slope + noise, deterministic | blades per frame: position, Bezier, LOD, wind, cull; indirect draw | blade shading, view-space thickening |
| stones, litter pieces (> 0.3 m) | the pieces (board:2122) | placement + cull (exists) | -- |
| small litter, leaves | -- | -- | a detail layer in the ground material |
| soil, sand, snow, mud, wet | -- | -- | class -> layer; snow by `n.y > cos(35 deg)` [SET] and `Scenario::Weather`; wetness = roughness/darkening |
| tracks, paths | the ribbon's centreline | the ribbon (0.1) | a decal |

### 2f. GPU budget shape for PS4 class at 720p60 ([SET], derivation shown)

PS4 titles above ran 1080p30 = 33.3 ms over 2 073 600 px; 720p60 is 16.7 ms over 921 600 px:
a PIXEL-bound cost keeps roughly its SHARE of the frame; a compute/vertex-bound cost doubles its
share. A18 Pro vs PS4 GPU is the same class by CLAUDE.md's word, not measured here.

| stage | ms at 720p60 | origin |
|---|---|---|
| blade generation compute (~300 k blades, near tiles) | 0.6 | Tsushima "~2 ms for > 1 M blades" at 1080p30 (unverified) x 1/3 blades |
| blade draw (overdraw-bound) | 1.0 | pixel-bound share kept |
| ground material (splat, hex-tile, detail normal, POM near) | 1.2 | Far Cry 5 terrain material ~2.5 ms at 1080p30 (Moore; unverified) x 0.45 |
| terrain CDLOD select + draw | 0.3 | 0c |
| scatter instances | 0.4 | board:2122's path |
| shadows of grass | 0.3 | dithered impostor only |
| **ground cover total** | **3.8** | (rev. 1: 3.5; + terrain select) |

Memory: tiles 256 m [SET], 4 x R8 at 1 texel/m = 256 KiB; 64 resident = 16 MiB. Blade buffer
300 k x 32 B = 9.6 MB. Ground material array 8 x (BC7 1 MiB + BC5 1 MiB + BC4 0.5 MiB) at
1024^2 = 20 MiB. Total < 50 MiB.

### 2g. Lab experiments

| id | question | inputs | solution | PROOF | negative control | to C++ |
|---|---|---|---|---|---|---|
| 2.1 `ground/placement.py` | do class + slope + noise place grass where a photograph would? | OSM landuse/natural at Jura, terrarium z14 slope; an orthophoto tile a scenario declares | density = class table x slope falloff x blue noise vs the orthophoto's green mask | IoU above a threshold measured on a hand-checked tile; zero blades on `highway`, `building`, `natural=water` | shuffle classes: IoU falls to chance | the density rule |
| 2.2 `ground/blade_lod.py` | how many vertices does a blade need per distance for < 0.5 px error? | camera 1.7 m; the Bezier | project; chord error vs vertex count and distance | 15 verts <= 10 m, 7 <= 40 m under 0.5 px at 720p; beyond 40 m a blade is under a pixel wide -> fold/billboard | 3 verts at 5 m: > 1 px | the LOD table |
| 2.3 `ground/hextile.py` | does histogram-preserving hex tiling remove repetition without changing the mean? | a ground albedo tile | Deliot & Heitz precomputation | mean/variance within 2 % over 4 x 4 tiles; autocorrelation at the tile period < 0.1 | linear blending: variance drops > 20 % | the T/T^-1 LUT bake, the 3-sample shader |
| 2.4 `ground/wind.py` | is the wind field deterministic and continuous across tiles? | `WindDeg`, `WindMs`, clock | world-space gust field advected by wind | C0 across borders to 1e-6; same clock -> same field bit for bit | seed per tile: a step | the field function |
| 2.5 `ground/gpu_instancing.py` | does compute placement + cull emit the same instance set as a CPU reference at every camera? | the placer's 4 textures, a camera path | numpy placement (dither pattern) + frustum/Hi-Z cull as a pure per-cell function; permute cell order | identical sets; count under the buffer capacity at every place (0.5); no instance on a refused class | let a cell read its neighbour's count: order-dependent | the placement kernel |

### 2h. Requirements

| requirement | value | origin |
|---|---|---|
| placer output per tile | 4 R8 textures, worker, deterministic order (board:2130) | Tsushima's tile inputs |
| compute -> indirect | one blade pass per frame into a fixed buffer; capacity from lab 0.5 | 0a, 0c |
| terrain | CDLOD select in compute, morph, skirts kept | 0.2 |
| ground material | class -> layer array; hex tiling; detail normal; POM at LOD0 | 2d |
| budget | 3.8 ms of 16.7 at p99 with clouds and water on | [SET] |
| memory | < 50 MiB | 2f |

---

## 3. Realistic water

### 3a. Who has done it and proven it

| body | technique | numbers |
|---|---|---|
| **Tessendorf 2001** | ocean height field as an inverse FFT of a Phillips spectrum, choppiness via horizontal displacement | the origin of every FFT ocean |
| **Horvath 2015** | empirical spectra (TMA/JONSWAP with directional spreading): wind and fetch in, sea state out | DigiPro 2015; code `blackencino/EncinoWaves` |
| **Sea of Thieves** (Rare, SIGGRAPH 2018 Talk: Ang; GDC 2018) | FFT ocean in UE4; colour = deep/sub-surface blend by view angle, sun and a wave-peak mask from choppiness | shipped Xbox One 1080p30 |
| **Far Cry 5** (Grujic, GDC 2018) | one water system for ocean/lake/river/waterfall; flow maps; procedural "irrigation" from spline boundaries; > 50 waterfalls generated; half-precision water shader | shipped PS4; the 13 MB PDF not read -- mesh/ms details unverified |
| **Uncharted 4** (Gonzalez-Ochoa, SIGGRAPH 2016) | rivers/rapids: wave particles along the flow | shipped PS4 |
| **Portal 2** (Vlachos, SIGGRAPH 2010) | flow maps: a vector field distorts the normal map with two phase-offset samples | shipped 2011 |
| **GTA V** (RAGE; Courrèges 2015) | planar reflection at 240x120 (650 draw calls) + cubemap; a plane over a carved bed | shipped PS3/PS4 |
| **Unreal Water plugin** (UE EULA) | `WaterBodyOcean/Lake/River`, `WaterMeshComponent` = quadtree of grids with LOD, Gerstner from a `WaterWaves` asset, rivers as splines carving the landscape, `WaterInfoTexture` | shipped; TECHNIQUE only |
| **Cesium** | quantized-mesh `WaterMask` extension (255 water / 0 land per texel) from coastline data | CesiumJS, Cesium for Unreal |

### 3b. Readable repositories

| repo | licence | read |
|---|---|---|
| `github.com/gasgiant/FFT-Ocean` | MIT | 3 cascades on disjoint wavenumber bands, JONSWAP/TMA, foam from the Jacobian |
| `github.com/tessarakkt/godot4-oceanfft` | MIT | compute Stockham FFT (from `achalpandeyy/OceanFFT`), **CDLOD quadtree**, buoyancy, whitecaps, SSS -- the closest to a whole water system in the open |
| `github.com/achalpandeyy/OceanFFT` | (unverified) | the Stockham compute kernel |
| `github.com/blackencino/EncinoWaves` | (unverified) | Horvath's spectra |
| `github.com/ebruneton/precomputed_atmospheric_scattering` | BSD-3 | the sky the water reflects |
| Filament `docs/Materials.md.html` | Apache-2.0 | `refractionType: screenspace`, `refractionMode: thin|solid`, `thickness`, `absorption` |
| `github.com/osmcode/osmcoastline` + `osmdata.openstreetmap.de` | GPL-3 (tool); ODbL (data) | where the SEA is |
| Cesium `QuantizedMeshTerrainData` | Apache-2.0 | a water mask on a terrain tile |

### 3c. Papers

| title | authors | year | venue | url |
|---|---|---|---|---|
| Simulating Ocean Water | Tessendorf | 2001 | SIGGRAPH course notes | people.computing.clemson.edu/~jtessen/reports/papers_files/coursenotes2002.pdf |
| Effective Water Simulation from Physical Models | Finch | 2004 | GPU Gems ch. 1 | developer.nvidia.com/gpugems/gpugems/part-i-natural-effects/chapter-1-effective-water-simulation-physical-models |
| Empirical Directional Wave Spectra for Computer Graphics | Horvath | 2015 | DigiPro | dl.acm.org/doi/10.1145/2791261.2791267 |
| Water Flow in Portal 2 | Vlachos | 2010 | SIGGRAPH Advances | cdn.akamai.steamstatic.com/apps/valve/2010/siggraph2010_vlachos_waterflow.pdf |
| Rendering Rapids in Uncharted 4 | Gonzalez-Ochoa | 2016 | SIGGRAPH Advances | advances.realtimerendering.com/s2016/ |
| Water Rendering in Far Cry 5 | Grujic, Cutocheras | 2018 | GDC | media.gdcvault.com/gdc2018/presentations/Grujic_Branislav_WaterRenderingFarCry5.pdf |
| The Technical Art of Sea of Thieves | Ang, Catling | 2018 | SIGGRAPH Talks | history.siggraph.org/wp-content/uploads/2022/09/2018-Talks-Ang_The-Technical-Art-of-Sea-of-Thieves.pdf |

### 3d. Feasible through SDL_GPU (re-cut)

| technique | SDL_GPU | cost ([SET] unless stated) |
|---|---|---|
| FFT ocean, 3 x 256^2 | 54 dispatches in 18 passes (0c) | 0.14 ms bandwidth bound (derived); ~0.3 ms with launch overhead |
| Gerstner (4-8 waves in VS) | raster only | ~0; periodic; Unreal ships it, Sea of Thieves did not |
| lakes: flat lid + wind ripples | raster | ~0 |
| rivers: flow map from the polygon's skeleton + way direction (OSM `waterway` ways point downstream) | baked once (worker or compute); two-phase sampling (Vlachos) | 0.1 ms per visible river |
| foam: Jacobian < 0; shoreline foam by depth from `WaterDepth.h` | in-material | ~0 |
| refraction: screen-space thin (Filament) + Beer absorption by depth | one colour + depth read | 0.2 |
| reflection: SSR (board:2129) + sky-view LUT fallback; planar only for a declared still lake | SSR half res | 0.8; planar = a second scene pass -- refused on the frame path by default |
| **water surface mesh: CDLOD in compute** -- 1 select dispatch, 1 indirect instanced patch draw, displacement from cascades in VS | yes (0c) | 0.2 ms GPU; rev. 1 said "CPU builds the quadtree" -- moved to compute |
| **shoreline skirt / bank blending: a compute pass writes depth-fade per patch vertex** | yes | ~0 |
| underwater | a post volume | later |

### 3e. CPU vs compute vs fragment

| element | CPU (once) | compute | fragment |
|---|---|---|---|
| bed | carved (exists, board:2115) | -- | -- |
| lid | the body's level and extent | CDLOD patches, FFT displacement | normal, foam, refraction, reflection |
| shoreline | the polygon | depth-fade | foam band, wetness |
| river | the way + monotone level (3.3) | the ribbon (0.1's kernel with a water profile); flow advection in VS/FS | flow-mapped normal + foam |
| waterfall | where the grade exceeds a threshold | a strip | scrolling detail; mist from `effect/` (board:2137) |

### 3f. Lab experiments

| id | question | inputs | solution | PROOF | negative control | to C++ |
|---|---|---|---|---|---|---|
| 3.1 `water/spectrum.py` | does the FFT height field reproduce the spectrum it was made from? | `WindMs`, fetch [SET 100 km] | JONSWAP/TMA -> h(k) -> ifft2 per cascade; recover the PSD | PSD within 5 % per band; Hs = 4 sqrt(m0) matches; overlap energy counted once | Phillips with a wrong cutoff: PSD misses the peak | spectrum tables, band limits, the Stockham kernel checked vs `numpy.fft` to 1e-5 |
| 3.2 `water/foam.py` | is the Jacobian a good foam predictor? | 3.1's field | J = det of the displacement gradient | foam fraction monotone in wind; zero at 0 m/s | choppiness 0: J = 1, no foam | the Jacobian pass |
| 3.3 `water/river_grade.py` | a river's level along its way, given an aliasing DEM? | `waterway=river` at Jura/Kaiserberg, terrarium z14 | `roads/band.py`'s bounded least squares + MONOTONE downstream constraint | every node in band; non-increasing to 1e-9; the outlet at the lake's level | eps = 0: the DEM's uphill steps counted (> 0) | the monotone band solver |
| 3.4 `water/flowmap.py` | is the flow field tangent to the bank and continuous? | river polygons + the way | shapely medial axis; velocity = tangent x width falloff; 1 m raster | divergence < 1e-3 inside; zero normal at the bank; agrees with the way at every node | reversed way: every node disagrees | the flow-map bake |
| 3.5 `water/reflection.py` | where does SSR fail on water, what does the fallback show? | Blender render of OldTown's river at 1.7 m | pixels whose reflected ray leaves the screen | missing fraction per place; the fallback within threshold there | planar reference: 0 missing | the fallback policy |
| 3.6 `water/cdlod.py` | is the water CDLOD C0 across rungs with FFT displacement on? | 3.1's cascades, a camera path | 0.2's select/morph with displacement sampled at the morphed position | gaps < 1e-6 m at 10 000 cameras; no crack pixel in a render | morph off: cracks | the select/morph for water (shared with terrain) |

### 3g. Requirements

| requirement | value | origin |
|---|---|---|
| ocean extent | osmdata water polygons (ODbL); `natural=coastline` | 3b |
| river level | the monotone band solver | 3.3 |
| flow map | 1 m R8G8 per tile, baked once | 3.4 |
| FFT | 3 x 256^2 cascades, 2.2 MB; 0.3 ms | derived |
| surface | CDLOD in compute, fixed patch capacity (4 096 nodes [SET]) | 0c, 3.6 |
| reflection / refraction | SSR + sky fallback; screen-space thin refraction | 3d |
| budget | 1.5 ms (FFT 0.3, surface 0.4, SSR 0.8) | [SET] |
| weather | `WindMs`, `WindDeg` -> sea state; rain -> ripple layer | present |

---

## 4. Realistic clouds and the atmosphere they stand in

### 4a. Who has done it and proven it

| body | technique | numbers |
|---|---|---|
| **Bruneton & Neyret 2008** (+ 2017 re-implementation) | precomputed 4D scattering LUTs, multiple scattering by iteration, from any altitude | the correctness reference; BSD-3 code with unit tests |
| **Hillaire 2016** (Frostbite) | physically based sky + volumetric clouds (Beer-Lambert, dual-lobe HG, powder, multiple-scattering octaves after Wrenninge 2013), temporal reprojection | shipped Battlefield 1 |
| **Hillaire 2020** (EGSR; `sebh/UnrealEngineSkyAtmosphere`) | no 4D LUT: transmittance 256x64, multi-scattering 32x32, sky-view 192x108, aerial-perspective froxels 32x32x32 | **measured** on mobile (paper): 0.53 + 0.27 + 0.11 + 0.12 = ~1.0 ms. **The engine holds this** (`MediumTransmittanceStage`, `MediumMultiScatterStage`, `MediumRadianceStage`, `AerialPerspectiveStage`) |
| **Nubis 2015** (Schneider & Vos, SIGGRAPH Advances; GPU Pro 7) | a cloud LAYER 1.5-4 km: density = weather map x Perlin-Worley shape (128^3) x Worley detail (32^3) x height gradient by type; ray march 64-128 steps with cheap/expensive alternation; Beer + powder + HG; 1/16 of the pixels per frame with reprojection | **measured**: "under 2 ms on PS4" (Guerrilla's page) at 1080p30 |
| **Nubis 2017** (Decima, SIGGRAPH Advances) | regional authoring, transitions, atmosphere integration, optimisation | shipped HZD |
| **Nubis Evolved 2022 / Nubis^3 2023** (Schneider) | VOXEL clouds (fluid-sim modelled, compressed SDF march, up-res, light-sampling acceleration); flown through | shipped HFW on PS4/PS5; ms in the PPTX/PDF at `d3d3g8mu99pzk9.cloudfront.net/AndrewSchneider/Nubis Cubed.pdf` -- not read |
| **Unreal Volumetric Cloud** (4.26+, Hillaire) | a layer, conservative density to skip the material graph, reduced-res march + temporal, lit from the same SkyAtmosphere, cloud shadow map, distant clouds cheap | shipped; EULA source |
| **Sea of Thieves** | 3D cloudscapes WITHOUT ray marching | shipped; the cheap alternative |
| **GTA V** (RAGE) | a cloud DOME with a scattering term, projected cloud shadows | shipped PS3 |

### 4b. Readable repositories

| repo | licence | read |
|---|---|---|
| `github.com/ebruneton/precomputed_atmospheric_scattering` | BSD-3 | `atmosphere/functions.glsl`, `model.cc`, `definitions.glsl`; the unit tests |
| `github.com/sebh/UnrealEngineSkyAtmosphere` | (README says "provided in hope"; LICENSE unverified -- read before copying) | `Resources/RenderSkyRayMarching.hlsl` (the four LUTs), `RenderSkyPathTracing.hlsl` (the TRUTH: a volumetric path tracer) |
| `github.com/sebh/TileableVolumeNoise` | MIT | `TileableVolumeNoise.cpp`: tileable Perlin, Worley, the Perlin-Worley cloud recipe -- generated on device (board:2140), never loaded |
| `github.com/clayjohn/godot-volumetric-cloud-demo` (+ `-v2`) | MIT | `clouds.gdshader`: a complete Nubis-style march in ~300 lines |
| `github.com/AmanSachan1/Meteoros` | (unverified) | Vulkan Decima-style clouds with reprojection |
| `github.com/JolifantoBambla/webgpu-sky-atmosphere` | MIT (unverified) | Hillaire 2020 in WGSL compute -- compute + raster only, like SDL_GPU |

### 4c. Papers

| title | authors | year | venue | url |
|---|---|---|---|---|
| Precomputed Atmospheric Scattering | Bruneton, Neyret | 2008 | EGSR / CGF 27(4) | ebruneton.github.io/precomputed_atmospheric_scattering/ |
| Physically Based Sky, Atmosphere and Cloud Rendering in Frostbite | Hillaire | 2016 | SIGGRAPH PBS course | blog.selfshadow.com/publications/s2016-shading-course/ |
| A Scalable and Production Ready Sky and Atmosphere Rendering Technique | Hillaire | 2020 | EGSR / CGF 39(4) | sebh.github.io/publications/egsr2020.pdf |
| The Real-Time Volumetric Cloudscapes of Horizon Zero Dawn | Schneider, Vos | 2015 | SIGGRAPH Advances | guerrilla-games.com/read/the-real-time-volumetric-cloudscapes-of-horizon-zero-dawn |
| Nubis: Authoring Real-Time Volumetric Cloudscapes with the Decima Engine | Schneider, Vos | 2017 | SIGGRAPH Advances | advances.realtimerendering.com/s2017/ |
| Nubis, Cubed | Schneider | 2023 | SIGGRAPH Advances | advances.realtimerendering.com/s2023/ ; guerrilla-games.com/read/nubis-cubed |
| Oz: the Great and Volumetric (multiple-scattering octaves) | Wrenninge, Kulla, Lundqvist | 2013 | SIGGRAPH Talks | (not fetched) |

### 4d. Weather-driven clouds from real data

| source | what it gives | resolution | fit |
|---|---|---|---|
| **Open-Meteo** (`hourly=cloud_cover,cloud_cover_low,cloud_cover_mid,cloud_cover_high`, `wind_speed_10m`, `wind_direction_10m`, `precipitation`, `weather_code`) | cover per layer in %, from ICON/GFS/ECMWF; ICON gives layers natively, GEM estimates from humidity | 1-11 km, hourly, forecast + archive | `Scenario::Weather` already has the fields -- this is the provider |
| **ERA5** | `tcc`, `lcc` (surface-800 hPa), `mcc` (800-450 hPa), `hcc` (450 hPa-top), `cbh`; TCC <= HCC+MCC+LCC (overlap) | 0.25 deg, hourly, 1940- | history for a replayed date |
| **METAR** | layers in oktas (FEW/SCT/BKN/OVC) with base height in hundreds of feet AGL, every 30-60 min per airport | point | the only MEASURED cloud base; `CloudBaseAglM` is METAR's word |

Derived mapping: `coverage_low = CloudLow`; bottom = `CloudBaseAglM` (METAR) or an LCL
estimate `125 m/K x (T - Td)` (Espy's rule; verify in 4.3); top = bottom + a type-dependent
thickness (stratus 300-600 m, cumulus 1-2 km [SET, verify against the WMO atlas]). Mid and
high layers as thin slabs; the march is spent on the low layer.

### 4e. Feasible through SDL_GPU and the cost

| stage | SDL_GPU | ms at 720p ([SET], derivation) |
|---|---|---|
| sky LUTs (exists) | compute | ~0.5 (Hillaire's mobile 1.0 ms is a weaker GPU) |
| noise volumes generated once | compute, 128^3 R8 (2 MiB) + 32^3 (32 KiB) | one-time ~1 ms |
| weather map from the provider | CPU or compute at preload: 256^2 RGBA8 per 64 km, deterministic by clock | ~0 |
| low-layer march at QUARTER res (320x180), 64 + 6 light steps, reprojection over 4 frames | 1 compute dispatch | Nubis: < 2 ms at 1080p30 with 1/16 pixels/frame = 129 600 px/frame; ours 57 600 px/frame = 0.44x -> ~0.9 |
| upsample + composite with aerial perspective | raster | 0.2 |
| cloud shadow map (256^2 transmittance, projected) | compute | 0.1 |
| **clouds total** | | **~1.2** |
| voxel clouds (Nubis^3) | possible (compute SDF march); 512^3 R8 = 128 MiB before up-res | DEFER until board:2140's layer is measured -- memory, not capability |
| distant clouds / cirrus | a lit slab in the sky-view frame (RAGE's dome as far field, board:2140) | ~0.1 |

### 4f. CPU vs compute vs fragment

No geometry: a cloud is a density function. CPU: the weather map, the layer heights, the wind
offset per frame. Compute: the march, the lighting, the reprojection, the shadow map.
Fragment: the composite. The one geometric object is the far-field slab.

### 4g. Lab experiments

| id | question | inputs | solution | PROOF | negative control | to C++ |
|---|---|---|---|---|---|---|
| 4.1 `sky/lut_vs_bruneton.py` | do the engine's four LUTs agree with Bruneton? | LUT readback through the door vs `functions.glsl` in numpy | 1 000 (view, sun) pairs | < 2 % away from the horizon, < 5 % at it (Hillaire 2020's band) | zero the multi-scatter LUT: > 20 % at twilight | none -- the oracle |
| 4.2 `cloud/density.py` | does Schneider's recipe yield the declared coverage? | `CloudLow` 0.1..0.9; the noise ported to numpy | fraction of columns with optical depth > 1 | within 10 % of declared; monotone | erosion off: overshoot | the remap constants |
| 4.3 `cloud/base_height.py` | LCL vs METAR's measured base? | METAR + Open-Meteo T, Td at 20 airports x 30 days | Espy's LCL | MAE < 300 m; bias reported | shuffle stations: > 1 km | the LCL rule with its error |
| 4.4 `cloud/march_cost.py` | how many steps before the picture stops changing? | a 1 024-step reference vs 32/48/64/96 | image difference | 64 within 1 % RMS; 32 not | 8 steps: banding > 5 % | step count, growth constant |
| 4.5 `cloud/lighting.py` | single scatter + octaves vs a path trace? | `RenderSkyPathTracing.hlsl` semantics in numpy | Beer + HG + N octaves vs Monte Carlo | transmittance exact; in-scatter within 15 % at 3 octaves | 1 octave: > 30 % dark | the octave count |

### 4h. Requirements

| requirement | value | origin |
|---|---|---|
| provider fields | present + `TemperatureC`, `DewPointC` for the LCL; Open-Meteo (model) + METAR (base) | 4d |
| noise | generated on device from a seed | board:2140 |
| march | quarter res, 64 steps, reprojection; same clock -> same bytes | 4e |
| lighting | from the medium's LUTs; a cloud carries no light | invariant |
| budget | ~1.2 ms clouds + ~0.5 ms sky | [SET] |
| memory | 2 MiB + 32 KiB noise; 256 KiB weather map; 4 x 460 KiB history | derived |

---

## 5. The frame, added up (rev. 2)

| layer | ms at 720p60 | share | origin |
|---|---|---|---|
| sky LUTs | 0.5 | 3 % | Hillaire 2020 scaled |
| clouds | 1.2 | 7 % | 4e |
| ground cover incl. terrain select | 3.8 | 23 % | 2f |
| water incl. CDLOD | 1.5 | 9 % | 3g |
| buildings (base pass share) + road decals | 2.5 | 15 % | 1i |
| compute geometry per frame (GPU cull exists, ribbons, terrain/water select, FFT) | 0.4 | 2 % | 0c bandwidth bounds x ~3 for launch/latency [SET] |
| **the world's look** | **9.9** | **59 %** | |
| left for geometry raster, clustered lights + shadows (2128), SSR (2129), post, present | 6.8 | 41 % | remainder |

What moved from the CPU to the device is not free on the device, but it is CHEAP there (the
compute rows are bandwidth-bound at tens of microseconds) and it removes the CPU's per-rung
mesher time and the bus traffic of ~100 MB of road vertices. The remainder is tight: RAGE and
Unreal spend ~40 % of a frame on shadows and lighting alone. The first measurement that shows
this table wrong is the one to take first: `make shots` with each stage toggled, p99 per
stage, at OldTown, Jura and Venice. Where a stage exceeds its row, the row falls, never the
target.

## 6. Open questions this report could not close

- Ghost of Tsushima's "~2 ms" and Far Cry 5's ms are from secondary summaries; read the GDC
  slides before quoting them in an item
- Atomics and subgroup ops on SDL_GPU: neither the header nor the wiki says a word. Buffer
  atomics are plain SPIR-V and should pass SDL_shadercross to MSL/DXIL; a CASE on this device
  is the proof, not this sentence. Until it is green, design without them (0b)
- Kendzi3D's licence (BSD-3 per OSM wiki; LICENSE 404) and `sebh/UnrealEngineSkyAtmosphere`'s
  (no line in the README) -- check before copying
- A18 Pro vs PS4 throughput is CLAUDE.md's assertion, not a measurement of this tree
- The road ribbon's resident size (~100 MB at 0.5 m over a city, 0c) is the one number in the
  compute split that could refuse it; 1 m sampling beyond the near ring, or expansion per frame
  for the far ring only, are the two answers, and lab 0.5 picks by counting
- Nubis^3's voxel path for a flown-through cloud is deferred on memory, not refused
