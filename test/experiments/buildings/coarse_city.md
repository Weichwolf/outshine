# Coarse urban evidence becomes shared relightable city fields (#1105)

This prototype supplies the coarse urban input needed by #1102. It stores
an inferred city field before any diagnostic mesh, and compares economical
procedural box instances with exact coplanar masked quads of the same recipe.
Neither the inferred city nor the mask renderer is an admitted native world path.

## Source evidence and what is inferred

The fixed normalized input retains provider geometry, original feature IDs,
properties, tile/layer/ordinal, and raw URI/payload hashes. It was recaptured
byte-for-byte from explicit cached receipts using `capture_urban_sources.py`;
no source was refreshed and no native mesh export was needed. The original
provider bytes remain separate from the enriched product and the test snapshot.

The versioned normalized snapshot is 113,823 B, SHA256
5bba30afef6d2e685787039eeebca5d8dae876d575fcd6a6b47ef3fae6fb91b7.
Its 395 source records retain relevant owner features and the added urban prior.

The owner is z11/1818/806, pinned OpenFreeMap revision 20260927_080001_pt,
155,990 B / SHA256 d1f2e00615b3f239eecd28bbfa2d22d13f95792d8e400d86495d56d49f672918.
It contains 179 landuse, 137 transportation, 31 water, 7 landcover, 1 park and 39 waterway
features but no building layer. This host's owner differs from master's recorded
156,224 B payload at the same URI; cross-host native byte equality is not claimed.

The z11 urban polygons cover about 52.8 million of 382.9 million projected square
metres in this owner. A z8 parent did not materially extend that urban evidence.
The final additional input is z5/28/12, 210,323 B / SHA256
5359a688cdb91df5f6e8f4d043abcd653a62dc6937edfc6966fd5048d9fdd66a.
Its residential multifootprint 3495310 covers this owner; it supplies a coarse
urban prior while the owner supplies finer open-space/transport constraints.
This is evidence of regional urban extent, not measured building geometry or
precise urban boundaries. The provider's coarse/fine zooms are not interchangeable.

Only the parent's urban landuse layer is added. Finer explicit water, park,
landcover and transport evidence supplies exclusions. Major line widths are
inferred clearances; underground hints do not clear the surface. Context geometry
is clipped to the owner plus a guard before union; originals and their reasons
remain in source records. Unsupported classes/invalid data stay explicit.
A region without urban evidence or without fitting generated lots never becomes
`World Empty`. Other world classes, original sources and functional assets remain
separate responsibilities.

The output is a render-only mass prior: global deterministic projected cells,
not individually reconstructed buildings. This uniform lattice is visibly artificial
in the close diagnostic views; no actual detailed-city plausibility is established.
The canonical Web-Mercator pitch is 32 m,
patch side 8, inferred footprint fraction 0.75, vacancy prior 0.15. Accepted cells
receive heights 6/12/24/48 m with probabilities 0.62/0.28/0.08/0.02. These values are
procedural assumptions, not provider measurements. The latitude cosine converts
the flat study to local metres; spacing varies globally. Terrain, Earth curvature,
metric/neighbor/LOD continuity and comparison to actual detailed-city imagery
remain unqualified. A mass may represent several unseen small buildings.

## Complete stored product and consumers

The all-owner result contains 247,731 inferred masses in 5,962 patches. Its seven
arrays occupy 630,132 B: integer patch keys, categorical height tiers, explicit
height values, frame/footprint parameters, outward-rounded Float32 patch bounds
and source membership CSR. The serialized provenance/diagnostic manifest is
175,457 B and compressed NPZ 159,967 B in this run; parsed metadata/library/resource
memory is additional. The point reader loads the NPZ only. These are not native
GPU allocation or application-memory figures.

A cache hit needs neither MVT nor GEOS nor source processing. A fresh blocked-
import reader/query test verifies this. The reader requires the seven canonical
array dtypes and bounded associated shapes, finite ordered bounds and height values,
valid frame parameters, Uint8 tier indices and monotonic Uint32 source CSR starting at zero and ending
at the index count. Invalid signed/fractional tiers reject before indexing;
negative Int8 tiers previously selected the last height (48 m in the real case).
Eighteen focused source/geometry/adapter tests now include these boundary cases.
All seven valid owner arrays remain byte-identical to the prior reader, so the
existing geometry/image evidence is retained; no new image behavior is claimed. The same stored fields drive both the
box and mask paths; a best procedural box opponent can also consume these fields
with a shared unit template. Do not compare expanded box diagnostic vertex bytes
against mask fields as a purported asset-memory victory.

