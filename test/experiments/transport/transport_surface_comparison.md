# Source-bound traffic surface families (#357)

All lower roads in the current Vienna Python body used the same grey inspection
material. The generator retained source classes and surface tags without passing
them to the picture. This experiment adds a bound material-family field on the
**same 95,456 lower traffic facets**, with actual whole-facet family integrals.
All384,928 positions,687,524 triangles, supplied normals and old material slots
remain exactly unchanged. No terrain, rail, pier or bridge geometry is replaced.

## Sources, material ownership and interpretation

First local SIGGRAPH basis: [Chen et al.2008](../../../doc/references/transport/siggraph/2008-interactive-procedural-street-modeling.pdf), Fig2
separates source graph from generated content. [Feibush, Levoy and Cook1980](../../../doc/references/materials/siggraph/1980-synthetic-texturing-using-digital-filters.pdf), p2/Fig1,
was read and optically examined: object/texture/image spaces and equivalent edge/
texture filtering matter. Our facet means are **not their image-footprint filter**.
No paper algorithm or SDK/foreign code is copied into this implementation.

Material explicitly confirms this limited interpretation in
[#357/6202135515](https://github.com/Weichwolf/outshine/issues/357#issuecomment-6202135515);
Transport acknowledges its boundaries in6202309555. Existing linear RGB values
are read from `src/assets/world/ground-materials.json`, unchanged. Asphalt, paving,
earth_dry and crushed_stone are procedural choices for the local comparison.
The actual MVT supplies only paved/unpaved/absent surface tags; paved does not
prove stone paving or asphalt, unpaved does not prove dry earth. Per-source basis
records preserve that distinction. Unsupported concrete surface values remain
unplanned with their original tag, rather than being silently generalized.
Native MR, wetness, filtering and layer composition remain #6/#189; light unchanged.

## Exact sources and lower-surface binding

Clean fresh generation, independent replay, all three renders, cost measurements
and final code controls: **b442d6c51d10ef0c00200be54b9fa8609a854ff3**.
Camera settings are copied separately from export evidence: no inherited path,
old hash or exporter copy counters. Each scene names the actual NPZ and exact
source-array payload; an explicit stale-metadata regression checks this boundary.
No network source refresh, SDK load or new runtime dependency.

| Input | SHA256 |
|---|---|
| #303 selected original input | `6d39b0165d18e8c3b466b38207d4291d564bcbd9a710628984bcacdb449f2a79` |
| saved noded graph | `7b56c5f15046bcd9ebc37524a84f7ac713380165a183d03fc9a9275493db0785` |
| source-ID table | `1af5997a16aed95cf38576dbe41344a27056133e1b1c3470c994ca4fc4a13231` |
| source-usage table | `0a17287132cca6e9bdb18923e01b91ab041317802eae433c7010abb150262bbc` |
| actual #303 ground | `16cd7e8b774136dc50e70e9549d376630b4f7bb88e221f4b46527c30367627e5` |
| old lower-source fixture | `fcc91443d76d69223c8173061cf1f2f69d78446a9cc26fd58d9482c3e4f68db4` |
| #352 complete baseline body | `19c392460839a4a7aebaec570982190d2ea18fccc3bd5aedac792be690697a4b` |
| #352 baseline report | `f97c5ab63888c1ac34153664e836b9b6c825aa54309f57ad27b1c5e67ef01963` |
| Material's unchanged JSON | `8d98fc709485870de6c8d61af160c1337424ef00994a512a5dda43173c88a658` |

6,216 actual noded edges plus48 explicitly selected old `ground` sources supply
6,264 corridor records. Each retains edge/used-owner IDs, saved half-width and basis;
original full attributes remain in the pinned captures. Empty/ambiguous source
classification stays explicitly unknown. Deck/approach/deep roles do not label the
lower surface. Crushed stone is only bound to the actual lower traffic facets under
chosen used lower rail corridors; no upper-rail projection paints ground underneath.
Exact XYZ triangle/slot matching binds the lower face-ID run of the complete body.
The old3259-source usage ledger is neither rebuilt nor granted new usage credit.

## Polygon areas and measured approximation

Source corridor widths use the actual saved graph widths and the original flat
component band convention; old lower corridors use their original own width/role.
Family union and declared precedence produce disjoint polygon domains: paved
carriageway, default carriageway, embedded tram default, unpaved, paved path,
default path, default rail bed, default structure. This is a **material overlap
recipe**, not a routing, access or physical-lane precedence. Each family's removed
overlap area is recorded. Unknown family0 remains the old material.

`whole` indexes the1,117 polygon parts directly. `cells` bisects large parts at bbox
midlines, preserving their area/holes, before the same triangle intersections:
1,307 cells/77 splits. Both retain the same actual source field. No source snapping,
terrain change or geometry tessellation. Exact projected XY fractions also equal
surface-area fractions within each affine 3D facet (its Jacobian is constant).

| Actual projected family area | m² |
|---|---:|
| paved carriageway | 411,717.102 |
| default carriageway | 40,521.020 |
| embedded rail default | 8,738.011 |
| unpaved | 60,417.020 |
| paved path | 156,958.708 |
| default path | 31,186.954 |
| default rail bed | 41,257.309 |
| default structure | 1,965.857 |
| unassigned | 6,577.332 |
| total | 759,339.311 |

0.8662% stays unassigned; no new landcover/grass claim. Reason codes:0 resolved,
1 cooperative time,2 pair budget,3 numerical/tiny facet. Both complete calls have
95,454 resolved and two tiny reason3 faces, total1.7453e-9 m²; no time/pair exhaustion.
Tiny unknowns preserve their old face, not an omitted piece of geometry.

Both methods differ at most3.1321e-11 m² per stored component. Independent global
intersection of actual traffic coverage with each family domain differs at most
6.4349e-8 m². Stored rows sum to actual triangle areas; all lower body bindings and
source records replay. Midpoint classification alone moves13,964.137 m² between
families (half-L1 area error),1.839% of the traffic area; the interior-island control
shows why an intact centroid is insufficient.

The rendering uses **constant linear area means on each original facet**. It
conserves the integrated input albedo before Float32 quantization, not its spatial boundary.83,313 facets have
more than one positive contributor (including tiny leftover unknown fractions);
these span564,774.828 m². Exact source polygons and proportions are retained, but
fine boundaries can blur across a large triangle. This is not a general LOD filter,
a spatially exact finish, nonlinear PBR mixture or a measured asphalt survey.

## Costs and copies

Full actual area phase: whole26.015 s/103,251 intersection candidates;
cells7.431 s/104,263 candidates, identical quality. Centroid5.688 s is faster but
misses whole-area content and is not selected. Full research build43.752 s includes
both alternatives, midpoint control and export; peak RSS242.039 MiB. This is
an offline experiment, not runtime load/frame time or native performance.

Isolated equivalent-output comparison on1,024 deterministic original facets:
seven normal repetitions, one separate Memray1.20.0 call each after identical input/
family decode. No renderer during cost measurements; shared host, not a device gate.

| Same sample phase | Whole | Cells |
|---|---:|---:|
| Median normal ms | 311.748 | 140.804 |
| Intersection candidates | 1,124 | 1,138 |
| Allocation/reallocation events | 1,228,563 | 464,471 |
| Cumulative requested bytes | 790,043,876 | 113,462,659 |
| Tracked peak heap bytes | 2,598,481 | 2,307,929 |
| Instrumented ms | 1,481.208 | 672.936 |
| Worker peak RSS MiB | 192.445 | 192.582 |

Equivalent actual saved rows within1e-7 m², no sample fallback. The phase includes
family decomposition/index and intersections; excludes source-family construction,
body decode/export and rendering. Input coordinates processed per overlay and
locality differ despite similar candidate counts. Cumulative allocations are not
live/unique bytes. Known sample arrays:73,728 output area bytes,1,024 reason bytes,
49,152 XY-sample copy bytes. Internal GEOS/copy totals are unobserved.

Full new face-colour payload11,000,384 bytes; unchanged base arrays47,739,616 bytes.
Renderer explicitly repeats33,001,152 colour bytes into corners and reads back
another same-sized array for verification. Blender/GPU storage and internal copies
are additional; these are partial known copies, not a complete bandwidth model.

## Actual rendering and regression

Optional `face_base_colour_linear_rgba` is imported unchanged as FLOAT_COLOR/CORNER,
linked to the actual Principled Base Color. Finite RGB0…1/opaque alpha1, per-face
size, readback and source mesh digest are checked. Real64×64 two-face emission
pixels verify the actual Base-Color source values; deliberately missing binding
fails the same expected-colour control. Legacy source normals/shader path retained.
All source array values remain exact; independent albedo-mean readback error0.

Matched regional1280×720 pair, same #352 camera/1400 m span, light, unchanged
roughness0.86/metallic0 and geometry; no closeups/exaggeration/new display geometry.
Both720 images and local1920×1080 after image were actually opened. Fresh legacy
before pixels equal the prior #352 after image exactly (0/921,600 differences;
PNG file hashes differ). Source-based after changes132,092/921,600 pixels14.3329%,
104,295 over1/255, worst31/255. Road/earth/path hues are now distinguishable in the
regional network. Empty surroundings, schematic bridge inspection colours and
fine faceted material boundaries remain; this is far from finished world art.

