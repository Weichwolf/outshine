# Coating and opening covariance

[#278](https://github.com/Weichwolf/outshine/issues/278) investigates the uniform
opaque regions visible in the native Darmstadt gallery. This experiment uses
the shipped native wall colour and House/Commercial profiles; the pixel shapes
are explicit synthetic counterexamples. It does not contain Place geometry,
change runtime shaders, or establish a native image/cost result.

The proposed field is the existing three-dimensional SurfaceNoise at a1.5m
scale, centred around zero, with0.16 relative wall-colour contrast. Its intended
mean is the existing source wall colour. The study isolates the additional
colour contribution on opaque wall area; glass/frame constants cancel. It
omits BRDF changes, physical colour-range policy, seed/frame lifecycle, far
fade and temporal filtering, which remain necessary before production use.

Three alternatives receive identical positions/derivatives and material values:

- Filter the field with1/4/9 Gauss samples, then multiply by the current projected
  box's wall coverage. These two independently averaged factors lose covariance.
- Take1/4/9 joint point samples of field times the actual pointwise wall mask.
  The hard window edge becomes a point-sampling problem.
- Partition the actual pixel parallelogram into4/9/16 equal subcells. Each field
  midpoint is weighted by that subcell's analytically filtered projected-box
  wall coverage. These boxes still approximate rotated/sheared subcell shapes.

There are720 pixel cases per profile:16 metric positions, three rotations,
five widths and three shears on one fixed oriented vertical wall in3D. Native
profile dimensions/fractions and wall colour are parsed from their real source
files; the expected wall colour is checked explicitly. The shared reference
library supplies the same analytic rectangular opening integration as the
current native model. The independent target instead integrates the actual
parallelogram's smooth field times the pointwise opaque region using uniform
midpoint areas, at32/64/128 samples per axis for the full matrix. The union of
each method's eight largest128-reference errors is refined to256:35 House and
39 Commercial cases. This is numerical refinement on a finite dataset, not
an exact integral, interval bound or global optimum.

Largest selected error in linear RGB against the256×256 reference:

| Method | Noise corner hashes | House | Commercial |
| --- | ---: | ---: | ---: |
| Point field × box coverage |8|0.025235|0.025235|
| Nine filtered field samples × box coverage |72|0.008057|0.007925|
| Nine joint point samples |72|0.004544|0.004135|
| Nine filtered subcells |72|0.002399|0.001559|
| Sixteen filtered subcells |128|0.001301|0.001041|

The same128×128 reference also records E[field], E[wall] and E[field×wall]
separately. Their covariance contributes up to0.006386/0.006525 RGB; even the
axis-aligned subset, where the projected box is the actual pixel rectangle,
has0.002672/0.004558 bias. This isolates factorization from the geometric box
approximation. The latter separately differs from the sampled true wall area
by up to0.23205/0.24644 (fractions, not relative percentages) in this matrix.
The candidate errors therefore contain both covariance and footprint-shape
error; neither cause is attributed exclusively to the other.

The64→128 reference change is at most0.000170/0.000263 RGB; selected128→256
change0.0000851/0.0001253. Selected refinements are not a global error bar.
At the same nine field evaluations, filtered subcells reduce the observed
worst error more than joint point samples. Increasing field samples alone
leaves the covariance problem. Sixteen subcells are the next native comparison
candidate; no runtime winner or accepted frame budget is declared.
Decision: do not add a runtime coating field yet. First compare a shared native
facade-coverage filter against the retained cases and measure its actual work;
then evaluate a joint coating consumer and its physical colour bounds. Extra
noise work cannot repair the existing opening-footprint error by itself.

Hash counts are source work counts, excluding opening-filter arithmetic,
compiler reuse and machine instructions. Python elapsed values are one
vectorized CPU observation at nice10, not a fair shader/device cost ranking.
There is no GPU-time, temporal-stability, A18 or lit-Place result here.

```sh
PYTHONPATH=lib/src python3 test/experiments/render/facade_coating.py --output /tmp/outshine-material-facade-coating-278 --plot
```

Use the reference library's declared Python dependencies. This mathematical
path requires NumPy; importing the package also requires Shapely. `--plot`
additionally needs Matplotlib and produces standalone PNG/SVG. The checked run
used a private system-site-packages environment with Shapely/Matplotlib installed
in system temp. No dependency or downloaded code is vendored in the repository.
Outputs require an explicit system-temp path; repository output is rejected.
Reports identify source hashes. Compressed arrays retain actual cases,
candidate values and refinement selections for inspection; normal tests do not
regenerate output pins. Optional plotting is an experiment artifact, not a
README Place-gallery replacement.

Before native activation: verify candidate shader values and actual work,
check motion and real source/frame bounds, select physical colour parameters,
and compare identical lit Places at the agreed profile. The pending native
raster path remains with #240/#243; this study introduces no parallel renderer.
