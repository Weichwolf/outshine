# Physical dense grass and bounded root plans (#304)

The flat tiles showed isolated tufts. Each actual three-segment12-blade tuft
has about0.0033m² summed top-projected area before overlap. At65cm root spacing,
a dense grass surface is geometrically impossible. Increased root density is
therefore necessary, rather than a ground-colour or canopy-envelope patch.

## Method and measured result

The bounded kernel is now [GroundCoverPlan](../../../lib/ground_cover.md).
Poisson roots replace the lattice without changing physical tuft models.
The experiment compares the initial jittered raster, the same nominal sparse
Poisson density, then authored25/75/150/300 roots/m². The first process pair has
36 versus44 actual roots and differing marks; it is a descriptive distribution
comparison, not an equal-geometry superiority claim. Main gain comes from a
physically appropriate density. No ecology or universal grass density is claimed.

Eight shared12-blade models retain the existing analytical folded/curved Blade,
three longitudinal segments, source variant seeds17..24, random yaw and
uniform0.85..1.15 scale. No image maps, closed envelopes or enlarged blades.
The CPU coverage reference expands these same shapes; Blender keeps prototypes
and root instances. Nominal root query is4×4m, with natural blade overhang.

Binary two-sided leaf union is measured inside the independently projected
4×4m ground polygon at256×256 and512×512. Directions: top and(14,-19,11.7).
No depth, light/shadow or global angular guarantee follows from this union.

Frozen CPU study `a17bb0918338e00a630e45da9a8d44f45ffe762a`, terminal0:
22.469s wall/22.179CPU-s/242.19MiB peak RSS. Host load1.31/1.99/2.37→
1.62/2.00/2.36; numerical libraries use one thread.

| Process / density m⁻² | Actual roots | Triangles | Top union % | Oblique union % | Root build ms |
|---|---:|---:|---:|---:|---:|
| legacy-raster / old65cm | 36 | 4,320 | 0.62 | 3.85 | 0.672 |
| poisson / 2.3668639053254434 | 44 | 5,280 | 0.70 | 4.63 | 1.493 |
| poisson / 25 | 396 | 47,520 | 6.34 | 32.03 | 1.594 |
| poisson / 75 | 1,179 | 141,480 | 17.75 | 68.54 | 1.759 |
| poisson / 150 | 2,368 | 284,160 | 32.64 | 88.68 | 1.860 |
| poisson / 300 | 4,757 | 570,840 | 54.62 | 97.10 | 2.440 |

Four alternating equal-output root benchmarks on78,518 roots: median batch
0.061834s versus scalar1.411810s, identical IDs/positions/marks. The scalar
reference draws the same five uniforms per cell ordinal; the batch avoids the
per-root Python loop. Both return5,025,152 bytes; this is not a GPU/frame result.

## Actual optical check and consumer

Frozen preview `25e2ef923bc1890df01b03f37cf0d9aad531fbc4` actually rendered and
opened whole at1920×1080/four samples. Left→right: old raster,75,150,300 roots/m².
Shared light/materials/orthographic camera; Blender5.2.2 EEVEE/llvmpipe software.
Terminal0,36.549s wall/41.665CPU-s/3071.86MiB peak RSS, setup0.280s,
render34.378s. EGL context-fallback diagnostics precede the successful image.

The density gain is visible.150 gives a mostly closed oblique grass surface;
300 adds modest oblique coverage at twice the geometry. Top views still expose
large gaps, and this homogeneous process lacks larger-scale species/patch
variation. The screenshot is a density proof, not accepted steppe/turf botany.

Four-tile recipes now use75 roots/m² for forests/heath and150 for Steppe.
Original wood/shrub roots, species and poses remain exactly unchanged via the
comparison population; its small legacy grass layout is still constructed only
to preserve that experiment's original coupled woody RNG stream. The reusable
kernel has no such dependency. Steppe/heath keep their previous blade scale
ratios. These are authored initial choices, not species/latitude calibration.

Whole Steppe source `4a2887e531b72a25d6478c82189782f96363ef71` rendered and
opened before/after at1920×1080/four samples. Same physical tuft assets, plant
scale profile, material and camera; shared camera/daylight helpers from #291
now replace duplicate code with the agreed identical values. Process terminal0:
57.072s wall/73.431CPU-s/2966.96MiB peak RSS; setup0.193s/render54.817s.
There are29,198 roots/3,503,760 instanced triangles. The sparse sandy field
becomes a largely closed dry grass surface, with actual edge blades/shadows.
It remains too homogeneous and lacks seed heads, herbs and species mixtures.
No closed-mesh or botanical/native/LOD acceptance follows from a successful image.
Inputs/PNG/receipts: `/tmp/outshine-vegetation-grass-steppe-4a2887e53/`.