Blender5.2.2/EEVEE, shared #291 helpers,16 samples/2 threads/nice10,
Mesa llvmpipe LLVM19.1.7 software. Observations: before20.181 s/2170.203 MiB,
after19.888 s/2243.988 MiB, local1080-after31.455 s/2394.090 MiB.
Renders were sequential after the independent replay and all cost probes; no isolated GPU,
FPS or whole-world budget claim. Each import checks actual positions, topology,
normal attributes and (for new bodies) every supplied colour at Float32.

PNG hashes: before37a244f7ac72fce54aac38863192567a11897fb25fc40dfba9cc54b141c1f838;
after8637939932dfe22e99131daa43df4dfa3d43a184e8661b2dc7c039bbbecacd10;
local1080-aftera3540944e41e58369d89eb2128e7e44dc0f2a485fac8620ddc99b6affa67a0c2.
Images remain local; no gallery/server/doc-image publication.

## Reproduction and limits

[README commands](README.md). Current actual outputs:
`/tmp/outshine-transport-surface-357-final-b442d6c51`, cost traces
`/tmp/outshine-transport-surface-357-cost-final-b442d6c51`. Required originals retained:
`/tmp/outshine-transport-network-303-cf33992d3` and
`/tmp/outshine-transport-upper-352-patch-3d82488b8` plus their earlier pinned inputs.
`comparison.json`, actual source/family JSON, all three area NPZs, coloured body,
scene/import receipts, independent replay and pixel comparisons are preserved.

