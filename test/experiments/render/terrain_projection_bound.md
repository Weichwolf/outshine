# Conservative projective terrain displacement

The native culler already bounds each LOD's world-space displacement. Its old
projection bound adds individual row norms and then combines both pixel axes.
This is safe but loose, even for an orthographic camera: equal axis scales give
sqrt(2) times the largest possible two-dimensional displacement.

## Bound

Let the clip matrix's spatial rows be `rx`, `ry`, `rw`. A fine point has positive
homogeneous coordinate `w` and normalized image coordinate `n = (x/w, y/w)`.
For a displacement `d` with Euclidean length at most `e`, its pixel displacement is

```text
D * ((rx, ry) - n * rw) * d / (w + rw·d),    D = diag(width/2, height/2).
```

The denominator is at least `minimumW - e*length(rw)`. The projected patch bounds
contain `n` in a rectangle. The matrix above depends affinely on `n`; its spectral
norm is convex. Its maximum over that rectangle is bounded by the maximum of its
four corner norms. For two rows `a,b`, the squared spectral norm is the largest
eigenvalue of their 2×2 Gram matrix. Normalize its discriminant by its trace to
avoid squaring large Gram entries. Nonfinite intermediates or underflow of a
nonzero matrix keep the finest LOD. A zero matrix has zero projected displacement.

The implementation adds `64*float_epsilon*old_row_sum_bound` to the numerator
for float arithmetic. Existing spatial expansion, displacement margins, positive
denominator checks and the **same 0.5 px Euclidean allowance** remain. The source
DEM error is unchanged. This tightens a projection bound, not the allowed error.

## CPU comparison

`terrain_projection_bound.py` reads the same 16,255 Körbersee pages and compares
both projective formulas with identical exact local interpolation-error inputs.
Native world bounds and float32 projection have additional margins; this CPU
count is not an exact native invocation or performance prediction.

| Yaw | Row-sum triangles including skirts | Spectral triangles including skirts | Reduction |
|---|---:|---:|---:|
| 0° | 5,989,896 | 5,067,896 | 15.39% |
| 180° | 5,423,208 | 4,547,600 | 16.15% |

`verify_terrain_projection_bound.py` checks 100,000 displaced points for general
projective matrices, float32 evaluation against independent double-precision SVD
at 4,000 rectangle corners, orthographic scales, unsafe denominators and underflow.
The native stable-selection test adds orthographic/perspective cases that the old
loose bound rejects, plus a tiny-projection fallback. Existing selection/resource
checks remain unchanged.

## Reproduce

```sh
python3 -W error test/experiments/render/verify_terrain_projection_bound.py
python3 test/experiments/render/terrain_projection_bound.py --pages "$PAGES" --scenario "$SCENARIO" --output "$OUTPUT"
```

GPU timings, final raster comparisons and remaining acceptance belong to #515.
An improved bound alone does not establish image quality, lower source error,
a global geometry optimum or a completed device budget.
