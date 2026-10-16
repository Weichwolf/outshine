# Qualify the grass shadow reference before changing transport — #473

The isolated EEVEE leaf adapter from #468 passes a single sheet but fails an
opaque neighbour: a black blocker does not suppress its back response. That
observation does not demonstrate missing multiple scattering in the Python
model. A separate ordinary diffuse control also exposed a camera-range error;
its correction improves real ground shadows without removing source geometry.
No leaf R/T, mesh, light energy, library/native API or cached product changes.

## Source and controlled geometry

[Bruneton/Neyret, Real-time Realistic Rendering and Lighting of Forests](../../../doc/references/vegetation/eg/2012-real-time-forests.pdf),
page3, was opened optically: view/light overlap and conditional illuminated
fractions matter. This motivates separating geometry, local sheet response and
neighbour transport. The existing SIGGRAPH2014 Embree and EGSR2007 leaf references
and local backend calibration are described in [grass_direct_light.md](grass_direct_light.md).
No vendor implementation, new botanical validation or numerical SpeedTree oracle.

`leaf_stack_cases.py` defines nine columns of vertical X-normal receivers:
clear, opaque blockers at0.1/1/10/50mm, one/two sheet blockers at50/100mm,
and reversed opaque/sheet winding. There are72 vertices,36 triangles,18 objects;
Float64 vertex and int64 triangle arrays contain2592bytes, excluding all other
allocations. View(+sqrt(3)/2,0,0.5), sun(-sqrt(3)/2,0,0.5), metres. Same authored
R(0.08,0.22,0.025), T(0.035,0.12,0.018), irradiance(3,2.7,2.4), black world,
flat true normals and calibrated zero-thickness Slab/sqrt(T) backend encoding.
The opaque blocker has only a Diffuse closure, R0.02 and no transmission closure.
All camera/shadow backface culling is explicitly False.

Independent Float64 plane/Gram rays see only the clear receiver illuminated;
all eight layered centres are geometrically occluded, including reversed winding.
The bounded Embree query agrees for packets1/4/8192. No hemispherical T-to-beam-T
conversion is assumed for a sheet blocker. Numerical opaque answers are separate
from measured optical RGB. `leaf_stack_checks.py` also verifies source roles and
that the positive control's blocker intersects light rays but misses camera rays.

Every render is1920×1080,8samples, Standard/exposure0; RGB is decoded from sRGB.
Existing0.005 absolute linear-RGB isolated-sheet tolerance is unchanged. The stack
experiment's successful process means the experiment completed, not that neighbour
occlusion passed: its report records both qualifications separately.

## What the actual images establish

Without shadows every receiver agrees with its analytic unoccluded T response.
With shadows the clear green fraction is0.998509 of its expected0.0893153. Even
opaque blockers at0.1/1/10/50mm retain0.998655/0.998519/0.998579/0.998576. Reversed
opaque winding retains0.998646; material and winding cannot explain this away.
These centre RGB results are exactly unchanged by `use_thickness_from_shadow`,
by a1000m→10m camera far plane, or by their combination. This statement concerns
measured centre regions, not identical complete PNGs or every EEVEE material.

A second control uses ordinary Diffuse reflection, two receivers and an opaque
blocker100mm toward the light. The blocker covers centre light rays and stays
outside centre camera rays, verified independently in Float64. At camera far1000m,
the supposedly shadowed green remains0.163241 versus expected zero; the original
0.005 criterion fails. At far10m it becomes0.0000249; maximum error over clear and
blocked receivers is0.000459. Shadow-off controls stay unchanged and pass0.000559.
Thus a finite camera range fixes ordinary shadow visibility, while the Slab
transmission closure still fails the opaque-neighbour control in this setup.
Internal EEVEE shadow-map implementation details have not been established.

A reversed-normal Diffuse substitution is rejected: the clear shadow-on receiver
falls to green0.008596 instead of0.089315 and shows self-shadow artifacts. Its
original failing process/criterion is retained. No substitution into the product,
no tuning R/T to the whole field, no invented per-leaf transmission multiplier.

