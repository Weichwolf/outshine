# Solar optical depth at a fixed sample budget (#286)

The old 40 uniform midpoint samples miss the thin ground aerosol layer on long
outward rays. This changes the solar source before the finite-air fit: correcting
only captured solar transmission reduced the dense-haze light error from
3.325664% to 0.283346%. Changing the final prefix expectations would hide this bug.

The solar LUT follows Hillaire, *A Scalable and Production Ready Sky and Atmosphere
Rendering Technique* (EGSR 2020), section 5.2; the local PDF's page 4 was inspected.
The adaptive quadrature below is our measured numerical choice, not a claim from
that paper. No new radiative-transfer model, medium parameters or texture layout.

## Selected rule

Keep the old uniform rule for resolved rays. For an outward ray with aerosol,
estimate its initial Mie fraction `f = sigmaM / min(sigmaRGB)`. For `N=40`
samples, let `C=floor(N/2)`, initial cell width `d=span/C` and
`v=cosZenith*d/MieScaleHeight*sqrt(f)`. Use focus
`q=max(0,1-0.25/max(0.25,v))`. The dimensionless 0.25 is a measured density
variation target, not a certified integration-error bound.

If `q>0`, partition the *physical distance* using
`s(u)=span*u*(1+q*(u-1))`. Integrate each cell with positive two-point Gauss
weights: its midpoint plus/minus half-width/sqrt(3), each weighted by half-width.
This gives 40 extinction evaluations, rather than the 80/160 alternatives.
Odd budgets use `2*floor(N/2)` samples in this branch; one sample keeps the old
rule and zero samples returns identity. No division by zero is evaluated.

Clear and inward rays skip the estimate entirely. Other outward rays add one
initial RGB extinction estimate and one extra scalar Mie-density evaluation.
No heap allocation, copied bytes, textures or retained storage are added to the
integration; CPU depth uses three doubles and GPU depth three floats as before.
The extra scalar arithmetic/branching is not free. Queue/readback wall time is
not a GPU timestamp or a frame-budget proof.

`AtmosphereCore.h` shares focus and physical edges between C++ and GLSL.
C++ `MediumTransmittance` and both GLSL callers consume the same rule.

## Equal-input comparison

`compare_solar_quadrature.py` compares seven rules against independent FP64
integration split at physical density/ozone boundaries and the tangent. Independent
32/64-point refinement must agree within absolute T=1e-7. The full comparison uses
49,152 native FP32 LUT looks for each Haze0/0.25/1/4, including high inward looks.

| Haze | Uniform40 max abs T | Selected40 max abs T | Actual GPU RGBA16F max abs T |
|---|---:|---:|---:|
| 0 | 0.0013061 | 0.0013061 | 0.0012933 |
| 0.25 | 0.0065404 | 0.0009741 | 0.0011266 |
| 1 | 0.0199256 | 0.0009741 | 0.0010520 |
| 4 | 0.0435656 | 0.0009740 | 0.0010556 |

The first two columns use FP64 integration of identical quantized native looks;
the third includes actual GPU geometry/FP16 storage error. Native CPU FP32 results
are also compared separately. These are 768×64 candidate-LUT controls, not a
claim that all LUT resolutions or the full renderer have passed.

Always-quadratic midpoint40 worsens the clear maximum. Always-Gauss40 passes the
optical comparison but makes the existing dense finite-prefix control exceed its
unchanged 1% light target (1.006665%). The selected rule preserves resolved clear
paths and passes that dense control at 0.964685% exactly as before. For actual
production solar compute → RGBA16F → native prefix, Haze4 improves from 3.325664%
to 0.362106%; Haze0/0.25/1 give 0.132592%/0.210340%/0.249123%. All original
expectations, repeated-query identities and source-position bounds are retained.
Psi is zero in these physical controls; this does not prove multiple scattering.

## Reproduction and limits

Build `air_prefix_luts.cpp` with the normal C++23 include paths. For each haze,
export to an explicit system-temp `haze-<value>` directory, with `768 64 <haze>`.
Use these same Medium80/looks inputs for the comparison:

```sh
python3 test/experiments/render/compare_solar_quadrature.py \
  --inputs "$solar_inputs" --output "$solar_output/comparison.json" --stride 1 \
  --cpu-captures "$solar_cpu" --native-captures "$solar_native"
```

Capture roots contain `haze-<value>/veil-native-f32.bin` (CPU) or
`haze-<value>/samples.bin.veil` (actual GPU RGBA16F). `--width` selects the input
width; native height is 64. Capture options are optional. The script refuses
outputs inside the checkout. NumPy is required; receipts hash inputs and captures.

The existing `air_prefix_gpu.cpp` consumer accepts an optional final solar SPIR-V
argument and saves the actual LUT alongside its prefix output. Use its regular,
matching native archive/header build, or a hash-verified already linked static
consumer. Never relink new native headers against an older Present ABI.
`check_air_prefix_gpu.py` compares readbacks against the original fixtures.

`MediumTransmittance/VerticalOpticalDepth.cpp` checks the independent analytic
exponential and triangular ozone column. Old production40 fails 30 checks; the
selected rule passes all 684. No expectation or 0.002 transmission limit changed.

Additional actual 256×64 captures retain the original dense clear-prefix failure:
2.038785% before and after, byte-identical output. At that width Haze4 improves
from 2.942506% to 0.992225%; the other vertical haze controls also pass. This
pre-existing interpolation error stays red under #259/#206; it is not an excuse
to increase the 1% target or claim the candidate-wide LUT is already integrated.

Full clear/hazy Places, temporal transitions, direct device times and Metal/A18
counterchecks remain open under #286/#206. Master publishes integrated Places;
no specialist image publication under doc/. The finite-aerial/Thin/cloud ordering
contracts remain separate under #266/#193. This correction does not merge #259
or close either broader feature.

The later [solar-lookup comparison](solar_lookup.md) addresses the remaining
interpolation/domain error with explicit physical endpoints and 512×64 storage.
It preserves the integration rule above. The numerical captures here belong to
the original #307 source/input freeze, not the later endpoint producer.
