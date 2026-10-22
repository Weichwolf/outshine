# Directed patch placement over an observer ball

Refs #867/#844. This geometric control consumes the existing source-owned native
capture pairs. It changes neither source meshes nor captured patches. Source
faces contain intentionally occluded regions: a whole-face→patch failure is
not itself a native error-contract violation. Patch→full face bounds placement;
the reverse direction measures a different, larger coverage domain.

```sh
OPENBLAS_NUM_THREADS=1 PYTHONPATH=test/experiments/render \
  .venv/bin/python -W error -m unittest test_patch_projection_bound
OPENBLAS_NUM_THREADS=1 PYTHONPATH=test/experiments/render \
  .venv/bin/python -W error test/experiments/render/measure_patch_projection.py \
  --input test/outshine/data/render/building-capture-movement/native-cell.json.gz \
  --radius-m 6 --output /tmp/outshine-patch-projection
```

Outputs use a fresh system-temp directory. The original Source/Part/FaceAt,
stream/triangle identities, native provenance, rational bounds and constructive
barycentric witnesses remain reproducible. Unknown identities stay unknown.
The complete selected surface is the reference; its existing difference from
independent Fine is a separate contract. The nearer cell has no patch emissions.

## Directed surface bound

For each source triangle, exact dyadic barycentric subdivision at depth0,1 or2
partitions its complete interior. Float closest-point queries propose one target
triangle per fragment and three barycentric rows. Integer rows are nonnegative
and sum to2³⁰, so each proposed vertex lies in that actual target triangle.
The exact source vertex uses dyadic weights on original Float32 coordinates;
no rounded midpoint changes the source domain.

Let P_i and Q_i be the three source and target points of a fragment. Their
affine correspondence is P(t)=Σt_i P_i and Q(t)=Σt_i Q_i, for t_i≥0,Σt_i=1.
Convexity gives |P(t)-Q(t)|≤max_i|P_i-Q_i|. Q(t) stays in one original target
triangle. The maximum of the exact rational squared vertex distances therefore
bounds every source point's distance to the complete target mesh. Independent
nearest target triangles per source vertex would not suffice: a mesh with holes
can contain all three vertices and miss the triangle interior.

Closest-point queries select a potentially loose correspondence, not an optimum.
Only the exactly checked feasible weights establish the bound. Depth escalation
stops after a sufficient certificate or depth2; the best established bound is
kept. Degenerate/unreliable proposals or exhausted work limits remain unproved.
Poor conditioning rejects a proposal; it never relaxes geometric acceptance.

## Projection bound

For a paired point p in a camera with focal f and positive forward depth z,
write ρ=|p_xy|/z. The perspective difference to q=p+u satisfies

`|π(q)-π(p)| ≤ f |u| sqrt(1+ρ²)/(z-|u|)`.

Let `S=1+((width/2)²+(height/2)²)/f²`. Whenever p projects onto the specified
screen, ρ²≤S-1 and z≥|p|/sqrt(S). If d bounds |u| and D bounds |p| below,

`|π(q)-π(p)| ≤ f d S/(D-d sqrt(S))`, with positive denominator.

The joint source/target AABB supplies a conservative eye-to-geometry distance
lower bound D0. Every eye in the closed radius-r ball has D≥D0-r. The argument
holds for any camera rotation placing p on screen; q may project outside it.
It does not prove clipping, front-facing state, depth order or raster coverage.

Square roots receive outward rational brackets at a2⁻⁴⁰ step in their own units using
integer square roots. Displacement/sqrt(S) round upward and distance rounds
downward. This is bound arithmetic, not coordinate snapping or a new acceptance
tolerance. The one-pixel gate compares Fractions directly; displayed floats
never decide admission. The reported radius ceiling is sufficient only strictly
below its rational value and concerns this geometric direction alone.

## Work, evidence and limits

Inputs contain at most512 triangles each. At most4096 fragment/target pairs are
admitted before query operands are allocated. Counts distinguish proposed
triangle/point queries, exact vertex checks, logical operand extents and retained
witness arrays; these are not complete allocation/copy/cache-traffic counts.
Fraction/Trimesh scratch allocations remain unmeasured. Timings and process RSS
are host evidence, not native CPU/GPU/frame admission.

Tests include analytic translation, exact zero displacement, a missing-interior
counterexample, work limits, unresolved projection domains and adjacent-float
pixel thresholds. Independent dense controls interpolate stored witnesses and
project them from27 eye positions and three camera rotations per eye. Sampling
tests the implementation; the convexity and projection inequalities establish
the continuous domain. Diagnostics include actual native triangle projections.

No result admits visibility omissions, per-Piece opacity/culling, normal/UV/
material/PBR error, shadows, source LOD, native arithmetic or a whole capture's
movement radius. The parked layered/facing/mesh-expansion path in #12 is not
extended. Original triangles remain available for unsupported consumers.

Primary context: [Cohen et al., SIGGRAPH1996, Simplification Envelopes](https://www.cs.umd.edu/~varshney/papers/simp_env.pdf)
requires deviation in both directions. Its local paper is
`doc/references/geometry/siggraph/1996-simplification-envelopes.pdf`; pages1–3 and
the page2 assumptions were inspected. This control constructs its own local
correspondences and projection bound; it implements no envelope simplifier.
