# Nested spatial fern study (#339)

The current understory lacks fern crowns. This experiment composes two levels
of pinnae into seven curved fronds and a complete plant. It consumes the public
PinnatePattern/Blade/FrondGuide procedures; it does not copy SpeedTree geometry
or textures. The plant is a generic authored fern, with no accepted scientific
species label. Botanical calibration remains #25.

## Reference and prepared hierarchy

[Source decision](https://github.com/Weichwolf/outshine/issues/339#issuecomment-6200847380):
local SIGGRAPH2003 course PDF-page63/Algorithm4/Fig7–8, including the Pellaea
photograph, opened; existing targeted Ghidra FrondFrame/Skin exports read;
SpeedTree fern colour and separate opacity images opened. Their positional
organ design, guide binding and qualitative twice-divided shape support this
own interpretation. Pellaea in the paper is not a botanical reference species
for this twice-divided plant. No new headless run or full vendor-field binding
is claimed in this increment.

`fern_plan.prepare` saves actual primary and secondary placement tables, local
shapes, guide profiles and root frames. Cold preparation samples variation once;
meshing saved parameters does not call any random constructor. Coordinates are
metres, organ-local X transverse/Y forward/Z normal, world Z up. Uniform scales
and rigid frames retain normals. IDs prefix(frond,pinna,pinnule); UINT32_MAX
marks the respective rachis, with request(frond,−1) for the main rachis.

Canonical pinnae have22 lateral pinnules plus a tip. Each frond has18 lateral
pinnae plus a tip, giving133 pinnae/3,059 leaf surfaces in the plant. All seven
fronds reuse the same canonical pinna source, with stored scale/position/frame.
Main and secondary rachises use closed analytic axis tubes. Leaves remain thin
surfaces; matching attachment points are not a welded whole-plant manifold.

The primary placement scale is bounded before mesh detail: for source lateral
extent e, parent origin y and direction components(cosθ,sinθ), scale is at most
`(L-y)/(cosθ+e*abs(sinθ))` and, when the denominator is positive,
`y/(e*abs(sinθ))`. The saved primary frames are XY rotations; source longitudinal
coordinates lie in0..1. This keeps the complete source within the parent guide.
Existing FrondGuide regularity and all face/corner orientations are checked.
It does not prove absence of inter-organ collisions.

Budgets precede prototypes/guide work. Partial requests skip unneeded guides;
the complete and selected assemblies use an explicit three-term transform order
and retain every UV, face/material and hierarchy field exactly. Source arrays
are not aliased. These are trusted experimental recipes/NPZs, not a new public
untrusted decoder or native cache ABI. Whole products and source pinnae are
cached; tile instances load those products without repeating plant generation.

## Actual image-driven revision and detail comparison

The first whole1920×1080 image exposed a sparse branch skeleton: only six pairs
of narrow pinnules per pinna, too much open space. One form revision increases
their number and width. Camera, shared daylight, material, ground and parent
frond arrangement are unchanged. The revised whole image was opened against
the initial image and local masks: broad feather-like pinnae now form a coherent
crown. Repeated smooth pointed pinnules, simple edges and orderly fronds remain
visible limitations; this is not SpeedTree or botanical acceptance.

An initial two-segment field candidate lost13.7% whole surface area on the sparse
form and was rejected before stand presentation. A controlled2/3/4/6 comparison
retains four segments. The current denser variant0 has0.646973m² near versus
0.624479m² field, a3.48% difference including rachises. Equal outer bounds and
hierarchical identities do not establish projected coverage or a general LOD
error. The current products are candidates, without an accepted screen selector.

## Completed construction costs

Current builder/render sources: c379acc6c. Full eight fern products/four prepared
plans, eight grass prototypes, two populations, own maps, compression and
comparison:6.832s wall/6.825CPU-s/154.43MiB peak RSS, terminal0 with recorded load.

| Variant0 | Vertices | Triangles | Array payload bytes | Construction s |
|---|---:|---:|---:|---:|
| near | 66,633 | 83,762 | 13,539,134 | 0.15695 |
| field | 42,637 | 48,006 | 7,919,730 | 0.09287 |

Six alternating normal-priority assembly calls on the same complete plant and
arrays: batch and per-pinna deformation are byte-exact, including normals.
Median139.18ms batch versus230.44ms individual, with complete output equality.
Guide evaluations
fall from140 to7, with the same133 canonical pinna placements and triangle count.
Separate untimed tracemalloc peaks31,905,975/31,930,435 bytes: no substantial peak
reduction claimed. Retained13,542,922/13,542,118 bytes include the13,539,134-byte
output. Known output/placement counts are not all allocator calls or copy traffic.
The measured process RSS includes imports and all construction/measurement work.

The4×4m flat population contains16 fern roots at authored nominal0.85/m² and
2,368 grass roots at150/m². Source root IDs, transforms and variant choices are
saved. Four field fern variants and eight grass prototypes yield1,052,256
instanced triangles; no per-root whole-plant generation. Neither density nor
these four forms are a calibrated habitat/species distribution.

## Materials and actual normal controls

Own256×256 linear-colour and16-bit physical height images describe a central
rib, diagonal side veins and mild pigment variation. Metallic-roughness plus
qualitative12% thin transmission; height scale60µm, preview bump strength0.25.
No photographs/vendor pixels or baked illumination. Species BSDF, general normal
variance/mip/temporal filtering and native material calibration remain open.

Fern materials attach original Float32 vectors as FLOAT_VECTOR/CORNER data and
use Object→World NORMAL (inverse transpose)→Normalize→Bump.Normal→Principled/Thin.
Shared camera/mesh/daylight helpers remain unchanged. Grass retains its existing
local material path. This avoids relying on Blender's packed custom normals.

Actual EEVEE control on04905721c: three64×64 planar GeometryNodes instances
(uniform, nonuniform, nonuniform with analytic UV-height gradient) against an
independently computed world normal. A fourth deliberately omits the transform
and must fail. Initial direct colour readback was quantized despite32-bit EXR;
that counterexample remains. The actual control amplifies the shader residual
by512 before readback; the original2e−5 vector-component threshold is unchanged.
Correct cases differ by≤4.76838e−7, missing-transform control by0.4921875.
All four expected outcomes pass, terminal0,15.276s wall/15.878CPU-s/1632.52MiB.
This proves these actual plane/instance/bump cases, not every curved, filtered,
transmitted or native shader normal. No shared rendering API is changed.

## Checks, whole images and reproduction

Four independent new controls plus existing frond/shape/spatial controls pass19
source tests and19 consumer tests against the previously installed wheel from
PR #329. No lib/dependency changes or new wheel build claimed. A fresh saved-plan
replay with RNG construction forbidden reproduces all eight complete fern and
eight source-pinna products byte-exactly; near/field hierarchy sets are exact.
The prior sparse near product also replays exactly after default recipe changes.

Three final whole images were opened: near crown, field crown at the identical
camera, and the dense4×4m fern/grass tile. Field pinnae remain recognizable in
this view; the tile shows a clear broad fern layer above the narrow grass.
Smooth repeated pinna shapes, orderly crowns, authored hues and lack of litter
remain visible. No general multi-view/temporal/coverage acceptance follows.
`test/scripts/pixels.py` near/field:10.6227% pixels differ, mean absolute RGB
channel error0.246250/255 and maximum82/255. Shadows/lighting are included;
the whole-frame mean is not a foreground silhouette or LOD bound.

| Image | Samples | Setup s | Render s | Process wall s | CPU-s | Peak RSS MiB |
|---|---:|---:|---:|---:|---:|---:|
| plant | 16 | 0.654 | 95.382 | 97.797 | 98.656 | 2073.30 |
| field | 16 | 0.442 | 95.772 | 97.976 | 98.696 | 2034.16 |
| tile | 4 | 1.284 | 44.001 | 47.097 | 50.221 | 2297.71 |

All three commands terminate0. Renderer/material helpers and vegetation lib
remain byte-identical to the frozen picture sources after the master merge.
All images Blender5.2.2 EEVEE/llvmpipe software,1920×1080. No GPU/device/frame or
native/Place gate is inferred. Research images stay local; no gallery/doc images.

Use `.venv`, installed lib dependencies and bpy5.2.2;
`PYTHONPATH=lib/src:lib/tests:test/experiments/vegetation`,
`OPENBLAS_NUM_THREADS=1`, `OMP_NUM_THREADS=1`, render`LP_NUM_THREADS=1`.

```sh
.venv/bin/python test/experiments/vegetation/fern_experiment.py \
  --output /tmp/outshine-vegetation-fern-products
.venv/bin/python test/experiments/vegetation/render_fern.py \
  --inputs /tmp/outshine-vegetation-fern-products \
  --output /tmp/outshine-vegetation-fern-shots --case plant
```

Repeat with case field and tile. Current products/receipts:
`/tmp/outshine-vegetation-fern-products-c379acc6c/`; current images/costs:
`/tmp/outshine-vegetation-fern-shots-c379acc6c/{plant,field,tile}/`.
Direct-normal controls: `/tmp/outshine-vegetation-fern-normal-04905721c/`;
initial readback counterexample: `/tmp/outshine-vegetation-fern-normal-507c4604a/`.
Needed initial form before image: `/tmp/outshine-vegetation-fern-shots-43c89f6f6/plant/plant.png`.
