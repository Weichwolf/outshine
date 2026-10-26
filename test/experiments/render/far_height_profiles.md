# Plan-first height profiles (#1085, parent #1083)

This is a bounded Python comparison, **not an admitted native far product**.
The complete supplied native cell remains the quality opponent. The first
candidate saves geometry but does not preserve the original material/lighting
response or establish a continuous transition. No native format, renderer,
resource budget, view distance or source content was changed.

## Method and ownership

`far_height_profiles.py` reads the existing BSC3 forms through the verified
parent reader. For every supported source part it retains the original outer
ring, holes, bottom, source frame, original parent ordinal and part identity.
The existing planar roof-domain recipes determine the minimum and maximum roof
height. Planning does not build a Fine/Shell mesh or triangulate its caps.
Unknown forms, invalid footprints, curved domes and incomplete hip coverage
request the complete original source, including its other parts.

The candidate is the same footprint extruded to its maximum roof height.
Constrained cap triangulation preserves the rings and holes; no hull or XY grid
closes footprint gaps. Original source-local millimetre rounding and forward
Float32 placement are reused. A collapsed triangle requests the whole original
source. The mesh shares complete vertex records while retaining UV/category
seams. Its provenance distinguishes source/part/profile face/ring/edge from
actual original native face bindings: generated faces do not invent native IDs.
Anonymous external IDs remain separate through their original parent ordinals.

`compare_far_height_profiles.py` queries metadata for all poses before generating
any candidate and cooks each required source once. The whole cell stays in each
result; unsupported or unselected sources retain their complete original Shell.
Loading original geometry here serves the independent opponent/fallback, never
candidate construction. The generator and source-owned shape are independent of
the selected camera; no camera-specific SSD products are proposed.

A provisional displacement is the original roof height range multiplied by the
source-up length, plus the declared original Shell error and an explicit local
rounding/Float32 allowance. Both surfaces must be enclosed before the existing
outward-rounded `far_product_cut.projection_error` is used. The observer box is
6 m on each side of the eye; the unchanged comparison allowance is one pixel at
1280×720. Missing, nonfinite or negative original-error metadata keeps the source
original. This is a conditional geometric model, **not a proved native IEEE,
opacity, radiance, shadow, reflection or transition bound**. A small two-sided
point displacement does not constrain first-surface identity or shading.

## Real input and results

The existing `building-full-view` fixture supplies all inputs; no export was
requested. The manifest, original archive/package hashes, parent/source/cell
membership and the native reference geometry are verified by existing readers.
The cell has 576 original sources and 14,169 Shell triangles. Planning supports
575 sources / 617 parts / 1,472 roof domains; source 2893 retains its complete
original Shell because its supplied hip facets do not cover the footprint.
572 sources satisfy the provisional far query and emit successfully.

| Complete cell | Original | Far profile |
| --- | ---: | ---: |
| Triangles | 14,169 | 7,728 |
| Full diagnostic draw/provenance arrays, bytes | 2,226,396 | 1,200,672 |

The decrease is 45.46% in triangles and 46.07% in these arrays. The arrays include
48-byte expanded diagnostic vertices and both provenance tables, rather than the
native packed vertex format. They are **not native asset/SSD/GPU residency bytes**.
The 500 m near control selects only three flat-roof sources; both versions still
have 14,169 triangles and 2,226,396 diagnostic bytes. Its triangle ordering/fields
are not word-identical, although the three rendered lighting pairs are pixel
identical. The unchanged-original control separately proves exact preservation
when no candidate is selected.

The saved-field consumer uses the corrected triangle/Rtree query with four
fixed samples per native pixel in the union projected cell bounds. It queries
ready assets, without rerunning source recipes. Categories are decoded from the
original triangle corners before interpolation: interpolated integer tags can
round across a category boundary and produced two spurious near-control changes
in the superseded first diagnostic. Cross-category input triangles are rejected.