Existing Python/NumPy/Shapely and Blender/Pillow environments; optional Memray
only for profiling.3 GiB AS/180 CPU seconds generation/replay,90 CPU seconds cost;
60 s cooperative per alternative in the full comparison (normal kernel default30 s),
262,144 query faces,8,192 corridors/cells,16 family domains,1,048,576 candidate pairs
and256-face batches. Renderer bounds colour imports to1,048,576 faces. Single GEOS
calls/decomposition can overrun a cooperative deadline; hard process bounds remain.
Local unresolved faces remain unknown. Failed full research comparison leaves the
original baseline intact. No native runtime producer, new API or shared cache added.

Terminal frozen b442d6c51 gate: **51 source/library +16 real Blender controls,
zero skips**, all exit0. Complete actual generation, independent area/body replay,
four equivalent-output cost children and all three fresh renders also exit0.
All ten affected Python files pass Ruff/format/AST≤120. New colour body has
58,740,000 source-array bytes, independently checked against scene metadata.
Documentation/diff checks are recorded in the delivery PR. No native gate claimed.
New source family/area methods remain experiments until a measured library/consumer
contract is supplied. Further supplied surface tags, exact spatial filtering,
materials on upper structures, functional cross-sections/continuous routes,
LOD/movement and native/device integration remain open in their owners.
