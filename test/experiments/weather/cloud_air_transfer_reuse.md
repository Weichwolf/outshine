# Sharing shadowed air transfer

Refs #353/#320, goal #190. This experiment measures a possible shared **air**
component, not a complete medium/renderer profile. The actual eye-cloud transfer
cannot simply be filtered across pixels: the supplied full-image controls and
independent witnesses deliberately reject that shortcut.

The current control has homogeneous clear-air coefficients and the same constant
solar/Psi inputs as #350/#363. Weather's actual density, wind phase37s and six-tap
solar transfer still shade Direct at each physical source. Camera extinction and
cloud emission can be disabled independently with experiment-only
`SOURCE_EYE_CLOUD_COVER=0`; the default remains the original coverage. The FP64
reference exposes the same independent eye-cover switch, with its old default.
Neither switch changes Weather's recipe or production shaders.

For homogeneous air, extinction `sigma`, direct coefficient `D` and diffuse
coefficient `P` are constant along a fixed ray. Let `s` be distance from actual
atmosphere entry, `q` the query clamped by the physical atmosphere/planet, and
`C(s)` the actual cloud solar transfer:

```text
I(q) = integral(0..q, exp(-sigma*s) ds)
V(q) = integral(0..q, exp(-sigma*s)*C(s) ds) / I(q)
L(q) = I(q) * (D*V(q) + P)
T(q) = exp(-sigma*q)
```

`V` is RGB because extinction weights differ by channel. It is a camera-path
weighted visibility, not Weather's point solar transfer. The coarse native
producer evaluates the existing full-physical-partition air prefix at selected
image rays. The CPU extracts `V` without clamping away errors and uploads one
RGBA32F angular field. The native reconstruction samples that field, computes
the actual pixel's entry/planet clip, phase and analytic homogeneous response,
then composes Direct and Diffuse in their original units. It does not interpolate
the clipped optical path or dim Psi. Grid nodes include both final image edges;
the final, shorter cell has its own correct interpolation weight.

The fixed control is1920×1080,55° vertical FOV, eye1772m, west-facing,10km query,
and the same10000km physical ray request as #363. Raster steps16/8/4 compare raw
joint L/T, raw air L/T and factored air. The native coarse rays must match their
full-image numerical records exactly. Separate source counts, field bytes,
actual producer/reconstruction GPU timestamp intervals and NVML snapshots are
recorded. Independent Gauss16/32 RTE witnesses include the worst interpolation
changes, the largest T discrepancy and twelve fixed image locations. Every
reconstructed pixel additionally compares T to its independent closed form.
RGB remains1% with1e-5 floor; analytic T remains2e-6 absolute. The old native
prefix is a comparison, not the optical-depth oracle: its accumulated FP32 T can
itself differ from the closed form. Both differences stay in the report.

This field is tied to one query distance and one camera/Sun/density/wind state.
It is **not** a valid API for arbitrary opaque or Thin queries. It is also not a
solution for joint cloud/air overlap: multiplying the resulting air L by a cloud
factor afterwards loses the distribution of source light along the path.
Sharing depth-dependent source/transfer information and mixing it with the
actual eye-cloud cells remains the next required step, with #344/#266 consumers.
General altitude-dependent atmosphere, night/low Sun, translation, state updates,
temporal stability, full physical depth coverage and null-cloud native profiles
remain separate requirements. No production raster, resource/history manager,
cache or public API is introduced.

The current prototype deliberately reads producer values back, normalizes them
on CPU and uploads the field. Summed producer and reconstruction GPU durations
exclude this gap and are not end-to-end frame times. A GPU producer, upload/
readback/normalization costs, actual peak memory, full renderer and device p99
still need proof before production adoption. CPU/compiler workers run nice10;
the controller and measured native processes retain normal priority.

Reproduce with Python3/NumPy, the project C++ toolchain and glslang:

```sh
python3 -m unittest discover -s test/experiments/weather -p test_cloud_air_transfer_reuse.py
python3 test/experiments/weather/cloud_air_transfer_native.py --output /tmp/outshine-air-transfer --api-root /path/to/clean-producer --archive /path/to/liboutshine.a --archive-receipt /path/to/receipt.json
python3 test/experiments/weather/plot_cloud_air_transfer.py --captures /tmp/outshine-air-transfer --output /tmp/outshine-air-transfer/comparison.pdf
```

The archive receipt must match its clean API checkout and SHA256. Current
consumer code compiles against those matching headers. Receipts preserve
commands/exit status, input/output/SPIR-V/generated-metadata and source hashes.
Plots additionally require Matplotlib. Private results belong in system temp;
no oracle or gallery is changed.

[Wronski, SIGGRAPH2014](https://bartwronski.com/wp-content/uploads/2014/08/bwronski_volumetric_fog_siggraph2014.pdf)
separates lighting and integration. Pages25/41 were inspected optically again.
Its local fog range does not prove full-Earth convergence. The weighted-visibility
factorization above is this homogeneous control's algebra, not a claim that the
paper's volume representation has already been integrated.
