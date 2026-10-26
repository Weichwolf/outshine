# Inner coverage from uncertain projection (#990)

`UncertainPatch.h` accepts four independent finite binary64 clip-coordinate
intervals per source corner. They must enclose the actual projection; this
experiment does not establish that producer contract. In particular, the positive
Add/Mul model in `../vertex_projection/ENVELOPE.md` is not a proof of every legal
world-shader/compiler rewrite. Opacity, completeness, source topology and the
requested front-facing orientation remain caller obligations.

## Exact reference and independent oracle

Positive `w`, nonnegative `z` and `z.upper <= w.lower` are required at every
corner. Eye/near/far crossings and uncertain facing return unknown. Exact GMP
quotients of the four numerator/denominator endpoint combinations give a window
rectangle for each vertex. The minimum of `z.lower/w.upper` over the four vertices
is a reverse-Z lower bound for both original triangles. This does not require
their depths to be coplanar.

The four consecutive-triple orientation tests each examine all 64 rectangle-
corner combinations. Orientation is multi-affine in the three independently
varying points, so its extrema occur at those combinations. Strict agreement
establishes a consistently oriented convex quadrilateral for every realization.
The original source diagonal then partitions that quadrilateral into its two
original triangles; no missing triangle is filled by a synthesized hull.

The exact constructor uses the rectangle centers as polygon `P`. Let `B` be the
origin-centered box whose radii are the largest vertex half-widths. Every realized
vertex differs from its center by an element of `B`. Thus, in every direction,
the realized polygon's support is at least `h(P)-h(B)`. Every point of `P eroded
by B` lies in every realized convex polygon. The constructor shifts each center
polygon edge inward by `abs(A)*rx + abs(B)*ry`, intersects adjacent edges exactly,
and rejects infeasible intersections. Existing inward quantization verifies the
final grid vertices. This is conservative and need not maximize usable area.

The independent source oracle does not reuse that erosion. For every perimeter
edge it constructs all 16 endpoint-rectangle edge lines, 64 lines in total. Each
line's minimum over an entire pixel is evaluated with exact rationals. The edge
function is multi-affine in the two source endpoints, so these tests imply
containment for every endpoint realization. Depth is checked against the exact
source lower bound. This oracle also checks the fast constructor below.

## Fixed-width constructor

`IntegerUncertainty.cpp` replaces GMP construction with bounded integer predicates.
Its domain is deliberately narrower: viewport dimensions at most 8192, finite
normal or zero binary64 input endpoints, and window bounds within +/-8192 pixels.
Rejection retains unknown; it grants no page suppression. The experiment still
uses the same 160-byte GPU packet and existing integer whole-pixel shader.

Window rectangles are rounded outward on a 2^16-units-per-pixel certificate grid.
Binary64 mantissa/exponent decoding and unsigned 128-bit quotient/remainder
arithmetic determine floor/ceil exactly, including very large exponent gaps.
No floating division supplies a coverage certificate. The shift guards keep
admitted numerator/denominator intermediates within 84 bits; oversized ratios
are rejected before shifting. Coordinate magnitudes are at most 2^29, so each
orientation intermediate is bounded by `8*(2^29)^2 = 2^61 < INT64_MAX`.

Floating-point viewport clipping and homothetic contraction propose a polygon.
Each quantized vertex must independently satisfy all 64 integer source edge
half-planes, and the output must be convex. Therefore its entire polygon lies
inside every realized source quadrilateral. Proposal rounding cannot authorize
coverage. At most 21 coarse contraction proposals and 20 refinements are tried;
each is separately certified. Refinement stops at a one-grid-unit motion bound.
No monotonicity, maximal coverage or optimal polygon claim follows from bisection.

The normal positive float32 depth proposal is compared to each exact dyadic
`z.lower/w.upper` by integer products/exponents. If necessary it is decremented
one ULP and rechecked; failure and zero/subnormal results return unknown.
The algorithm uses fixed-size automatic arrays and no explicit heap allocation.
Compiler stack size, OS/driver peaks and caller JSON/GMP allocations are not
measured by this statement. It is currently an experiment, not product code.

## Reproduction and observed comparison

Run `../patch_selection/run.py` as documented there. It shares the same aligned
pair ranking between exact-input, rational-uncertainty and integer-uncertainty
controls. The two uncertainty constructors receive the same 944 enclosing calls
on the fixed 17-mesh input, reject the same 108 uncertain-facing candidates and
retain 128 packets. Ranking, preparation and reduction times are separate.

On the Iris Xe diagnostic, rational erosion covers 308/4096 cells and the integer
construction 310/4096. Both reduce 128 packets to 42 with byte-identical R32F and
D32F buffers before/after reduction and in both draw orders. Their different
safe polygons need not produce identical buffers to one another. These counts
are neither a complete-root coverage denominator nor a geographic guarantee.

Preparation time excludes shared ranking and includes envelope calls, serialization
and reduction. The independent GPU harness measures packet preparation from
already supplied intervals separately. These diagnostic timings establish no
world loading/FPS improvement. Exact commands, source hashes, costs and terminal
results belong to the delivery's recorded evidence, not a timing threshold here.

`uncertain-patches.json` adds 20 fixed analytic/adversarial cases: zero/one/two-
pixel uncertainty, anisotropy, varying depth, uncertain facing, clip crossing,
adjacent binary64 values around a quantization boundary, and a large exponent
gap. Both constructors must satisfy the independent whole-pixel/depth oracle;
explicit counts or conservative minima prevent a vacuous all-unknown pass.

The producer's complete arithmetic family, actual world rasterization precision,
whole-root usefulness, world/root/camera/light lifetime and side-demand union
remain open in #990. Every result remains unqualified for page suppression.
The conservative erosion basis is Durand et al., SIGGRAPH 2000, cited and visually
inspected as recorded in [README.md](README.md); the endpoint predicates follow
directly from multi-affinity rather than a guessed projection epsilon.
