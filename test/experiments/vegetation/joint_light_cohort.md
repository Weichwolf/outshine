# Joint-light modes on additional source cameras

Owner: vegetation, [#643](https://github.com/Weichwolf/outshine/issues/643).
Follow-up to [joint angular/footprint reduction](joint_light_basis.md).
The compact opaque response remains experimental; no material/default/library
or native ABI change, no species/whole-tree acceptance or claimed beauty gain.

## Fixed source cohort and existing procedures

Use the same seed718, needle meshes, physical dimensions, spacing and pattern.
Bake canonical2-,8-,32-needle sources with the original three camera directions.
Then use the **identical32source** with normalized additional directions
(-1,.1,.2),(.4,-.7,.5),(.5,.3,-.8). This is twelve camera products, not twelve
independent species/recipes. Each case retains its own fitted projected bounds;
do not compare absolute world flux between different sources' charts.

Reuse the area integrator, same positive camera contacts, Embree/Float64 fallback,
64view-relative light knots and six independent evaluation lights plus two pole
controls from the previous study. No extra source caster or raytracer. All new
tables use **only the six independent lights**, not the training pole controls.

The optional `--normals` collector is the existing `NormalMeasure`, four view
quadrants. It receives those same area-weighted visible normals and only one
co-axial diagnostic light; all64/8unshadowed fields already exist in JointMeasure.
Normal collection adds no camera clipping/visibility pass or shadow rays. Its
mass agrees with W; the four-lobe response stays below the exact finite
unshadowed response. The original32case's18old fields remain array-exact versus
the previous#675product. The three added fields are raw four-lobe moments.

Compare normalized mean, positive four-lobe first moments without internal
self-shadow, exact unshadowed response and joint source response. The previous
local SIGGRAPH2002/2007 sources, inspected optically, and finite-measure limits
remain in [the preceding report](joint_light_basis.md) and
[normal_lobes.md](normal_lobes.md); this is no new general transport operator.

## What is and is not held out

At each footprint, fit angular SVD modes using either the original32source's
three cameras or pooled32/2/8sources' nine cameras. Let angular rows V be
orthonormal. For the target camera's known64-light matrix X, spatial coefficients
are `X @ V.T`. Query only through the existing positive angular cone weights,
then project final response to[0,W]. Evaluation light values do not enter fitting.

The new cameras do not train the shared angular modes, but **their own spatial
maps still require their64-light source bake**. This is not reconstruction of an
unseen view without its source, continuous view interpolation or a runtime cache
miss measurement. Pooling more cameras helps some errors and worsens others.
At1pixel the original fit has at most3modes, the pooled fit at most9; requested
rank8/12is not falsely counted as eight/twelve actual stored modes there.

Existing nonnegative2×2box filtering coarsens raw joint fields, W and raw normal
moments before evaluation/fitting. No average of conditional responses, no
independent visibility product, no camera-frame mixing. Signed coefficients
remain linear until final interval projection. MIP/LOD motion is still untested.

## Additional-camera results

Same finite order4source, all three additional cameras/six independent lights.
Errors are absolute unit-diffuse response, common1/pi omitted. Percentages are
RMSE divided by RMS projected coverage, not percent of each dark pixel.

| Footprint | Four-lobe max | Pooled≤12modes max | Four-lobe RMSE/RMS W | Pooled≤12 RMSE/RMS W |
|---|---:|---:|---:|---:|
|16×16|.156259|.121215|5.766%|4.530%|
|8×8|.057540|.042175|4.324%|3.191%|
|4×4|.023056|.013680|3.513%|2.371%|
|2×2|.008654|.005522|2.825%|2.269%|
|1×1|.004243|.004957|2.344%|2.094%|

At4², four-lobe RMSE.0046994→pooled12RMSE.0031712, a32.5%reduction on this
finite cohort. That is a measured representation improvement, not an improved
whole tree or physical R/T material. The remaining source error is still visible
in magnified fields. At1pixel pooled12reduces RMSE but **worsens maximum error**.

The original32three-camera modes give1pixel max.008447/RMSE.002576, worse than
four lobes' .004243/.002332. This is an actual generalization counterexample to
adopting the earlier coarse low-rank fit unconditionally. At4² pooled8RMSE.003524
is slightly worse than original32modes' .003462, despite more training sources.
Training Frobenius error is not held-out angular/image quality.

Exact unshadowed4²response has RMSE.004978, slightly worse than four lobes'.004699:
normal underestimation can accidentally offset omitted self-shadow. That does
not make normal lobes a visibility model. Normalized-mean4²RMSE is.034509 and
remains a rejected baseline, not a proposed brightness adjustment.

On these additional cameras, observed order2→4query differences are:

| Footprint | Maximum difference | RMSE |
|---|---:|---:|
|16×16|.010534|.0009272|
|8×8|.005407|.0004882|
|4×4|.001585|.0002439|
|2×2|.0003415|.0001146|
|1×1|.00009140|.00003457|

The4²comparison gain is larger than these observed quadrature differences.
Neither order4nor their difference certifies continuous-source convergence.
No global/perceptual/continuous-angle optimum or foliage quality approval.

## Cost and checks

The unchanged8,000,000contact×light cap remains enforced **before quadrature**.
Order4candidate counts2needles9,136–12,512;8needles26,656–29,296;
original32source93,328–99,072; additional32cameras94,720–96,896. All fit the
111,111candidate limit for72queries without raising it. The twelve camera
products issue30,355,019actual rays. Each case uses its own one-time source/BVH;
all its cameras/lights reuse it. This remains expensive offline research work.

Joint collector fields591,872B/camera, plus34,816B for the optional normal
collector. At4², target12mode maps use2,304Float32bytes; shared angular modes
use3,072B. Existing four3-vector lobe maps at that **same footprint** also use
2,304B. Coverage, chart/view data, containers and staging are extra. General
normal/material data remain needed for R/T/specular, so the joint fields cannot
be presented as replacing the entire normal/material payload or as a free gain.

One fresh installed additional32/Q4bake, Linux/WSL4CPU, one numerical worker,
overlapping order2analysis:8.940s wall,8.853s user,0.116s system; process peak RSS
142,635,008B via Linux `resource.getrusage`, including imports/source/BVH/reduction.
Not device/GPU residency, allocation count, p99, native or whole-world budget.
No isolated speedup is inferred. Prior exact work/copy counters remain separate.

88source/88installed controls pass.64actual outshine_ref import paths/source
bytes match the unchanged#667wheel, SHA256
`e73361c9b8df238432fb00e2c97eadf6bddaac60e5aa883fcafbdd7b038a642e`.
Fresh installed bake preserves physical source bytes, all21source fields and
30initial reduction arrays. The subsequent45cohort arrays and all level metrics
are exact in installed replay. Added independent controls cover new spatial
rows in a known light subspace, zero gaps, actual rank/bytes, distinct shadow
and normal responses through every footprint, simultaneous collectors without
changing old output, and invalid cameras refused before source construction.

Two2560×1440figures actually opened: fixed5°light/three additional cameras at
native4², and errors across all footprints. These are magnified measured source
fields; no new whole-tree render, changed geometry, image gallery or server.
Source-field code snapshotd90aaa637; analysis/figure snapshot25ef05475. Later
changes only extend rejection of nonnormalizable cameras and control coverage.

## Reproduction and next decision

All original inputs, logs, private figures, environments and outputs stay under
`/tmp`; the current evidence root is
`/tmp/outshine-vegetation-joint-cohort-proof-8cc5686c0/`. Original model/catalog
paths are in [joint_light_basis.md](joint_light_basis.md) and retained unchanged.

```sh
TMPDIR=/tmp OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1 \
PYTHONPATH=lib/src:test/experiments/vegetation .venv/bin/python \
  test/experiments/vegetation/joint_basis_study.py \
  --models /tmp/existing-models --catalog /tmp/existing-catalog \
  --output /tmp/outshine-new-cameras --capacity 32 --order 4 \
  --angles view --knots 64 --normals \
  --views '[[-1,0.1,0.2],[0.4,-0.7,0.5],[0.5,0.3,-0.8]]'
PYTHONPATH=lib/src:test/experiments/vegetation .venv/bin/python \
  test/experiments/vegetation/joint_basis_cohort.py \
  --training /tmp/existing-original32 /tmp/existing-two /tmp/existing-eight \
  --evaluation /tmp/outshine-new-cameras --output /tmp/outshine-cohort-new
PYTHONPATH=lib/src:test/experiments/vegetation .venv/bin/python \
  test/experiments/vegetation/joint_cohort_plot.py \
  --source /tmp/outshine-cohort-new --output /tmp/outshine-cohort-new.png
```

Retain the four-lobe reference and the bounded joint-source controls. Do not
rebake a full512view angular bank or adopt a universal low-rank material from
this cohort. The next useful physical extension is R/T on actual visible source
contacts with the existing optical/material procedures, preserving correlation
before filtering. Specular, wind, external shadows, view/LOD motion, mature
forms and complete stands remain open; native integration continues independently.

The next [two-sided source-surface study](sheet_area_light.md) applies the existing
diffuse optical model to actual open leaves; it does not assign it to closed needles.
