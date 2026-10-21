# Candidate projection contract (#990)

This experiment retains the Lit/Flat position expressions and adds only
`precise gl_Position` in temporary diagnostic shaders. Product shader sources,
bindings, formats and default experiment modes are unchanged. This is a proposal
for master, not authorization to suppress pages or a Metal qualification.

## Why a producer contract is necessary

The general Vulkan expression-evaluation rule permits transformations beyond the
positive normal-form family in [ENVELOPE.md](ENVELOPE.md). Passing a finite cohort
does not close that gap. A scoped `NoContraction` contract prevents reassociation
between the two matrix-vector operations and the shift addition. It does not
choose the internal summation order of a compound matrix operation. Its inherited
precision still needs a bound; CPU/GPU bit identity is not the requirement.

The extracted Lit/Flat bodies perform all position arithmetic in `main`. GLSL's
`precise` propagation is limited by function boundaries. This proposal therefore
does not automatically qualify deformation helpers, terrain sampling, other
position producers or a future refactor into functions. Such changes require
their own producer contract. Opacity, original triangles and runtime ownership
also remain independent obligations.

## Connecting the typed instructions to the existing envelope

Let `u=2^-23`, `eta=2^-126`, `gamma(k)=k*u/(1-k*u)` and `R(k)=1/(1-k*u)`.
Use the original normal-or-zero inputs and `B>=1` from ENVELOPE.md. Preserve its
overflow guard. A four-term matrix-vector component inherits an absolute error
bound from equivalent internal sums/products, not necessarily a particular tree's
actual rounded output. Each such formula has at most four degree-two monomials,
eight leaf occurrences, seven binary nodes and selected height four.

Applying the same bounded-perturbation argument locally gives

```text
dot4 error <= gamma(4) * sum(abs(a[i]*b[i])) + 900*eta*L*R(7),
L = max(1, abs(all supplied operands)).
```

The factor 900 is `(8 + 3*7 + 1)^2`: at most two operand flushes and one output
underflow per node, with degree at most two. Every noise monomial contains an
`eta`, leaving at most one original operand. This also bounds the symmetric error
range of inherited precision, whose endpoints need not both be realizable by one
rounding tree. Intermediate overflow is excluded, and arbitrary identity insertion
outside the allowed local A/C/D family is not assumed.

In Lit, composing the model dot with the separate shift addition gives an error
at most `gamma(5)*S[j] + 904*eta*B*R(8)`, where
`S[j]=sum(abs(M[k,j]*P[k]))+abs(shift[j])`. The actual shifted coordinate is bounded
by `6*B^2*R(8)`. Composing the view dot gives at most

```text
gamma(9)*sum(abs(V[j,r])*S[j]) + 9016*eta*B^2*R(59).
```

For Flat, the shift first rounds the translation column, with `P.w=1`. Its model
dot noise is bounded by `2704*eta*B*R(7)`; the complete result is bounded by the
same relative term and `16216*eta*B^2*R(59)` of noise. These loose constants use
`R(a)*R(b) <= R(a+b)` and
`(1+gamma(a))*(1+gamma(b)) <= 1+gamma(a+b)`.
The original overflow bound covers these typed intermediate stages as well:
their ideal monomials are subsets of the original twenty, and the noise constants
remain below the original noise allowance. Signed zero has its real-coordinate
meaning; intermediate denormals/flushes remain included rather than forbidden.

Both results fit the existing `gamma(21)` relative term and `13481272*eta*B^2*R(59)`
noise term. Thus this proposed typed Vulkan contract provides a connection to that
envelope without replacing matrix multiplication by a fixed scalar order. It does
not establish the contract for unqualified original shaders or for Metal's
translation/compiler settings. A device/compiler conformance defect would also
require investigation; finite observations are supporting evidence, not the proof.

## Reproduce the targeted experiments

`cost.py --projection-contract` adds modes 4/5: extracted native Lit/Flat bodies
with precise position output, indexed through the same native depth consumer.
The original four modes remain byte-identical controls. The candidate shaders have
two `OpMatrixTimesVector` operations and one `OpFAdd`, all `NoContraction`; they
retain compound matrix operations. Timings include depth clear/raster/store and
are not isolated vertex execution times or world-FPS evidence.

`run.py --precise-projection` applies the qualifier to the vertex arithmetic
diagnostic. This intentionally retains the old CPU comparisons: matrix-internal
evaluation may still differ. The existing independent envelope verifier remains
applicable to the returned intervals and captured GPU values.

```sh
python3 test/experiments/render/vertex_projection/contract.py \
  --native-checkout /path/to/prepared/native/checkout \
  --output /tmp/outshine-projection-contract-unique-run
```

This driver compiles temporary precise versions of the 20 Lit and 12 Flat variants
in the inspected Makefile, including UV/tint/normal-map/motion layouts. It checks
the three annotation targets, full float32 types and absence of RelaxedPrecision,
and compares annotation counts with the actual frozen cached native modules.
It records compiler/source/include/module hashes and terminal commands. This is a
check of annotation propagation on that fixed producer, not a general SPIR-V
dataflow verifier or an automatically maintained list of all future shaders.
Variant-list, source-topology and actual input-binding changes need review.

Hardware timings, original/candidate depth and full-world images belong to the
delivery's evidence. No generated diagnostic images are published in the repo.
World/root/frame/camera/light lifetime, side-demand union, useful complete-root
coverage and actual avoided IO remain #990's acceptance conditions.

## Primary sources

[Vulkan expression evaluation and inherited precision](https://docs.vulkan.org/spec/latest/appendices/spirvenv.html#spirvenv-evaluation-expressions)
defines the relevant instruction rules.
[GLSL precise qualifier](https://registry.khronos.org/OpenGL/specs/gl/GLSLangSpec.4.60.html#the-precise-qualifier)
defines propagation and its function boundary.
The Vulkan GLSL mapping associates precise with NoContraction; the generated
modules are checked directly. The error constants and composition above are our
derivation. The interval-analysis background and visually inspected SIGGRAPH
reference remain linked in ENVELOPE.md.