For an N×N patch, west/east faces share 2N planes and north/south faces share 2N planes.
Occupied roof tiers and a floor add at most K+1 planes. Continuous masks test local
footprint phase, categorical field height and hit height. They preserve gaps and
disocclusion of the generated recipe; there is no fitted depth-layer approximation
or baked lighting. This grouping is exact only for this controlled geometry family.
Own grid hit IDs and source-region membership are not native original Face IDs.

The diagnostic shader shares one unlit nearest-height atlas and material across
all selected patches (65,536 B of Float32 RGBA image payload in this 64-patch
study, separate from the stored arrays; actual native GPU allocation is unknown).
Its patch ID is categorical: recover the known integer
before atlas addressing in Blender; native bindings require an explicit flat
integer rather than an interpolated continuous coordinate. A retained float-
address counterexample is wrong; rounding restores pixel-identical output to
the original separate-material implementation. Plane kind/height and continuous
coordinates have distinct roles. Native filtering/opacity/TAA/shadow contracts
still require their own qualification.

## Correctness, images and work

Ten focused tests cover independent triangulated opaque geometry, exact mask/box
depth/own-ID/normal comparisons, holes/floor/parallel/tie/history cases, source
absence versus unknown, explicit water, parent urban priors retaining child
exclusions, deterministic fields, pure ready loading, atlas packing and projected
work. The chunk acceptance path also matches individual GEOS predicates.

All 11,520 region-focused rays across front/reverse/elevated/8 m movement/far views
return equal depth/own-ID/face/normal arrays. They sample a bounded 64-patch study
containing 2,795 masses; they are not world-wide coverage or a continuous-motion
bound. The all-owner stored bake is independent of these cameras. Diagnostic
meshes are built only after the product is saved and reloaded. Model geometry,
not native detailed-building truth, is the independent opaque opponent.

The lit matrix uses 1280×720, 32-sample software Eevee, five poses and day/backlight/
night, with common neutral metallic-roughness parameters, light, camera and ground.
Images and actual inspection are recorded in the local receipt. Numerical point
equality does not prove raster/alpha/filter equality. Image results are retained
with changed-region statistics rather than hiding a distant city in sky averages.

All 30 renders finished with exit 0 at frozen code producer
`a98eabfc5347ca7eaec6e2dd6dbdc47a9cf65989`; all three complete comparison
sheets were actually inspected. Camera/light/product/profile/hash agreement is
checked per pair. The night control adds one shared local area light; this is
not a model of actual urban emission or general night lighting.

| Pose | Day | Backlight | Night |
| --- | ---: | ---: | ---: |
| front | 9 / 8,817 / 1,432 | 10 / 9,124 / 1,557 | 6 / 3,916 / 26 |
| reverse | 9 / 11,659 / 2,192 | 12 / 11,818 / 2,094 | 7 / 4,862 / 83 |
| elevated | 14 / 16,003 / 3,779 | 15 / 17,955 / 4,663 | 12 / 7,624 / 150 |
| moving | 13 / 8,904 / 1,505 | 13 / 9,340 / 1,638 | 7 / 3,970 / 42 |
| far | 20 / 528 / 113 | 19 / 576 / 186 | 5 / 233 / 23 |

Each cell is **maximum RGB8 channel difference / changed pixels / pixels with
a channel difference above 1**. There are 921,600 pixels per image. In the far
views the changed-region mean absolute channel differences are 1.31 day,
1.36 backlight and 0.64 night; their changed bounds cover only about 154×22 pixels.
`test/scripts/pixels.py` independently confirms the far-day result. The images
show close recipe coverage and lighting, with finite raster/shadow/edge differences.
They are not bit-equal or an actual detailed-city comparison.

For the finite front→8 m move, compare the two paths’ RGB frame changes,
`abs((mask_moving-mask_front)-(box_moving-box_front))`. Maximum differences are
13/13/7 for day/back/night, with 2,836/3,130/86 pixels above one channel level.
This is a two-pose diagnostic, not continuous-motion/TAA stability or an error bound.

The study uses 33,540 opaque-box triangles versus 4,682 masked-plane triangles.
After frustum clipping and front-face culling, the mask quads sum 7.69–9.49 times
the projected geometric pixel area of the box faces, before depth/alpha/occlusion
rejection. This is a real potential fragment-cost disadvantage, not measured
shader invocations. Vertex count alone does not select the native winner.

## Complete cost controls and decision

Intel i5-1135G7 / Linux x86_64, Python 3.13.5, existing environment, one BLAS/OpenMP thread, ordinary
priority under shared development load. Three fresh processes per path alternate
order. Acquisition/MVT decoding are outside the normalized-snapshot bake timing;
ready NPZ load and operation timing are separate. A separate trace is not included
in ordinary medians. Exact per-process commands and hashes are retained.

