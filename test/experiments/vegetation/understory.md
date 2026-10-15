# Flowering understory and coherent form patches (#316)

The dense grass studies remained morphologically and spatially uniform. This
experiment adds an own flowering Convallaria-majalis interpretation and compares
short grass, tall grass and broad-leaved herb groups on the same saved roots.
No accepted botanical species, habitat distribution or SpeedTree quality is
claimed. The two grasses are size forms of the same unidentified graminoid,
not two newly accepted species.

## Own organ construction and optical reference

Local first source: Prusinkiewicz/Lindenmayer/Hanan, SIGGRAPH1988 *Developmental
Models of Herbaceous Plants*, `doc/references/vegetation/siggraph/1988-developmental-models-of-plant-architecture.pdf`.
PDF-page5/Fig9 was opened, including its two broad basal leaves and unilateral
pendant inflorescence; §4 describes the raceme, §6 separates surfaces from the
architecture. This figure is itself a procedural model, not an herbarium
photograph or measured morphology dataset. It supports the interpretation;
independent botanical calibration still remains #25.

`LilyStudy` builds two cupped, curved basal blades, an arced stalk and nine
connected pendant six-lobed bells. Dimensions, taper, flower graduation and
plant variation are authored. Existing Blade/axis/frame procedures are reused.
Leaves attach at their petiole centreline; the mean of a cupped cross-section
is not that attachment. Stalks originate on the same exact parent arc and join
the bell-cap anchors. Leaf/stalk/bell IDs are stable independently of meshing.
A saved complete parameter recipe can regenerate selected organs with RNG
forbidden; over-budget requests do not sample a recipe or create geometry.

The first actual near image exposed edge-on, apparently flat leaf bands.
One image-driven revision uses a fixed plant-local fan normal (never a camera
normal), stronger cupping/curvature and own parallel-vein maps. The final whole
image was opened against Fig9: two broad leaves with visible converging ribs,
progressively smaller hanging bells and intact connections are recognizable.
The stalk/flowers remain too regular, buds/season stages, leaf sheaths and
independent botanical dimensions/textures are missing. This is not species
acceptance. The earlier near image remains only as the needed before comparison.

The reusable [Bellflower primitive](../../../lib/bellflower.md) is now in lib/:
regularity/injectivity derivation, analytical jets, oriented thin mesh, open
mouth, hard upper cap and seam-aware corner UVs. Assembly consumes those corner
UVs explicitly while preserving the existing leaf/wood paths. Complete source
prototypes were checked finite, valid-indexed, nonzero-area and correctly
oriented against all source corner normals.

## Materials and stored products

`understory_leaf_maps.py` emits linear RGB and16-bit height256×256 PNGs from
analytic parallel ribs, a stronger centre rib and mild pigment variation. The
actual images and height-scale metadata are cached. Blender uses image
filtering, metallic-roughness plus qualitative thin transmission and a small
bump contribution. No photographs, vendor assets or captured illumination.
There is no measured species BSDF, native normal-variance/temporal-filter or
BSSRDF admission. Renderer input receipts hash the actual maps and meshes.

Eight finite variants per form plus one near herb are built once. The near
herb has6,110 vertices/11,618 triangles; its field product982/1,696 retains
exact parent/organ anchors. Complete near/field surface areas for variant0 are
0.0167942/0.0165927m². This small aggregate area difference is not a silhouette,
flower-detail or general LOD error bound; a measured selector remains open.

## Spatial comparison

GroundCoverPlan supplies9,381 unchanged signed cell/ordinal roots in8×8m at
nominal150/m². A lossless zigzag of the two signed cell coordinates supplies
nonnegative hashing keys; original IDs remain unchanged. Existing competing
Fourier/Gumbel marks receive an explicit saved stand-metre frame,1.8m scale,
16 waves/form and0.92 correlation strength. The older ECEF frame, limits and
recipe encoding remain unchanged. Stand-frame recipes have a separate tag;
mixed coordinate frames are refused. Strength0 skips field work after caller
validation.