The entire Laubwald before/after was also opened at1920×1080/four samples,
same4a2887e53 source and completely unchanged original30 tree placements and
physical tree meshes. There are58,284 grass roots/29,248,200 total instanced triangles.
A connected grass layer replaces isolated tufts, including beneath the crowns;
shading remains responsive to actual foliage. Still no herbs/leaf litter,
large-scale patches or screened geometry selection.
Terminal0:161.608s wall/205.025CPU-s/3514.41MiB peak RSS; setup10.819s,
render148.720s. Load2.42/1.98/1.99→2.60/2.27/2.10. These are costly offline full
physical references, not a frame/device/LOD budget pass. Current whole forest:
`/tmp/outshine-vegetation-grass-forest-4a2887e53/`.

The built wheel SHA256 is
`18f8aecfc5cd82baaaf4cc84ad460cba9f6a45be37139d9a0abd2c1bb0af2cc8`.
A separate `.venv` imports the installed package, reproduces8,744 saved roots
exactly through the public API and loads the same arrays with RNG/generation
forbidden. Nine ground-cover/graminoid controls pass there. Source controls also
check that the new grass density cannot alter any original woody pose.
Wheel/check outputs remain in `/tmp/outshine-vegetation-ground-wheel-3bbb2b9e2/`.

 Final shared-mesh consumer `2941db4464b37c97f31945575ca308887004c1e9`
actually rendered/opened the same whole Steppe with all three shared helpers:
camera, daylight and mesh. Terminal0:53.118s wall/68.618CPU-s/2960.87MiB RSS;
setup0.177s/render51.078s. Every pixel is identical to the earlier4a2887e53
Steppe; the superseded duplicate PNG was removed. Current PNG/receipts and exact
pixel/hash comparison are in `/tmp/outshine-vegetation-grass-shared-2941db446/`.
26 source controls pass without skip, including actual Blender cameras, daylight
and mesh import;9 controls pass from the fresh wheel. Focused Ruff/format,
diff check and AGENTS-only `make lint-docs` pass. The touched existing public
export module retains its same two pre-existing Ruff findings (TRY004/RUF022),
independently reproduced on current master; no full-lint/native gate is claimed.
Research images remain local; only master publishes integrated Places.

## Local references

First: Deussen etal., SIGGRAPH1998 *Realistic Modeling and Rendering of Plant
Ecosystems*, local `doc/references/vegetation/siggraph/1998-plant-ecosystems.pdf`.
PDF page5 was opened again: population density descriptions are distinct from
individual plants. Approximate instancing in §6 motivates shared form products;
no competition simulator or vector quantizer is implemented by this point plan.

The installed SpeedTree10.2 Games/Grass.spm was inspected without importing its
assets. Its stored graph is Tree→Branch→Frond with six Branch and six Frond
nodes, stored48 triangles. That is a bundled template statistic, not runtime
performance or a real grassland density. File SHA256:
`bdf1c1bb82defef97380ba6bfd973baf2b6e5122945ac2640eb867fc7d1b69f8`.
Existing local Ghidra parent/template and graph-terminal findings (#24/#295)
support the hierarchy/instance interpretation; this turn adds no claim about
SpeedTree's world-distribution code. No vendor geometry, code or texture enters
our implementation. Our exact signed cell/ordinal IDs and separate PCG streams
are independently defined and tested.

## Reproduce

Use `.venv`, the installed `lib/` dependencies and bpy5.2.2. Set
`PYTHONPATH=lib/src:lib/tests:test/experiments/vegetation`,
`OPENBLAS_NUM_THREADS=1`, `OMP_NUM_THREADS=1`; render with `LP_NUM_THREADS=1`.

```sh
.venv/bin/python -m unittest test_ground_cover test_graminoid biome_tile_checks broadleaf_crown_checks
.venv/bin/python test/experiments/vegetation/ground_cover_experiment.py \
  --output /tmp/outshine-vegetation-grass-comparison
.venv/bin/python test/experiments/vegetation/ground_cover_preview.py \
  --inputs /tmp/outshine-vegetation-grass-comparison \
  --output /tmp/outshine-vegetation-grass-optical
```

Current frozen data/receipts: `/tmp/outshine-vegetation-grass-comparison-a17bb0918/`;
actual whole comparison PNG/receipt/cost in
`/tmp/outshine-vegetation-grass-optical-13f5d95ee/` (receipt identifies the repaired
renderer freeze25e2ef923). Post-promotion products are checked exactly against
these saved CPU arrays; no second full coverage sweep is needed for a module move.
