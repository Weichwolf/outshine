# Independent shadow depth and PCF reference (#873 / #868)

The fixed [native input](../../outshine/data/render/regional-shadow-translation/README.md)
contains 61 world projections and camera rebasing inputs, **no caster vertices,
placements, packed GPU views or receiver fragments**. This experiment separates
ideal ring correctness from the missing finite-arithmetic/native admission.
It changes no product code, GPU layout, sampler or quality threshold.

## Reproduce

Python 3.11+, NumPy and Pillow are required; repository `.venv` supplies them.

```sh
.venv/bin/python -m unittest discover -s test/experiments/render \
  -p 'test_shadow*reference.py'
.venv/bin/python test/experiments/render/test_shadow_translation_cost.py
.venv/bin/python test/experiments/render/measure_shadow_ring.py \
  --output "${TMPDIR:-/tmp}/outshine-research-shadow-reference"
```

Inputs are versioned; normal tests never regenerate them. Output includes the
full report, four compressed raw final depth/physical-ring arrays, exact analytic
world triangles and a diagnostic PNG. Output paths are explicit and temporary.

## Full-raster comparison

Each region receives 13 fixed analytic world triangles derived from its **first**
real light projection. They include tile-edge-crossing casters and interior casters.
They remain fixed through all 61 real transforms. No main-camera culling occurs.
Every frame and all four original 2048×2048 tiles are rasterized afresh, including
unchanged projections. Copy and ring candidates rasterize only disjoint newly
exposed logical strips. All 1,023,410,176 reference texels are compared with both
candidates. No cropped raster or lower-resolution proxy replaces that comparison.

`raster` is an affine Float64, pixel-centre, two-sided opaque reference with
Float32 depth storage, reverse-Z max and clear zero. It excludes native coverage
rules, hardware subpixel quantization, alpha, instancing and GPU depth interpolation.
The measured triangles are flat in light depth; an independent affine-plane test
also checks sloped interpolation, depth clipping and overlapping casters. This is
not a replay of the original Flensburg shadow geometry or its final PBR image.

The origin is the physical local texel for logical (0,0). If a world point moves
by `(dx,dy)` in current logical texels, origin becomes `oldOrigin-(dx,dy) mod N`.
Logical Y follows the shader's negative clip-Y mapping. PCF clamps each logical
neighbour before adding origin and wrapping locally. Receiver-plane offsets,
neighbour strides and bilinear phases remain logical. A separate scalar weighted
enumeration checks borders, physical seams and nonzero receiver slopes.
Using wrapped physical offsets is retained only as a deliberately failing control.

Projection/basis/depth changes, fractional shifts, disjoint windows, resolution
changes and any content revision reset the full tile. A content revision covers
caster insertion/removal, animation, terrain and light/material coverage changes;
localized dynamic dirty rectangles are not implemented. Rejected candidates never
replace accepted data/origin/projection/content, including rejected initial fill.
This CPU transaction stages a copy: it is **not** a GPU lifetime/rejection proof.

Tests independently enumerate every exposed pixel for all shifts -9…9 at size 8,
exercise both axes/signs, diagonals, repeated wraps and large jumps, retry/reset,
and all 64 origins at size 8. They intentionally use small tiles for combinatorial
controls; the actual-trace raster comparison preserves 2048×2048.

## Float32 rebasing counterexample

`shadow_rebase_arithmetic.py` reconstructs the source's sequential host-double
`worldProjection - linear*preView`, then casts the matrix and `anchor+preView`
to Float32. A declared identity local placement uses 4,913 deterministic points
within ±200 m and a fixed analytic anchor equal to the first camera centre.
These are legal analytic inputs, **not observed native placements**.

For each of the 36 observed tile updates, points are compared against that region's
previous full update with its exact integer world-projection delta removed.
Both declared left-to-right unfused and fused Float32 dot variants produce depth
differences up to 5.960464477539063e-8; region-0 XY residual reaches
0.000244140625 texels. Eight explicit witnesses are checked independently using
exact Fraction arithmetic and nearest-even Float32 rounding. Receiver witnesses
include the source's region bias before the `step` comparison.

For example, region 0, initial→measured frame 2, local point (-200,-200,-200):
unfused depth changes from 0.49020686745643616 to 0.49020689725875854.
The biased analytic receiver threshold 0.49020686745643616 changes `step` from
1 to 0. No stale projection, smaller bias or threshold relaxation is authorized.
The tiny numeric residual is not a measured visible error in the real Place.

Host/compiler contraction, GPU dot ordering and actual depth interpolation remain
unobserved. The experiment declares its arithmetic; it does not identify which
variant either GPU executes, nor prove a product regression. It shows why equal
world-depth coefficients and integer XY deltas alone do not prove depth reuse.
The ideal ring parity above **does not admit native reuse**. Next compare actual
packed views/placements and full GPU depths; camera-independent shadow coordinates
are a possible remedy to assess with the backend owners, not a delivered change.

## Costs and limits

The report counts projected vertices, triangle/scissor visits, triangle/pixel
candidates, covered samples, new texels, CPU
staging-copy bytes and retained-copy assignment payloads. Ring transaction staging
copies overlap in this Python reference. These are not GPU ring copies or bus
traffic. The copy candidate assigns old→new once; the separate #870 scratch model
counts two GPU transfers. Do not equate these different payload definitions.
Counts include complete initial fill separately from later updates. Measured host
wall/CPU time and Linux process peak RSS cover instrumentation and exports;
allocation count and native CPU/GPU costs are unavailable. No speedup is claimed.

Both backends still need real-caster conservative selection, wrapped raster
rectangles/viewports, load/scissored-clear behavior, attachment/resource lifetime,
accepted-submit state, finite-arithmetic admission and current Place/device checks.

## Source

Losasso/Hoppe, SIGGRAPH 2004, *Geometry Clipmaps*, §§3/5 and Figure 2,
`doc/references/terrain/siggraph/2004-geometry-clipmaps.pdf`: text and page 3 inspected.
Toroidal storage and newly exposed strips motivate the experiment; the paper's
terrain clipping/delayed-update policy is not adopted and proves no shadow method.
Product arithmetic/PCF source at native input product commit `37db0b9e3`:
`SunShadowStage.cpp`, `geometryShadowVertex.glsl`, `surfaceLighting.glsl`,
`shadowFilter.glsl` (unchanged at this experiment's initial base).