| Pose | Original hits | Added / lost | Different source | Same-source material category changes | Max normal angle |
| --- | ---: | ---: | ---: | ---: | ---: |
| static-020 | 298 | 4 / 0 | 10 | 28 | 90.53° |
| moving-020 | 305 | 3 / 0 | 10 | 30 | 92.57° |
| moving-021 | 319 | 2 / 0 | 20 | 23 | 90.48° |
| near control | 48,667 | 0 / 0 | 0 | 0 | 0.00691° |

The largest same-source depth change is 5.45 m in the far poses; the near control
reaches 0.001318 m. These depth values are not surface-distance certificates.
Preserving footprint gaps avoids the older column-field gap-fill mechanism but
raising roofs still changes which original source/face is first visible.

## Actual pictures and transition

`render_far_height_profile.py` produces actual matched 1280×720 Eevee images with
16 samples, identical verified cameras, source vertex colour, original/candidate
custom normals, common neutral metallic-roughness materials and a shared ground.
Three far poses and the near control each have original/profile pairs for day,
backlight and night: 24 images. Night uses the same dim environment/sun and local
area light for both versions. Native procedural wall/roof UV shading, emissive
windows, weather/terrain and native shadow/filtering/MSAA are **not implemented
or qualified** by this neutral opponent; the saved UV/metre/category fields remain
available to the independent field comparison.

All 24 images and five transition/control images completed with status 0. Their actual
pixel crops/error sheets were opened. `test/scripts/pixels.py` independently
confirms the first day comparison (95 changed pixels, 58 over one, maximum 12).
`compare_far_profile_images.py` records full-frame error, the changed rectangle,
its mean error and image hashes. Whole-frame means alone would hide the tiny far
cell in a large common sky/ground. Crops enlarge original pixels; error displays
multiply RGB8 error by eight rather than inventing detail.

| Far lighting | Changed pixels across three poses | Maximum RGB8 channel error |
| --- | ---: | ---: |
| Day | 84–102 | 11–12 |
| Backlight | 78–95 | 15–18 |
| Night | 72–76 | 31–39 |

The night sheets visibly change the small locally lit roof contribution. All
three near lighting pairs have zero changed pixels. These are isolated real-cell
controls, not full-world beauty images or a twelve-place/device gate.

`transition_far_height_profiles.py` locates a real one-pixel metadata threshold
for source 2511, a source visible in the far field probe. Fixed-orientation eyes
0.25 m apart bracket it at 3,810.94 m from the cell centre. The source changes
from original to profile; complete-cell triangles change 7,877→7,873. Four actual
day images show original/profile at each eye. Maximum inter-eye RGB8 change is
3 for the complete original and 6 for the profile. A fixed-eye counterfactual then holds only source 2511 original while retaining
the exact after camera and all other selected sources. The representation switch
alone changes seven pixels, three by more than one, with maximum error seven
RGB8 levels. Its fifth actual transition/control image was opened alongside the
others. The final transition producer regenerates all five geometry assets; they
are word-identical to the rendered inputs. A hard threshold alone has not
established a stable radiance transition. There is no native LOD relaxation.

## Work, scaling and limits

One ordinary fresh comparison on producer `c6d9597fd` reports 0.588 s roof
planning, 1.072 s for 2,300 source visits over four metadata queries, 4.417 s
profile baking and 10.095 s including original-reference load, diagnostics and
compressed output before reporting. This is a single Linux/Python observation
under concurrent low-priority software-render verification, not a native timing
comparison or solver/device winner. Generated arrays: 5,148 arrays, 1,168,080
payload bytes. Before vertex sharing, actual corner records account for
1,082,304 bytes. These work/payload counts do not measure all memory traffic.

The ordinary comparison reports 147,259,392 raw process peak RSS bytes. A separate
tracemalloc run on the same producer reports 82,209,053 peak traced bytes,
800,478 live traced blocks and 352,382,976 raw process peak RSS bytes. Tracing
changes costs; it does not count every GEOS allocation or cumulative allocations.
Both runs' eight saved mesh assets are compared word-for-word with the rendered
geometry producer `bf9d42f77`; extracted helper/report changes preserve geometry.
Cumulative copied bytes, all native allocations and GPU/device memory are
unmeasured. Process peaks are not an application memory budget.

