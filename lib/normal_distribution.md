# Positive diffuse normal distributions

Owner: vegetation, [#643](https://github.com/Weichwolf/outshine/issues/643).
Module: `outshine_ref.vegetation.normal_distribution`. NumPy-only reference
mathematics; no geometry, renderer, source acquisition or cache dependency.
The [experiment](../test/experiments/vegetation/normal_lobes.md) owns actual
needle comparisons, source uncertainty, images and measured costs.

## Data and API

`normal_partition_ids(normals, count, frame)` accepts≤8192unit normals `(N,3)`.
`count=4` uses signs of the first two coordinates in the orthonormal supplied
frame. `6` uses the dominant signed object-space axis; `24` subdivides each
cube face2×2. Every24bin belongs to its parent6bin by integer division by4.
The frame/partition convention belongs to the product; do not silently mix
different quadrant frames or renumber bins when interpolating products.

For each pixel/bin, store `(W,Mx,My,Mz)`, where `W=sum(w)` and `M=sum(w*n)`.
Weights are nonnegative projected-footprint weights; unit normals are expressed
in one documented source frame. Values are already weighted, not conditional
normals or albedo. Empty bins have all zero entries. Moment fields are `(P,B,4)`;
vector fields `(P,B,3)`, `1≤P≤4096`, `1≤B≤24`. Light queries `(K,3)` are unit
vectors, `1≤K≤128`. Coefficients/mass are finite and bounded by2,000,000 to keep
the reference work/numerics finite; this is not a runtime/device residency budget.

`evaluate_diffuse(vectors, directions)` returns `(K,P)`:
`sum_bin(max(dot(vector_bin, light),0))`. It never normalizes the vectors.
Direction batches are≤8. Results are owned; inputs are not modified.

`jensen_error_bound(moments)` returns a per-pixel bound for that clamped-first-
moment approximation. It rejects negative/nonfinite mass and inconsistent
`||M||>W`, with2e-12relative tolerance for floating accumulation.

`mass_preserving_vectors(moments)` instead returns `W*M/||M||` per bin.
Its total vector length is `sum(W)`, preserving isotropic unshadowed diffuse
energy. It changes the first moment and can overestimate individual directions;
`mass_preserving_error_bound` bounds absolute discrepancy. A positive-mass bin
with exactly zero resultant is rejected, not assigned an invented direction.
Signed cube partitions avoid that degeneracy for valid unit-normal samples;
arbitrary quadrant measures need the explicit check.

`quantization_error_bound(original_vectors, encoded_vectors)` returns
`sum_bin(||encoded-original||)`, bounding any unit-light response difference
for directly encoded vectors. Matching shape and finite/bounded coefficients
are required. This does not certify quantized raw moments or subsequent
renormalization; FP16 tuples can leave the valid moment cone.

## Finite-measure derivation

For one bin, let `F(l)=sum(w*max(dot(n,l),0))`, `A(l)=max(dot(M,l),0)` and
`E=.5*sqrt(W*W-dot(M,M))`. For `W=0`, everything is zero.
Convexity gives `0≤F-A`. Since `max(x,0)=(x+abs(x))/2`,

`2*(F-A) = sum(w*abs(x))-abs(sum(w*x))`

`≤sum(w*abs(x-mean(x))) ≤sqrt(W*sum(w*(x-mean(x))²))`

`≤sqrt(W*sum(w*||n-M/W||²)) =sqrt(W*W-dot(M,M))`.

Sum the bounds over bins. Refining a partition cannot decrease `A`; the24-bin
lower approximation is therefore no darker than its nested6-bin parent.
The4-bin view partition is different, so no such ordering is claimed for4vs6.

For the mass-preserving variant, write `N=A+d`, where
`0≤d≤W-||M||`. The source lies in `[A,A+E]`, hence
`abs(F-N)≤max(E,W-||M||)` per bin; sum those terms.
Uniform-sphere mean of `max(dot(a,l),0)` is `||a||/4`, proving the isotropic
energy statement. Unnormalized moments can lose that energy despite being a
valid lower approximation.

Positive-part is1-Lipschitz; changing `M` by `delta` changes its response by
at most `||delta||`. This proves the direct-vector quantization bound.
All statements concern the same finite positive measure in real arithmetic.
Numerical checks allow Float64roundoff; these are not certified floating-point
or continuous-source, camera-visibility, shadow, material/PBR or world-error bounds.

## Filtering and scope

Mass and first moments are additive and may be box/bilinearly filtered with
nonnegative weights **before** resolving an approximation. Preserve frame/bin
semantics. The source exact response and moments must represent the same measure.
For mass preservation after filtering, normalize the filtered `(W,M)`; merely
filtering already normalized vectors can lose mass. MIP examples in the study
resolve each level afresh from filtered moments.

No light visibility, R/T, specular roughness, multiple scattering, wind or
colour/normal correlation is implied. These functions complement existing
`vegetation.leaf_normals` slope/roughness filtering; they do not replace it.
No native texture layout, shader binding or asset format is prescribed.