The old own sampler from 06479b4b1 tests 317,170 individual footprint geometries.
The new conservative gate first tests 6,006 whole patches and accepts 56,711 lots
without constructing their footprints; 260,459 individual cases remain. An entire
accepted patch bounds every possible contained footprint. Outward Double margins
make uncertain boundary cases fall back to the original predicate. All seven
all-owner product arrays are byte-identical to the immutable old sampler against
the same current source adapter/input, including dtype and shape.

| Complete operation median | Old/control | New/alternative |
| --- | ---: | ---: |
| Normalized-source bake, s | 15.2023 | 14.0420 |
| 11,520-ray ready query, s | 1.2196 boxes | 1.0497 masks |
| Ready data load, ms | 10.90 boxes | 11.25 masks |

The bake saves about 7.63% local wall time and avoids about 16.0% of total created
macro/footprint rectangles, without changing output fields. It still examines
373,321 candidate cells. Query work includes 737,280 patch-box/ray tests in either
path; boxes add 589,445 box tests, masks 510,157 plane tests and 234,754 mask queries.
The query arrays occupy 472,320 B. These are explicit work/array counters, not
physical DRAM traffic, every allocation/copy or a native GPU work model.

Ordinary bake process RSS ranges 144.5–144.9 MB old and 144.8–146.3 MB new. Separate
traces peak 35,072,871 B versus 39,843,200 B traced, raw RSS 157,134,848 B versus
156,758,016 B; the extra grouping is not a total-memory improvement. Query traced
peaks 4,105,335 B / 5,106,583 B and raw RSS 40,189,952 B / 43,028,480 B are likewise kept
separate. Interpreter/libraries, parsed input/metadata and transient arrays count;
GEOS/native-library allocations are not fully captured by tracemalloc.

A separate serialization check under concurrent rendering/DSP load writes the NPZ
and full manifest in 95–123 ms and reloads both from the OS page cache in 9.4–14.2 ms.
It does not measure fsync/durability or native DB/end-to-end startup. Metadata is
not included in the point-only load timing above.

Retain the measured exact broad gate for this source case. Supply both render
consumers; masks are not a default native speed upgrade. The useful integration
input is the camera-independent enriched field derived from available coarse
sources before individual Fine geometry. Source/terrain/coverage uncertainty,
GPU field/index layout, filtering, caster/reflection behavior, demand/residency,
LOD transitions, native full-scene cost and device p99 remain open at #1083/#1102.

## Same stored field in the existing native box control

Master e55ce64d7 adds a model-space instanced-box control with a bounded `--field`
input. The shared fixed test input `fixtures/coarse_city_native.field` contains
exactly the same 64 selected patches / 2,795 occupied cells as the lit study.
Its 72×72 rectangle pads unselected patches with zero for this diagnostic subset;
those zeros say nothing about world emptiness. The text is 11,477 B and its decoded
Float32 grid 20,736 B. These are control input sizes, not native allocation peaks.
The adjacent `.reference.json` pins input/source/consumer hashes and coordinates.

`export_urban_patches.py` reproduces this input directly from a prepared product
and manifest, without source/GEOS/mesh work. It preserves occupancy and exact
Float32 height values; oversized rectangles, duplicate cells or lossy heights
reject the entire export instead of silently truncating. Five adapter tests check
negative/global coordinates, unselected gaps, explicit axis mapping, rejection,
fixed input/source pins and a fresh blocked-import ready export. Normal tests
never regenerate the reference input or its pin.

Native X is east/columns, +Y is up/height and +Z is south/rows, matching master's
ENU→East/Up/South contract at #1102/6333851856. The adapter reverses the stored
north-increasing rows and uses the rectangle's north-west corner as its origin.
The receipt's affine map is a proper rotation with determinant +1. The earlier
e4bc08e50 experimental reflected input is superseded; its test assumption was
changed explicitly to the established native frame. Native unit-box winding is
constructed independently. Comparing
every stored independent opaque box bound against this documented native unit-box
construction gives 2,795 matched boxes, exact heights and maximum Double corner
deviation 1.82e-12 m. This checks content and coordinates, not executed native
Float32/shader geometry. Native camera/light are independent; cross-renderer
raster equality and device performance are not asserted. The original 30-render
producer remains the recorded image evidence. The later reader boundary correction
returns byte-identical valid city fields; generator, masks and geometry are unchanged.

The native owner can feed the fixed input directly to the existing driver, with
its required native archive, compilation database and shaders, without rebuilding
the source study:

```sh
python3 test/experiments/render/native_urban_field.py \
  --field test/experiments/buildings/fixtures/coarse_city_native.field \
  --output /tmp/outshine-urban-control-source
```

For another prepared study, use the existing `.venv` and fresh temp output:

```sh
python -B -W error test/experiments/buildings/export_urban_patches.py \
  --prepared /tmp/outshine-urban-study --output /tmp/outshine-urban-native-input
python -B -W error -m unittest test_export_urban_patches
```