## Finite field camera and retained geometry

`grass_light_preview.setup` now declares camera clip0.1..10m for its fixed4m
camera distance and1.4m orthographic study. `camera_depth_receipt` transforms all
mesh bounding corners to the camera and rejects any clipped geometry before
rendering. It records bounds, object/vertex count and checked corner count.
Two actual Blender tests compare these bounds with independent NumPy depths and
require rejection at either clipping plane. It is a study configuration, not a
worldwide visibility distance. The unchanged finite full grass/ground images and
all16 isolated R/T patches were rendered again; all16 still pass, max0.000876.

The original saved grass identity remains
`1cb40500dc435ebf78573379766dbeae51cb3f452bf76053184a2f78fe041a9a`:
2368 roots,8 existing prototypes,284160 blade triangles and the same ground.
The finite camera includes both complete meshes. Actual front/back images were
opened against the previous images; ground shadowing is stronger. Fixed common
footprint mean green becomes0.118641 front and0.069545 back versus previous
0.119255/0.071944. The unchanged CPU model remains0.134909/0.042195. Descriptive
MAE0.019311/0.018300 is not an acceptance gate. In particular the remaining back
mismatch cannot qualify canopy transport against this failed neighbour oracle.

## Costs, receipts and reproduction

Local proof root: `/tmp/outshine-vegetation-leaf-stack-proof-8a429c34c`.
All receipts bind to their measured commit; no images are published to the repo.
Baseline layers: efa693b66,59.49s wall/60.13s CPU/1858.3MiB peak RSS, exit0.
Thickness:70391486f,62.75/63.36s/1857.3MiB, exit0; reversed-normal negative control
40.49/41.17s/1803.4MiB, exit1. Opaque1000m control:ed4d69f2e,
23.90/24.51s/1829.6MiB, exit1; same control10m:7ae8434e4,
24.15/24.77s/1828.2MiB, exit0. Layers10m:33dd6703a,
66.72/65.87s/1857.8MiB, exit0. Layers with thickness10m:84dda5808,
67.07/67.59s/1856.3MiB, exit0. Original failure evidence is retained.

At84dda5808 the unchanged whole front/back field in one process costs269.10s
wall/270.87s CPU/3177.5MiB peak RSS, exit0; the16-patch/four-image replay costs
117.46/116.40s/1962.5MiB, exit0. EEVEE5.2.2, llvmpipe LLVM19.1.7; CPU-only backend
on an Intel i5-1135G7. Overlapping study runs and load are recorded; no speedup,
GPU/frame/device or total-allocation claim. Reported packed geometry bytes do not
measure Blender copies, shadow-map allocations or driver memory. The camera guard
checks16 corners for the full field and144 for the separated stack objects.

Use `.venv`, existing NumPy/embreex/bpy/Pillow dependencies and explicit fresh
system-temp outputs. `PYTHONPATH=lib/src:lib/tests:test/experiments/vegetation`:

```sh
python -m unittest leaf_stack_checks leaf_camera_checks grass_light_checks raster_rows_checks grass_projection_checks test_ground_cover test_graminoid
python test/experiments/vegetation/leaf_stack_preview.py --output <temp> --clip-end 10
python test/experiments/vegetation/leaf_stack_preview.py --output <temp> --clip-end 10 --variant shadow_thickness
python test/experiments/vegetation/leaf_shadow_control.py --output <temp> --clip-end 10
```

Use far1000 to reproduce the failed ordinary-shadow control; the reversed-normal
variant deliberately fails isolated-sheet acceptance.28 numerical checks pass
with source and the previously installed library; two additional Blender camera
checks pass. No library code changed and no new wheel validation is claimed.
Saved field/pixel reproduction commands remain in `grass_direct_light.md`.
A trustworthy canopy-transmission reference, diffuse neighbour transport,
world-scale products, botanical coverage, procedural detail, wind, angular/LOD
transitions and native/device acceptance remain open at #23/#451 and their owners.
