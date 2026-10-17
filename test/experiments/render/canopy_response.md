# Fixed-footprint material response — #451

The missing grass stand needs conditional light/view response, not a colour-only
cover correction. This bounded reduction reuses Vegetation's #468/#477/#481
geometry, sheet R/T and first-neighbour operator. It does not add another ray
solver. One fixed1m² ground footprint, east view and six fixed upper beams are
retained; only uniform leaf RGB reflection R, transmission T, ground albedo g
and current beam RGB energy vary. No native shader or shared contract changes.

## Derivation and source

For each RGB channel let x=(R,T,g). With fixed opaque geometry, direct nodal
radiance is D_l=G_l*x for beam l. The existing geometric operator F transfers
that field. Receiver R uses its own outgoing side, T the opposite side, and g
only ground nodes. Let W_i encode the saved primary-contact weights for each
receiver material/side; w contains their complete first-contact distribution.

    a_lj = wᵀ G_lj
    b_lij = W_iᵀ F G_lj
    mean radiance = sum_l E_l [sum_j a_lj*x_j + sum_ij b_lij*x_i*x_j].

This is exact algebra for the defined nodal D+KD model before rounding. It is
not an independent-coverage approximation, a higher-order transport solution,
or proof that centroid direct lighting matches actual-contact direct lighting.
The primary distribution includes actual saved gaps/face IDs/normals; changing
view, footprint, geometry or beam directions requires another valid coefficient
set. Changing optical values or beam RGB does not rebuild geometry.

First technique source: Sloan/Kautz/Snyder, SIGGRAPH2002,
[Precomputed Radiance Transfer](../../../doc/references/lighting/siggraph/2002-precomputed-radiance-transfer.pdf).
PDF page2/Figure2 opened optically: transfer and current illumination are separate.
The material polynomial above is our derivation; no SH projection, interpolation
or complete PRT method is implemented. Primary source:
[Microsoft Research](https://www.microsoft.com/en-us/research/publication/precomputed-radiance-transfer-real-time-rendering-dynamic-low-frequency-lighting-environments/).

## Reproduction and evidence

Existing experiment dependencies plus NumPy/SciPy/embreex; one numerical thread:

```sh
PYTHONPATH=lib/src:lib/tests:test/experiments/vegetation OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1 \
  python test/experiments/vegetation/grass_projection_experiment.py --output /tmp/outshine-material-contacts
PYTHONPATH=lib/src:test/experiments/vegetation OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1 \
  python test/experiments/render/canopy_response.py --inputs /tmp/outshine-material-contacts --output /tmp/outshine-material-response
```

Inputs were freshly reproduced from source5aeb4f802:2368 roots,284160 blade
triangles, full284162-face finite mesh,288256 collocation nodes. Current stand
identity f38e50baa1a962e03a097b3c819978ecdf36c841a1372bae89014a1238a30bab;
full geometry9f0a81299452bbdb6399e3613aca0808dfc49860b883693fb35859ea22b831bc.
This is not a claimed byte-identity check against historical #486 outputs.
Existing contact reconstruction again passes all196608 union memberships and192
independent rays. Mapped east contacts cover85.9787% of the footprint.

Twenty deterministic bounded RGB material/light queries, including black, agree
against the full existing CSR+mix_sides evaluation: maximum absolute error
3.2752e−15 linear RGB, preset limit1e−12. Float32 coefficient storage with Float64
evaluation gives5.9535e−9, preset1e−6. Float32 arithmetic on GPU is not tested.
The numerical comparison PDF is optically opened; it is no physical scene image.

Six beams ×(3 linear+9 quadratic) scalar coefficients require288B Float32
payload, excluding identity/direction metadata. CSR arrays alone are23595408B;
links9224192B, six direct geometry-field arrays83017728B. Known coefficient
preparation holds the direct fields and makes two additional83017728B field
copies during basis conversion; one
incoming RGB basis and receiver weights each13836288B. Internal Embree/SciPy/
NumPy allocation and traffic remain unmeasured. This replaces only the defined
fixed query, not the complete operator for arbitrary views or heterogeneous
materials. The existing operator remains the reference.

Original contacts: `/tmp/outshine-material-canopy-response-inputs-5aeb4f802/`.
Results: `/tmp/outshine-material-canopy-response-final-5aeb4f802/`;
PDF, raw logs and cost receipt: `/tmp/outshine-material-resume-20261017/`.
The resource audit exits0:23.08s wall,23.16CPU-s,485072896B peak child RSS
(Linux getrusage). Preparation is12.10s; full material/light query median531.98ms
versus51.82µs reduced across20 states. The full timing includes current direct
field combination, existing CSR application, R/T mix and primary weighted mean;
the reduced timing includes current RGB material and beam evaluation. Neither
includes IO in its query timing. No geometry or field data is loaded during a
reduced query. These nonisolated CPU observations are not native/frame gates.
The serialized NPZ currently retains Float64 reference coefficients;288B names
only the tested Float32 numeric payload, not its container or metadata size.

## Still open

World-space filtered products, near/far handoff, angle changes/interpolation,
heterogeneous green/dry blades and substrate, material detail/specular response,
higher scattering orders, wind, native execution and full Places/budgets remain
open. No world-scale storage estimate or natural-grass admission follows from
288B for this one footprint/view/six-beam case. #451 and #189 remain active.

The fixed-view spatial/native extension is documented in [canopy_field.md](canopy_field.md);
its bounded GPU field is still not a world material or an angular/near-far admission.