Weights(0.05,0.65,0.30) are authored form priors, not calibrated species
abundances or finite quotas. Actual counts are525 herb/5,384 short/3,472 tall.
The paired control permutes entire intrinsic form/variant/yaw/scale packets
among the same roots, preserving their exact counts and geometry budget. It is
an explicitly whole-stand shuffled control, not the partial-replay generator.
Both variants contain1,953,120 instanced triangles and1,228,911 population bytes.
Median nearest distance0.0392583m; same-form neighbour fraction0.471911→0.761433.
Neither statistic is an ecological acceptance target.

The final whole1920×1080 stand images were opened: grouped height/herb patches
replace the shuffled fine-grained mixture. The visual difference is moderate;
large uniform green regions, limited plant variety and missing organic litter
remain. Roots, intrinsic model budget, camera, light and materials are shared.

## Completed costs and checks

Frozen products/renderer `d9f4df9183e5bc4b3d630e64985d7b56328d187f`, terminal0:
whole cache build2.106s wall/1.877CPU-s/87.36MiB RSS under recorded host load.
One streamed/matrix selection pair takes0.096219/0.099863s (see exact receipt), with
bit-exact complete outputs; no meaningful speedup is claimed. Actual parameters,
point/form products and both own texture maps are saved separately. The24 field
prototype arrays total2,459,264 bytes and0.612s measured mesh construction; the
remaining cache-build cost includes near geometry, classification, compression
and image output. Python allocator calls/copy traffic and GPU time are unmeasured.

All images: Blender5.2.2 EEVEE/llvmpipe software,1920×1080, actual shared
camera/daylight/mesh helpers. Hero uses an explicit local fill light.

| Case | Samples | Cached setup s | Render s | Process wall s | CPU-s | Peak RSS MiB |
|---|---:|---:|---:|---:|---:|---:|
| plant | 16 | 0.213 | 84.754 | 86.773 | 87.662 | 2025.29 |
| shuffled | 4 | 0.330 | 41.960 | 44.100 | 49.988 | 2632.21 |
| grouped | 4 | 0.345 | 42.355 | 44.579 | 50.907 | 2600.13 |

Source controls cover corolla jets/regularity, UVs, organ attachments and saved
partial reconstruction, signed IDs, exact partial field replay, matched shuffled
budgets and unchanged legacy fields. A fresh old ECEF replay additionally
reproduces47,284 stored forest choices with constructors/provider work forbidden.
Ruff/format and focused checks are run on the delivery; no native/Place/device
or whole-engine gate is claimed.

Transport's actual Blender custom-normal packing discrepancy is acknowledged
under #291. These images still use that host path; CPU source vectors are
verified, exact decoded shader normals/instancing/bump remain unproven. The
agreed direct CORNER-attribute material path needs a separate actual vegetation
consumer check. No claim that these Blender pictures certify native normal LOD.

## Reproduce and artifacts

Use `.venv` and the installed lib dependencies/bpy5.2.2;
`PYTHONPATH=lib/src:lib/tests:test/experiments/vegetation`,
`OPENBLAS_NUM_THREADS=1`, `OMP_NUM_THREADS=1`, render`LP_NUM_THREADS=1`.

```sh
.venv/bin/python test/experiments/vegetation/understory_experiment.py \
  --output /tmp/outshine-vegetation-understory-products
.venv/bin/python test/experiments/vegetation/render_understory.py \
  --inputs /tmp/outshine-vegetation-understory-products \
  --output /tmp/outshine-vegetation-understory-shots --case plant
```

Repeat render with `--case shuffled` and `--case grouped`. Current inputs,
receipts and geometry checks: `/tmp/outshine-vegetation-understory-products-d9f4df918/`;
current opened images: `/tmp/outshine-vegetation-understory-shots-d9f4df918/`.
The cache builder originally wrote the same product directory with suffix57f896d33;
it was renamed to its actual frozen source commit, with files unchanged.
The necessary earlier near comparison remains in
`/tmp/outshine-vegetation-understory-shots-d333885fa/`.
Research images stay local; no images under doc/ or replacement gallery.