## Python mask semantics for the native comparison

The native resource/material/binding implementation is owned and agreed by master
and engine. These are the mathematical semantics of the supplied Python reference;
the contract adds no binding numbers or native asset format.

Let P be pitch, f the footprint fraction and m=(1-f)/2. In native local X/Z,
u=(X-originX)/P, v=(Z-originZ)/P, c=floor(u), r=floor(v), with phases fract(u/v).
H(r,c) is the stored, unlit physical height; zero is an absent mass in this
**diagnostic field**. Read discrete cells without bilinear filtering or mip mixing.
Patch/plane kind, roof height and fixed wall cell indices are categorical/flat.
Do not interpolate a patch ID or derive a fixed wall's cell from a rounded vertex.

| Face | Plane and outward normal | Keep fragment when |
| --- | --- | --- |
| X wall | X=(c+m)P or (c+1-m)P, normal -X/+X | H(r,c)>0, m≤fract(v)≤1-m, 0≤Y≤H(r,c) |
| Z wall | Z=(r+m)P or (r+1-m)P, normal -Z/+Z | H(r,c)>0, m≤fract(u)≤1-m, 0≤Y≤H(r,c) |
| Roof | Y=h for each occupied height tier, normal +Y | H(r,c)=h>0 and both footprint phases inside |
| Floor | Y=0, normal -Y | H(r,c)>0 and both footprint phases inside |

Positions above are relative to the patch origin. X walls carry their fixed column
as a flat descriptor; Z walls carry their fixed row. The other cell index comes
from the fragment. Lookup outside the patch discards. An N×N nonempty patch uses
4N+K+1 quads with K occupied roof tiers; completely empty patches need no planes.
The source case uses N=8 and f=0.75. This reproduces the recipe's closed solids;
raster edge/AA conventions still need native qualification.

This is binary geometric cutout, independent of lighting. Camera/depth/caster
passes must use the same field and predicate. The common ground remains separate.
Use the same existing native MR parameters, camera, lights and transforms for both
box and mask paths; the diagnostic Blender material is not a new native look.
The stored field remains valid during movement/light changes. Original sources
and functional/near assets stay separate; no Fine building generation precedes
this field. Native measurement pays complete decode/upload/residency, projected
area, shading and shadow costs; fewer triangles alone do not choose the winner.

## Sources and reproduction

SIGGRAPH 2003 Decoret et al., *Billboard Clouds for Extreme Model Simplification*,
[existing paper](../../../doc/references/vegetation/siggraph/2003-billboard-clouds.pdf),
pages 1–2 / Figure 1 / equation 1 read and visually inspected: planar
geometry plus transparent maps. This does not implement its greedy plane-space
optimizer. SIGGRAPH 2006 Mueller et al.,
[*Procedural Modeling of Buildings*](../../../doc/references/buildings/siggraph/2006-procedural-modeling-of-buildings.pdf), page 3 /
Figure 4 / scope/repeat/mass-model rules read and visually inspected: mass products
before detailed structure. No CGA/Embree/third-party implementation is copied.
OpenMapTiles landuse semantics are pinned in the
[existing schema reference](../../../doc/references/data/osm/openmaptiles-landuse-schema.yaml);
provider fixture attribution/URIs/hashes stay with the input. No photos/textures.

Use `PYTHONPATH=lib/src:test/experiments/buildings`, the existing `.venv`, one BLAS/
OpenMP thread and explicit fresh system-temp outputs. Non-bpy commands use `-B -W error`; bpy uses `-B` because its installed API emits a known deprecation warning.

```
python -B -W error -m unittest test_urban_patches
python -B -W error test/experiments/buildings/prepare_urban_patches.py \
  --input test/experiments/buildings/fixtures/coarse_city_source.json.gz \
  --output /tmp/outshine-urban-study
python -B test/experiments/buildings/render_urban_patches.py \
  --scene /tmp/outshine-urban-study/front/masks.json --lighting day \
  --output /tmp/outshine-urban-lit
```

`capture_urban_sources.py` accepts an explicit owner receipt and urban-parent
receipts, verifies cached raw hashes and emits a fresh deterministic snapshot.
It does not fetch or update a source. `bench_urban_patches.py` measures `bake` with
`--input`, optional own `--reference` source, or `query` with `--product/--rays/--mode`;
`--trace` is a separate run. The immutable own baseline can be extracted with
`git show 06479b4b1:test/experiments/buildings/urban_patch_bake.py`. The final
source adapter/input is deliberately shared for fair sampler comparisons.
Software images use `LIBGL_ALWAYS_SOFTWARE=1 EGL_PLATFORM=surfaceless` and low
priority. No new packages or native implementation. Local evidence:
`/tmp/outshine-research-coarse-city-5a229fc57/`; images stay local.
