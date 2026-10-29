# Conditional whole-source projection envelopes

Issue #1724 extends the existing #990 expression-family enclosure to complete
stored-position boxes. It supplies a Python reference, not production shader,
raster, World or page-suppression qualification. The older geometric camera
projection in #1720 does not model actual packed GPU operands.

## Inputs and ownership

`box_envelope` consumes bounded nonempty Nx3 source boxes, exact submitted
binary32 model/view matrices and shift. Matrices are row-major NumPy arrays;
column-major upload words must be reshaped with `order="F"`. Box endpoints must
be exact stored binary32 values. Every consumed operand and every stored vertex
must be finite normal-or-zero. Endpoint normality alone cannot establish the
vertex-stream domain: the producer must attest it. Tests/control replay inspect
the complete actual vertex streams; runtime metadata must carry that fact.

The box must enclose the complete selected source under the same asset version,
instance and submitted frame binding. Bounds of samples, camera JSON, an ideal
unrounded shift or a different placement are insufficient. The input shift is
already rounded; this does not certify its relation to ideal Earth coordinates.
Invalid input/overflow raises instead of admitting a replacement. Native
consumers must keep originals on an unknown result.

Try an existing complete cohort box first; only an inconclusive result warrants
finer source metadata. The kernel performs no geometry, IO, source generation or
upload. Deriving boxes by scanning all fine positions belongs to fixture/source
preparation, not the admission path. The study separately scans primitives as
an opponent and never claims cold-IO savings from that scan.

## Existing arithmetic family, extended to a box

Use [ENVELOPE.md](vertex_projection/ENVELOPE.md) and its existing rational
`family_limits` without new GPU-specific constants. For a fixed output component,
the ideal polynomial is the sum of original `V*M*p` and `V*shift` monomials.
The existing bound is `gamma21 * sum(abs(monomial)) + noise(B)`, with the same
20-monomial/59-node/21-path structural assumptions and overflow rejection.
Here B bounds all box coordinates and consumed operands at once, and is at least
one. Its shared noise conservatively bounds the noise at every stored point.

Combine ideal model/view coefficients before projecting the source AABB; this
avoids expanding a model AABB and then a view AABB when transforms cancel.
Products of two binary32 inputs are exact in binary64. Directed
binary64 additions enclose the combined ideal coefficients, including shift in
the constant coefficient. Signed coefficient/endpoint products and directed sums
then enclose the whole ideal affine range.

Separately accumulate the absolute ORIGINAL monomial coefficients. Absolute
values of combined coefficients would lose cancellation mass and are invalid.
For each coordinate use its maximum endpoint magnitude. Directed positive
products/sums and upward-rounded exact gamma/noise yield a uniform error radius.
Expand both ideal bounds by that radius. All three-factor nonzero input products
are above binary64 underflow; the existing structural bound rejects binary32
intermediate overflow. Intermediate binary32 denormals/flush remain covered by
the unchanged #990 noise, while subnormal INPUTS remain outside its domain.

`conditionally_inside` requires positive lower w and conservative homogeneous
side/near/far inequalities, using lower w for every side. It is a sufficient
clip-volume test under the named expression family. No epsilon, raster coverage,
opacity, depth replacement, camera reuse or compiler qualification is inferred.
Both producer-expression-family and raster-qualified flags remain false.

## Independent opponents and limits

The rational #990 point oracle checks all eight box corners plus three actual
positions for each complete native mesh. Dense synthetic matrices, inverse
transforms, shifted cancellation, uncertain w, input subnormals, intermediate
underflow and overflow have focused controls. Synthetic expressions exercise
four binary32 rounding modes and preserved/flushed intermediates.

The eight-corner opponent keeps ideal/error correlation: with fixed noise,
`ideal + gamma*mass` is convex in each coordinate, and the lower expression is
concave. Its extrema occur at endpoints. Point enclosures at all corners thus
enclose the continuous box, including interior points. This can be tighter than
the uniform maximum error used by the main kernel; the results are deliberately
not asserted bit-identical. Cost comparisons report their numerical difference
and equal decisions only for the measured controls.

