# Same-contact needle direct light

Owner: `outshine_vegetation`; [#643](https://github.com/Weichwolf/outshine/issues/643).
Continues the [unshadowed angular counterstudy](needle_response.md).
This delivers a bounded opaque diffuse reference, not a physical foliage shader
or a compact angular product. No recipe/library/native/material bindings change.

## Model and reuse

Load only the frozen actual256-needle/full or35-needle/tail interval from #630:
positions, triangle indices and original corner normals. One immutable source
and one existing `ScatterScene`/Embree acceleration structure serve every view,
light and bias query in that process. No whole plant/axis/window plan is opened.
Actual source owner, axis30258/path(183,13,15), global first-needle IDs and the
source-file hashes are copied from the frozen interval into the report.

Existing `grass_projection.contacts` finds the foremost orthographic triangle;
the temporary projected geometry is translated above its required z=0 plane,
then contacts are transformed back. `surface_normal_transfer.interpolate_normals`
interpolates the original corners at those positions. A sheet viewed exactly
edge-on has zero projected coverage, no fabricated width and no light work.

For each actual camera contact, compute `c=max(dot(n,light),0)`.
Only positive contacts request a light ray from `point+bias*light` through the
same geometry, using existing bounded `ScatterScene.nearest` packets.
The ray length exceeds the source bounding-box diagonal; signed light directions
are supported. Flags outside positive contacts mean *unevaluated*, not visibility.
Within each projected pixel footprint, average `c*visibility`, with gaps zero.
This is source self-occlusion only; no neighbouring plants/terrain/external shadow.
Wood and foliage both use unit opaque diffuse response, no colour/material fit,
transmission or scattering; the common Lambertian `1/pi` is omitted.

For a deliberate incorrect alternative, multiply unshadowed mean response by
mean visibility **conditional on positive contacts in that same footprint**.
It uses the same rays and never interprets a culled nonpositive contact as clear.
An independent two-contact counterexample has cosines1/.5, visibility0/1 and
two additional gap samples: joint response.125, separated product.1875.
Thus even this conditional factorization loses the cosine/visibility covariance.

The filtering order follows Han et al., SIGGRAPH2007
[Frequency Domain Normal Map Filtering](../../../doc/references/vegetation/siggraph/2007-frequency-domain-normal-filtering.pdf),
page3/equations2–5, also inspected optically. The paper's normal-map convolution
does not itself account for three-dimensional joint visibility. Existing ray,
Float64 fallback and spatial filtering procedures are reused; no second raytracer
or transport operator is introduced.

## Independent geometry correction

The initial Embree primary-grid implementation missed full-interval pixel5955
at256², while both raster coverage and independent Float64 intersection retained
source face1229. This is an actual thin-triangle false miss, not a shading fit.
The final orthographic primary uses the existing raster path instead.
`data/needle-primary-thin-triangle.json` reduces that own procedural source to
11vertices/one triangle while retaining the relevant bounds. Its explicit ray,
Float64 hit and raster coverage are a permanent regression control.

For both final256² products, all three view coverage masks agree with a separate
`raster_batch.rasterize` replay; common interpolated normals differ by≤3.88e-8
(the comparison replay stores Float32 attributes).64 independent Float64 primary
probes per view agree. The previous missing pixel remains an explicit regression;
these probes do not prove every possible ray exact. Per light/bias, up to32
deterministically selected positive shadow rays also agree with Float64.

## Actual study and limits

Three normalized views:(1,0,.35), (0,1,.35), (1,1,.8).
Eight lights per view:camera/opposite plus six signed world axes.
Source grids128²/256²/512² and biases0.2/2/20µm are compared on unchanged geometry.
Saved products contain the actual contact IDs/positions/normals, positive-contact
cosines/flags and native64²/16²/4²/1pixel fields. Each source/view is fitted to its
own projected bounds: compare alternatives within a case, not absolute flux
between differently fitted sources/views.

At256² source sampling and2µm bias:

| Interval/view | Native16² product-of-means maximum error | RMSE |
|---|---:|---:|
|256needles/view0|.009912|.000807|
|256needles/view1|.018726|.001968|
|256needles/view2|.011620|.000593|
|35needles/view0|.035335|.001718|
|35needles/view1|.021043|.001475|
|35needles/view2|.040221|.001389|

In full/view0, positiveY-light native16² response sum falls6.942015→3.116881
with actual self-occlusion; tail9.063259→7.319705. Exactly coaxial frontlight
equals the unshadowed source in all six256² cases, as expected for visible opaque
contacts. Both2560×1440 comparison figures were opened at common linear exposure;
these are diagnostic finite-source renders, not a species/whole-tree beauty result.

Sampling is **not converged**: at native16², full/view1 maximum difference
128²→256² is.163164, then256²→512² is.083902; RMSE.019712→.007767.
Tail/view0 corresponding maxima.098693→.035928, RMSE.006875→.002476.
These differences preclude treating256² as a continuous oracle or fitting compact
angular coefficients to it as though source aliasing were negligible.
At512², bias changes alter up to21full/11tail contact flags in a light/view query;
maximum mean absolute response change is2.19e-5/9.99e-6 over the projected grid.
No bias-independence or fully converged precision is claimed.

## Work, storage and reproduction

One selected source decode:6,328,320B full/865,200B tail for the three arrays.
Encoded BVH input copies:1,062,912B/145,320B; internal kernel memory unmeasured.
The independent Float64 oracle/fallback retains7,004,160B/957,600B, explicitly
diagnostic/fallback storage rather than an accepted world budget.
At256², three primary grids each have65,536sample sites. Full raster accepted
fragment counts12,008/41,192/6,620 versus219,341/377,771/146,847bbox candidates.
Primary contact arrays retain334,260/941,340/179,040B (view0/1/2).
Only current-view contacts live across that view's light queries.

All24view/light pairs at three biases use290,928full/274,404tail light rays;
75packets each, maximum8192rays per packet. Explicit ray input conversion/copy
bytes8,145,984/7,683,312, not all temporary allocation traffic or peak RSS.
Float64 shadow probes add1,932/2,028rays and9,192/7,730exact candidate-triangle
tests; their broad-phase scans still visit the source bounds and are not free.
Raster projection copies, batch scratch, interpolation arrays and reporting are
separate work. Reports retain these counters and actual wall times; overlapping
nice10Linux runs are diagnostics, not isolated benchmarks or GPU/device evidence.
The full finite field arrays are proof outputs, not accepted per-world-area storage.

Private inputs/results/logs only under
`/tmp/outshine-vegetation-joint-light-proof-dc04c6080/` and the #630 source directory.
Core raster study frozen atb40e05c46; later edge-on handling changes only zero-area
projection. Fresh installed tail replay after that addition exactly matches every
array in all three views;39actual reference modules load from the installed wheel
and their source files match the checkout.13source/13installed controls pass.
No library code changed; reuse the existing verified wheel instead of rebuilding it.

With `.venv`, `TMPDIR=/tmp`, one NumPy thread and fresh explicit `/tmp` outputs:

```sh
PYTHONPATH=lib/src:test/experiments/vegetation .venv/bin/python \
  -m unittest needle_joint_checks needle_response_checks -v
PYTHONPATH=lib/src:test/experiments/vegetation .venv/bin/python \
  test/experiments/vegetation/needle_joint_study.py \
  --source /tmp/selected-needle-interval/physical.npz \
  --output /tmp/outshine-vegetation-joint-light-new --size 256
```

## Next decision

#643 remains active. Before an angular compression/default change, compare
projected-source-area integration against the present screen-grid integration
at equal error/work, reusing existing quadrature/triangle/ray primitives. Thin
source surfaces should not require uniformly refining empty screen area.
Existing collocation/first-exchange fields are not automatically an area-integral
oracle. Nonnegative directional approximation, further view/light sampling,
R/T/specular/roughness, multiple scattering, wind and actual complete-tree/stand
rendering remain separate unresolved requirements. No native/runtime/device gate
or SpeedTree-quality equivalence follows from these finite-source controls.
