# Terrain work selection: measured alternatives

Owner: [#515](https://github.com/Weichwolf/outshine/issues/515), spatial representation
in [#12](https://github.com/Weichwolf/outshine/issues/12).
Inputs are the existing native height-page exports from `terrain_cache_reference.py`.
No network acquisition, asset regeneration, changed world, or relaxed geometry limit.
These are CPU comparisons, not new runtime paths or device-budget evidence.

## Decision

The native current-opaque-depth selector is implemented separately. Its remaining
Körbersee bottleneck is terrain itself: the instrumented first frame submits
6.55 million terrain triangles; vertex/fragment phases overlap at about 31.8 ms.
A smaller scene or an isolated good frame cannot admit this port.

None of the alternatives below justifies immediate native expansion. Lower-envelope
proxies lose their useful coverage once made conservative. Choosing early exact
terrain by distance or projected coverage produces modest savings; full native
cost and first-frame reliability remain unknown. Normal cones are safe in the
measured CPU cases but reject little, especially in Vienna. Compare representation
and spatially adaptive geometric detail next; keep shading detail independent.
The subsequent [local patch comparison](terrain_patch_lod.md) cuts the same
one-pixel all-resident geometry by 45–51%; it supports a bounded native prototype,
with shading and actual GPU acceptance still open.

## Conservative lower envelopes

`terrain_occluders.py` fits a plane through three heightfield corners. The minimum
and maximum residual over every fine vertex enclose all affine triangle interiors.
It validates convexity, the complete straight ordered boundary, consistent fine
winding and coverage. Thus each accepted patch remains a continuous heightfield.

A lower plane alone is **not** an occlusion proof. A viewing ray can enter the
patch through its side below the actual surface and hit the lower plane without
crossing any source triangle. The initial 320x180 Körbersee comparison exposed
12 such unsafe depth samples, despite an unchanged final image in that case.
The side-entry counterexample is retained in the verification.

The admitted CPU construction requires the viewing ray to cross an upper plane
and the lower plane inside the same convex footprint. Continuity then guarantees
an actual surface crossing between them. The lower polygon is clipped against
that upper-intersection condition; two convex quads produce at most eight vertices.
Camera/plane degeneracy fails open. This is a local heightfield construction,
not a general offset-surface or closed-earth reconstruction theorem.

Körbersee, 1280x720: 16,255 pages; 6,657,792 frustum interior triangles.
Safe proxies add 86,424 triangles and reject only 8,704: 6,735,512 combined,
more than the baseline. Zero changed depth pixels and zero front-of-source
proxy samples. No native implementation is justified by this comparison.

## Outer curtains and a runtime geometry defect

The optional `--skirt-aware` comparison includes all source outer skirts in both
rasters. A lower proxy is admitted only above the highest skirt-bottom plane,
with the eye above that plane and outside the convex tile footprint. The ray
then crosses an actual top triangle or outer wall before reaching the proxy.
Unknown orientation and degenerate cases fail open. Skirts are always retained.

Körbersee at 320x180: 119,584 proxy triangles reject 311,168 of 6,657,792
interior triangles. Net saving is (311,168 - 119,584) / 6,657,792 = 2.88%,
with zero changed depth pixels or unsafe proxy samples. This still does not
justify an additional native occluder pass.

This experiment exposed inward-facing runtime skirts. An independent native GPU
fixture uses two neighboring flat tiles at heights 0.6 and 0.2, viewed from
x=2 at height 0.4 with near=0.1. The near gap wall at x=0 must produce reversed
depth 0.1/2 = 0.05. Before correction, the GPU returned 0.1/3 = 0.0333333,
the far wall at x=-1. Reversing only skirt winding restores the analytic depth.
The native regression is `GroundLattice/OuterSkirtsFaceTheGap`; this correction
is independent of the rejected proxy optimization.

## Exact early terrain

`terrain_front_depth.py` renders a selected subset of original pages first,
queries their current depth, and renders only the remaining retained patches.
Early pages are counted once. Selection uses distance or projected coverage;
world AABBs, local oriented boxes and exact projected vertex bounds are compared.
All reported complete raster comparisons below have zero changed depth pixels.

Körbersee, 320x180, original eight-cell patches:

| Early selection | Bounds / depth levels | Early pages | Baseline triangles | Early + late triangles |
|---|---|---:|---:|---:|
| Distance | World / 4 | 256 | 6,657,792 | 6,408,832 |
| Coverage | World / 4 | 1,024 | 6,657,792 | 5,713,664 |
| Coverage | Local OBB / 4 | 1,024 | 6,588,160 | 5,495,040 |
| Coverage | Exact projected vertices / 10 | 1,024 | 6,583,424 | 5,303,936 |

Tighter bounds also change the frustum baseline; compare each row with its own
baseline. The strongest row saves 1,279,488 / 6,583,424 = 19.44%, before native
selection, pass and memory costs. Exact projected vertices are an expensive
comparison bound, not an approved runtime selection method.

The optional reverse-depth bias comparison preserves the existing absolute
`1e-6` default in `current_visibility.occluded`. A relative `1e-6` case is
explicit: the world/4/1,024-page result becomes 5,571,200 triangles. This is not
permission to reduce native numerical margins; projection, raster and history
error need their own bounds and GPU tests.

## Meshoptimizer normal cones

`terrain_cones.py` calls the installed meshoptimizer C API; no bundled source or
new implementation. The installed library used here is Homebrew meshoptimizer 1.3.
Perspective rejection uses the documented apex, axis and cutoff formula with
explicit padding. A separate every-triangle oracle checks each rejection.
Skirts remain independent. Patches are limited to four/eight cells so they fit
the library's documented cluster limits.

| Place / yaw | Candidate triangles | Cone-rejected triangles | Unsafe rejected patches |
|---|---:|---:|---:|
| Körbersee / 0° | 6,657,792 | 568,832 | 0 |
| Körbersee / 180° | 5,960,064 | 546,944 | 0 |
| Wien / 0° | 1,751,680 | 2,816 | 0 |

All three complete depth comparisons have zero changed pixels. These are 320x180
CPU camera views, not native measurements or complete-scene savings guarantees.

## Reproduce

Use system-temp output paths. Reports include page-data and scenario fingerprints;
older exploratory receipts are identified separately in the owning issue.

```sh
python3 test/experiments/render/verify_terrain_occluders.py
python3 test/experiments/render/verify_current_visibility.py
python3 test/experiments/render/verify_terrain_cones.py --library /path/to/libmeshoptimizer.dylib
python3 test/experiments/render/terrain_occluders.py --pages "$PAGES" --scenario "$SCENARIO" --output "$OUTPUT"
python3 test/experiments/render/terrain_front_depth.py --pages "$PAGES" --scenario "$SCENARIO" --output "$OUTPUT" --priority coverage --bounds obb --near-pages 64 256 1024
python3 test/experiments/render/terrain_cones.py --pages "$PAGES" --scenario "$SCENARIO" --output "$OUTPUT" --library /path/to/libmeshoptimizer.dylib
```

The raster oracle omits near-crossing triangles, alpha and MSAA; skirts are
included only by the explicit curtain comparison. Its CPU
projection and double precision do not prove shader rounding or material quality.
Reported CPU wall time includes validation and is not a GPU-time prediction.

## References

[GPU-Driven Rendering Pipelines, SIGGRAPH 2015](../../../doc/references/geometry/siggraph/2015-gpu-driven-rendering-pipelines.pdf)
provides the culling/batching reference; the meshoptimizer public header documents
its actual cone API and perspective formula (Real-Time Rendering 4, section 19.3).
[Simplification Envelopes, SIGGRAPH 1996](../../../doc/references/geometry/siggraph/1996-simplification-envelopes.pdf)
figures 2–4 clarify why enclosing half-spaces and closed prisms matter; the local
ray-bracket construction above is not claimed as that paper's algorithm.
[Geometry Clipmaps, SIGGRAPH 2004](../../../doc/references/terrain/siggraph/2004-geometry-clipmaps.pdf)
is the reference for separating geometric sampling from normal detail.

## Larger adaptive patches

A further cached-input comparison uses all 16,255 Körbersee pages, 1280x720,
view yaws 0/180 and the same 0.5-pixel positional allowance. All cases retain
fine patch boundaries and the original outer skirts. This is a frustum-only
CPU model; it does not establish native GPU times or pixel equivalence.

| Patch cells | View 0 triangles | View 180 triangles |
|---:|---:|---:|
| 8 | 5,508,280 | 4,987,008 |
| 16 | 6,846,824 | 6,304,240 |
| 32 | 8,233,736 | 7,577,904 |

Replacing every patch with a larger one is rejected: view 0 grows by
(8,233,736 / 5,508,280 - 1) * 100 = 49.48%. Larger shared error domains
force finer selected levels. A lower theoretical boundary floor alone does
not predict useful work savings. The existing CLI now also accepts 32 cells;
`verify_terrain_patch_lod.py` checks its affine errors and complete boundaries.

## Shared coarse patch edges

`terrain_patch_stitching.py` compares mixed edge spacings on the same sixteen
eight-cell patches per page. Neighboring edges use the finer spacing; original
page borders and skirts stay fine. Exact common-refinement vector bounds force
unsafe variants to refine, retaining the 0.5 px allowance. Each refinement lowers
a finite level potential; a page requires at most 49 selection rounds.

All 16,255 cached Körbersee pages, 1280×720, frustum-only comparison. A complete
page may retain the old patterns when stitching would cost more:

| Pattern policy | Yaw 0 triangles, with skirts | Yaw 180 | Saving |
|---|---:|---:|---:|
| Original fine boundaries | 5,508,280 | 4,987,008 | — |
| Internal 2:1 neighbors, page fallback | 5,363,894 | 4,890,669 | 2.62% / 1.93% |
| Unrestricted neighbor levels, page fallback | 5,250,987 | 4,790,074 | 4.67% / 3.95% |

Without the page fallback, 2:1 stitching increases the opposite view's count.
The policies evaluate 88,331/96,585 exact variants and need 144/353 shared index
patterns (38,280/81,480 bytes). Measured Python preparation takes 20.93/21.55 s
beside a frozen lint. These are model construction times, not native frame costs.
Extra native neighbor selection, grouping and error metadata are unmeasured.
The small reference-count saving does not justify a native integration here.

Five checks cover random mixed-level topology without internal open edges or
T-junctions, independent dense vector interpolation, near-plane/peak refinement,
and unchanged defaults. Twenty-four old/new index arrays and twelve error stencils
remain byte-identical. Contact/temporal pixels, fine normals and native GPU costs
are not proved by this model. The production renderer and its PNGs are unchanged.

[Geometry Clipmaps](../../../doc/references/terrain/siggraph/2004-geometry-clipmaps.pdf),
page 4, figures 3–4 and section 6.2, motivates compatible boundaries and spatial
transitions; both the equations and diagrams were inspected. This experiment
uses explicit patch triangulations, not the paper's clipmap/morphing scheme.

## Exact early patches with adaptive LOD

`terrain_early_patches.py` compares early eight-cell patches with the existing
0.5 px positional allowance at 1280×720. Early patches use original fine geometry;
late patches keep their selected LOD. Each unculled reference uses exactly the
same geometry as its culled case, including the early replacements. Early geometry
is counted once. The existing absolute reverse-depth bias of 1e-6 is unchanged.

All 16,255 cached Körbersee pages are read; 3,620 pages intersect conservative
frustum bounds at yaw 0. The adaptive baseline has 4,558,520 interior triangles
and 926,720 unchanged skirt triangles. Ranking by projected boxes selects unsafe
near-plane bounds first: the first 512 patches cover zero raster pixels. It saves
nothing. Ranking only positive front-facing triangle area avoids that failure,
but requires projecting/testing the fine triangles before selecting the early set.

| Early patches | Early triangles | Late triangles | Total with skirts | Saved against identical geometry | Changed depth pixels |
|---|---:|---:|---:|---:|---:|
| 128 | 16,384 | 4,520,464 | 5,463,568 | 23,416 | 0 |
| 512 | 65,536 | 4,430,784 | 5,423,040 | 75,248 | 0 |
| 2,048 | 262,144 | 4,033,168 | 5,222,032 | 305,920 | 0 |

The largest case saves (5,485,240 - 5,222,032) / 5,485,240 = 4.80% against
the original adaptive baseline, before selection/pyramid/extra encoding costs.
The larger 305,920 saving uses its own more expensive fine-early baseline.
This experiment does not justify adding an early-patch path to the native renderer.
The 320×180 comparison also has zero changed depth pixels; 512 early patches
only save 36,920 interior triangles against the original adaptive baseline.

Four independent synthetic checks exercise overlap, fine/coarse replacement,
ranking, capped incremental budgets and counting each early patch once.
The opened 720p depth comparison is consistent with the reported zero difference.
Pixel-centre CPU rasterization does not prove shader rounding, clipping, MSAA,
normal/material equality, native scheduling or a net GPU gain. Skirts are counted
but not rasterized. No runtime code, image threshold or error allowance changes.

```sh
python3 test/experiments/render/verify_terrain_early_patches.py
python3 test/experiments/render/terrain_early_patches.py --pages "$PAGES" --scenario "$SCENARIO" --output "$OUTPUT" --priority surface --width 1280 --height 720 --budgets 128 512 2048
```

## Adaptive patch merging

`terrain_patch_tree.py` chooses between 8/16/32-cell patterns bottom-up instead
of replacing all patches with one larger size. Each candidate keeps its own
0.5 px error bound and complete fine boundary. Off-frustum children cost zero;
merging must beat the sum of selected children. Outer skirts remain unchanged.

| Körbersee view | Eight-cell baseline with skirts | Hierarchical choice | Reduction |
|---|---:|---:|---:|
| 0° | 5,508,280 | 5,438,784 | 1.26% |
| 180° | 4,987,008 | 4,939,600 | 0.95% |

All 16,255 pages are fingerprinted; 6,667 intersect either view. The selector
matches exhaustive evaluation of all 17 admissible tree partitions on 1,000
random positive-cost inputs. Selected leaves cover every cell exactly once;
mixed detail, partial visibility, invalid shapes and negative costs are checked.
The optimum is restricted to these patterns and precomputed bounds. It is not
a global geometry optimum. Native metadata, draws and compaction cost are not
included. This small gain does not justify adding 15 index patterns and another
hierarchy to the native path. No new native path is adopted.

```sh
python3 test/experiments/render/verify_terrain_patch_tree.py
python3 test/experiments/render/terrain_patch_tree.py --pages "$PAGES" --scenario "$SCENARIO" --output "$OUTPUT"
```
