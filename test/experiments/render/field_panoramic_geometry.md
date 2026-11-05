# Rotation-independent field geometry

Reference: `field_panoramic_geometry.py`; native implementation:
`src/render/impostor/ImpostorPoseBounds.cpp`, `ProjectPanoramic`.

The eye and capture/support planes are fixed. Camera rotation, including roll,
does not change the support. Re-evaluate after eye or projection changes.
This bounds geometric displacement only; it grants no coverage, material,
sample/filter, native floating-point rasterization or shadow authority.

For a source point `p`, support point `q = p - h d`, capture direction `d`,
support normal `n` and centre `c`, `h = dot(p-c,n)/dot(d,n)` is affine.
The exact perspective-difference numerator on either screen axis is a
component of `h * cross(p-eye,d)`. Over a convex capture hull, `|h|` and
`|cross(p-eye,d)|` attain upper bounds at vertices. Their separate maxima
`H` and `C` conservatively bound their product.

Let `B` enclose both complete hulls relative to the eye. Its distance from
zero is `r`, its diameter is at most `D`, and its maximum norm is `R`.
For any rotation with a visible point, that point has forward distance
at least `r/s`, with `s = hypot(1,tan_x,tan_y)`. Every other source/support
point is at most `D` away, so its forward distance is at least `Z=r/s-D`.
Jitter enlarges the viewport slopes; the native bound includes at least
the renderer's half-pixel temporal guard, including binary32 rounding.

Camera validation permits a Gram-matrix error `tau=1e-9` per entry.
Gershgorin bounds give singular values `sqrt(1-3tau)` through
`sqrt(1+3tau)`. Therefore use
`Z=sigma_min*r/s-sigma_max*D`. The relevant cofactor norm is at most
`sigma_max^2`; Cauchy-Schwarz gives the coupled screen bound
`L1 <= hypot(fx,fy)*sigma_max^2*H*C/Z^2`.
Each axis also has its individual focal-scaled upper bound.

Native interval operations enclose the capture vertices, fitted plane,
World/ECEF transform, ray products, distances and arithmetic. Near/far
checks use the actual binary32 projection coefficients, not nominal lens
distances. Eye-plane crossings, backside support, orthographic queries,
invalid frames and overflow retain the original representation.

Tests exercise corners/interior points, viewport edges, arbitrary rotations,
roll, fitted planes, ECEF translations, jitter and rejected clipping domains.
Python uses independent rational screen projections; native tests use
independent long-double projection of the supplied operands.