`scale_far_height_profiles.py` plans all original parent forms in physical archive
order, releasing per-source products rather than building the parent mesh:
64 blocks, 8,768,973 native form bytes, 13,218 sources. It supports 13,176 sources /
15,526 parts / 38,050 domains and processes 67,600 ring vertices / 189,286 planar
domain coordinates. Forty-two complete sources delegate: 20 absent forms,
13 incomplete hip plans, six domes and three invalid footprints. All ordinals
and reasons remain in the report. One ordinary observation takes 20.916 s total,
15.026 s planning, with 97,406,976 raw peak RSS bytes. These are actual plan-only
scaling results, **not 64 native reference views or parent candidate mesh costs**.

The focused test run covers 29 tests including existing roof-domain/material
consumers; the final affected run covers nine controls after the unknown-error
metadata check. No oracle/tolerance was weakened. Tests cover independent roof
heights, no early triangulation, winding/closed volume, holes, source fields,
atomic fallback, local rounding collapse, unchanged-original words and category
seams. All nine Python files pass focused Ruff check/format.

## Decision and next hypothesis

Do not port the flat-normal/raised-wall candidate as an admitted replacement.
It is a useful plan-first geometry baseline with complete local delegation,
but its geometric allowance does not preserve the lighting/material response.
The next concrete comparison should retain roof normal/material variation in a
source-owned relightable representation and test its additional work/storage
against this baseline and the complete original. Do not repeat an XY pitch sweep
or per-triangle projective repacking without a new hypothesis: the existing
`column_field.md` and `projective_field.md` already document their failure/costs.
Master owns native production, cache/IO/GPU contracts and world/device admission.

## Sources and reproduction

SIGGRAPH 1996, *Simplification Envelopes*, was read including the paper's bound
assumptions/figures. Its two-sided geometric error motivates separating geometry
from appearance; this probe does not implement its algorithm or claim global
optimality. Local PDF: `doc/references/geometry/siggraph/1996-simplification-envelopes.pdf`.
Existing column/projective/LDI negatives remain the appropriate family controls.

Use `.venv/bin/python`, `PYTHONPATH=lib/src:test/experiments/render`, and fresh
explicit system-temp outputs. Required packages are the existing NumPy,
Shapely/GEOS, Trimesh/Rtree, Pillow and bpy environment; no new dependency.

```
python -B -W error -m unittest test_far_height_profiles test_far_profile_fields
python -B -W error test/experiments/render/compare_far_height_profiles.py \
  --input test/outshine/data/render/building-full-view/cell.json.gz --output /tmp/outshine-profile-check
python -B -W error test/experiments/render/probe_far_profile_fields.py \
  --input /tmp/outshine-profile-check --output /tmp/outshine-profile-fields
python -B -W error test/experiments/render/scale_far_height_profiles.py \
  --input test/outshine/data/render/building-full-view/original-inputs.tar.gz --output /tmp/outshine-profile-scale.json
python -B -W error test/experiments/render/transition_far_height_profiles.py \
  --input test/outshine/data/render/building-full-view/cell.json.gz --source 2511 --output /tmp/outshine-profile-transition
python -B test/experiments/render/render_far_height_profile.py \
  --scene /tmp/outshine-profile-check/static-020/profile/scene.json --lighting night --output /tmp/outshine-profile-night
```

For headless software verification set `LIBGL_ALWAYS_SOFTWARE=1` and
`EGL_PLATFORM=surfaceless`; limit BLAS/OpenMP threads to one. Render verification
uses low priority and does not claim device timings. Rendering omits `-W error`
because the installed bpy warns about its existing Material.use_nodes API.
Private evidence root: `/tmp/outshine-research-building-profiles-8ab3aa077`.
No diagnostic images are published in the repository.
