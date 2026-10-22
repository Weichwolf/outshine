# Bounded projection envelope (#990)

This is a native reference for a specified arithmetic family, not page suppression.
It retains production shaders and their existing Lit/Flat expressions. The GPU
diagnostic checks whether its observed outputs fit the reference. Neither passing
samples nor the derivation below establish that every permitted compiler rewrite
belongs to this family. That qualification remains required before a world consumer
can use the bounds. `qualified_for_page_suppression` remains false.

## Expression family and domain

Let M and V be the supplied binary32 model/view matrices, P=(px,py,pz,1), and
S=(sx,sy,sz,0). The ideal component is

`C[r] = sum_j V[j,r] * (sum_k M[k,j]*P[k] + S[j])`.

Flat instead adds S to the model translation before multiplication by P. Its shift
terms additionally multiply P[3]=1. The common family has at most 20 monomials,
degree at most three, and at most 60 leaf occurrences. It allows positive-form
addition/multiplication, reassociation, commutation and distributive expansion or
factorization preserving those monomials. It excludes insertion of cancelling
terms, arbitrary additional identities, divisions or unrelated work. FMA contraction
fits when its error is enclosed by the corresponding add/multiply formula; ordinary
GLSL FMA is not assumed correctly rounded. Producer qualification must cover any
compound instruction's inherited-precision rules as well.

There are at most N=59 binary nodes. On an expanded monomial, selected addition
nodes number at most 19 and selected multiplication nodes at most two; hence
h=21. The addition bound follows inductively: at a sum select one branch and count
the other as at least one additional monomial; at a product add the two selected
counts, using `(a-1)+(b-1) <= a*b-1`. Multiplication combines at most three leaves.
Literal zero/one terms were retained in these counts. This is a structural bound,
not an empirically chosen epsilon or claim of optimality.

The native function rejects nonfinite or subnormal inputs among the 38 consumed
scalars. Unused point.w/shift.w do not participate. B is the largest consumed input
magnitude or one, whichever is greater. Unknown inputs remain needed. Signed zero
has the same real-coordinate meaning here; positive-divisor checks are separate.

## Rounding and underflow bound

Use u=2^-23, covering either adjacent binary32 rounding result, and eta=2^-126.
A normal finite primitive has relative perturbation at most u. A subnormal result
or a flush to zero can instead be represented by an additive perturbation of
magnitude at most eta. An operand flush is another additive perturbation at its
input edge. Allow two operand errors and one output error per binary node. The
resulting expression has at most nu=60+3*59=237 leaf occurrences and still degree
at most three. Errors are independent bounds; no correlation is assumed.

For original monomials, the standard product-of-perturbations argument gives
`gamma = h*u/(1-h*u)`. Noise terms each contain an additive error, leaving at most
two original factors. Their magnitude is at most `eta*B^2*R`, where
`R=1/(1-N*u)` bounds the accumulated relative factors. Treat each leaf occurrence
as a distinct formal variable. Expanded terms correspond to subsets of at most
three such occurrences; their number is below `(nu+1)^3=13481272`. This also bounds
terms containing multiple additive errors, since eta<1 and B>=1.

Thus the reference uses

`E[r] = gamma * sum(abs(original_monomial[r])) + 13481272*eta*B^2*R`.

Any subtree in the stated family is bounded by
`20*B^3*R + 13481272*eta*B^2*R`. Inputs are rejected if this upper bound reaches
binary32's largest finite value. This excludes overflow/NaN through intermediates
within the model, rather than checking only the final result. The derivation does
not cover a transformation outside the stated family merely because its final
real polynomial happens to be equal.

## Native interval evaluation

`Envelope.cpp` encloses each ideal component in binary64 and expands it by an
upward-rounded E. Binary32 products of two consumed normal values fit exactly in
binary64's significand/exponent range. Third-factor products and sums are rounded
outward with `nextafter`; exact zeros need no expansion. Binary64 exponent range
also covers all products in this domain. The function requires CPU round-to-nearest
and checks binary32/binary64 representation requirements with static assertions.
It neither changes process rounding nor assumes that the GPU uses the CPU mode.

`Divide` rejects nonpositive/uncertain w, malformed/nonfinite intervals and quotient
overflow. It bounds each coordinate/w by all four endpoint quotients rounded
outward. Flooring the z lower endpoint to binary32 supplies the diagnostic's
downward corner bound. This is not a claim about rasterizer interpolation precision,
clipping, viewport snapping or complete D32 pixels.

The scalar algorithm requests no heap storage. Its fixed work counters report
logical input inspections, binary64 products/additions/divisions and
outward steps. They omit comparisons, loads, branch/call costs and CPU timing
overhead; they are not complete machine-instruction counts. The native diagnostic
times one envelope per input, outside GPU draws. `Divide` costs are separate from
those scalar counters. JSON output and test preparation are outside this kernel.

## Checks and next consumer

`run.py` builds the native component and domain checks together with the existing
GPU diagnostic. Original CPU/GPU values remain in `rows`; one `enclosures` record
per input includes consumed values, bounds, unknown reason, work and CPU time.
Clip escapes, unsafe comparable native corner bounds, empty admission and missing
GPU samples fail the process. The domain checks exercise input subnormal/Inf/NaN,
overflow rejection, intermediate underflow, uncertain w, invalid intervals and
CPU rounding-mode rejection/restoration.

`python3 verify_envelope.py /tmp/run/result.json` independently checks binary64
endpoints against exact rational C/E, all observed GPU coordinates and applicable
GPU ratios. It also evaluates shuffled expanded products/sums and precomposed
view/model matrices with exact-rational primitive rounding: nearest-even, toward
zero and both directed modes, each preserving or flushing subnormals. These finite
tests challenge the implementation; they are not an enumeration of every compiler
rewrite or a replacement for the model derivation.

Before page suppression, qualify the real producer against this family or broaden
the bound. Then consume coordinate uncertainty in an inward whole-patch polygon,
validate original triangles/frontness and clipping/raster rules, and apply conservative
depth with existing camera/root/light generations and side-demand union.
Outward XY boxes alone are not opaque coverage. Actual avoided IO, full world
images and device budgets remain #990's acceptance conditions.

## Sources

[Duff, SIGGRAPH 1992](../../../../doc/references/vegetation/siggraph/1992-interval-arithmetic-csg.pdf),
page 132 (PDF page 2, also inspected visually), motivates inclusion-preserving
interval evaluation and directed endpoint rounding.
[PBR, Managing Rounding Error](https://www.pbr-book.org/4ed/Shapes/Managing_Rounding_Error)
describes gamma bounds and transform-error propagation. Its assumptions do not by
themselves certify this GPU expression family or our projective consumer.
[Vulkan expression evaluation and precision](https://docs.vulkan.org/spec/latest/appendices/spirvenv.html#spirvenv-evaluation-expressions)
allows rearrangement and specifies default rounding, denormal and inherited
precision behavior. The 20/60/59/21/237 bounds and noise-term count above are our
explicit model derivation, not guarantees quoted from those sources.