The fixed A18 operand archive is bound by its receipt/hash, not reconstructed
from camera metadata. Its 94,999 stored positions and 46,889 triangles reproduce
the versioned original parts exactly. All triangles are checked against their
1,891 complete source boxes using separate nearest-even, unfused CPU Lit and
Flat expressions. Those evaluations are NOT new native GPU observations. Two
explicitly unresolved sources and the previously unqualified opacity remain
outside replacement authority. Seventeen older complete native mesh controls
add 34,788 positions; this selected subset is not worldwide coverage.

Production Lit/Flat compiler/translation qualification remains #990 with native
owners. A passed finite replay cannot establish every permitted compiler rewrite.
The native original capture is displayed beside projected vertices and bounds;
diagnostic PNGs stay in system temp. No new product/README/doc image is published.

## Reproduction and cost scope

Use the checkout's Python environment, with
`PYTHONPATH=test/experiments/render:test/experiments/buildings`,
`OPENBLAS_NUM_THREADS=1` and `OMP_NUM_THREADS=1`:

```sh
.venv/bin/python -B -m unittest test_capture_box_envelope test_capture_source_support
nice -n 10 .venv/bin/python -B test/experiments/render/capture_box_study.py \
  --output /tmp/outshine-capture-box/study
.venv/bin/python -B test/experiments/render/capture_box_cost.py \
  --output /tmp/outshine-capture-box/cost
```

Freeze measured sources first. Nine rotating normal-priority CPU/wall rounds
exclude correctness comparisons; separate Memray traces count allocation events,
requested bytes and scoped high watermark. Inputs and held expected results
precede tracing. Numerical tightness is distinct from timing. These are Python
held-metadata scopes, not native frame times, cold loading, process/UMA peaks or
proof of device-budget compliance.

Both methods form 76 shared model/view/shift coefficient products. The direct
kernel then evaluates 84 products per box (64 signed coefficient-endpoint,
16 mass and four error scaling), versus eight times that point work for the
corner opponent. This counts explicit numeric products, not validation or all
instructions. Exact stored-position validation copies 72 bytes per box through
Float32/Float64 arrays per evaluated box/point; the corner method evaluates eight
points per input box and additionally gathers 192 bytes per input box.
Total copies and physical traffic remain unmeasured. Returned numeric arrays
occupy 160 bytes per box; source IDs are separate from the 48-byte endpoint pair.

On34ec02768 (Linux/Python3.13.5/NumPy2.5.4/Memray1.20, host load
8.06/5.77/4.55), the single complete cohort costs1.812ms CPU with the direct
box and1.817ms with corners; that small timing difference is inconclusive.
Direct/corner scoped requested allocation is474,192/488,000B and peak
8,132/7,836B: the direct single-box path does not win every metric.
At1,891 source boxes, direct/corner CPU is6.309/38.120ms, requested allocation
9,581,027/73,678,964B and scoped peak739,288/6,239,388B. Four raw traces are
retained. Both methods return all supported controls conditionally inside.

The largest extra direct-box extent is6.773e-7 in homogeneous x,3.881e-7 in y
and2.433e-9 in reverse-Z for the cohort, with identical w bounds. The eight-corner
path buys no extra supported admission here. Keep the simpler direct interval
path as reference; corners remain its numerical opponent. No universal speed,
tightness or native-device winner is claimed. Older mesh controls demonstrate
larger correlation loss, and an inconclusive cohort must retain/refine originals.

Batching only the shared coefficient accumulation preserves all result fields
bit-for-bit on2,492 boxes (current1,891 sources/cohort plus30 dense synthetic
bindings of20 boxes). Compared with the frozen scalar implementation71ee724d5,
cohort allocation events fall6,363→3,027 and requested bytes1,001,968→474,192.
The scoped peak rises320B; input products and interval quality stay unchanged.
Initial and final traces are separate; noisy Python timing is secondary to this
direct work/allocation evidence. This does not establish a native speedup.

## Sources

First consulted local SIGGRAPH reference:
[Duff 1992, Interval Arithmetic and Recursive Subdivision for Implicit Functions
and Constructive Solid Geometry](../../../doc/references/vegetation/siggraph/1992-interval-arithmetic-csg.pdf),
visually inspected interval rules on PDF page 2. It motivates interval inclusion
and avoiding loose repeated transforms; it supplies no device arithmetic bound.
The actual arithmetic derivation and compiler limits remain in the existing
#990 [contract](vertex_projection/CONTRACT.md) and envelope reference above.
