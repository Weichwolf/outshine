# Weather shadows inside the clear-air source

Refs #320, #193, #266; goal #190. The confirmed coordinate/source contract is
[#320/comment6199694036](https://github.com/Weichwolf/outshine/issues/320#issuecomment-6199694036).
The composition experiment consumes the actual Weather density and its existing
six-midpoint solar optical depth at each air source position. Only DirectEmission
receives this additional transfer; DiffuseEmission/Psi and extinction keep their
original meaning. Source fitting follows this composition. The sun irradiance
scale is applied once by the existing cloud recipe and later runtime consumers.

`cloud_air_source.glsl` owns the composition bridge beside the sole mixed-prefix
experiment consumer. Weather's generator recipe and the generic render integrator
keep their module boundary. The existing mixed adapter forwards its actual
`Cloud.OriginM + Cloud.Direction * ClearBeginM` and unit direction to the air
prefix. No new host uniform, resource binding, callback registry or prefix field.
This is the existing fixed spherical density/tangent frame, with entry and any
pixel-origin offset already included; ENU conversion remains the caller's job.

The independent control holds solar/Psi textures and homogeneous clear
coefficients prescribed to isolate direct-source composition. It uses the actual
procedural cloud field, 16 declared constant cloud cells per shell interval and
the existing approximate cloud source. An independent RTE integral resolves the
spatial air source with Gauss8/16 over metric25m segments and physical/query/cloud
boundaries. This is independent of the native Bernstein fit and cell quadrature.
It does not claim globally converged atmospheric LUTs or a refined cloud medium.
The positive sun direction and declared geometry remain unoccluded by the planet
within the tested air path. Separable Sun-T6 error remains with Weather #264.

The cohort contains 26 rays: four eye heights, three slopes, two azimuths and two
orbital downward rays. Their physical extent reaches the atmospheric/planet end;
surface queries do not shorten the source grid. There are 81 duplicated queries
per ray, three cloud covers and sparse schedules. Additional wrong controls omit
the shadow, dim the whole source or discard the actual source origin.
Null clouds preserve full native records. The experimental logarithmic grid
compares 64/128/256 cells with the original22 quadratic cells. No production
sample-count change or whole-frame device-budget claim follows from this cohort.

The experiment reports actual air-source positions, union cells and cloud
density/source counts. The additional solar density-work bound is at most
12 evaluations per air-source position (two intervals times six samples); this
is an upper bound, not measured GPU instructions/time. No GPU allocation is added
by the bridge. Compiler registers/spills, complete frame timing and cache/reuse
storage remain unproven. The native submit/copy/compute/readback wall time is
not dispatch GPU time. Shared source work needs its own measured comparison.

Reproduce with Python3/NumPy/Pillow, glslang and the existing static regular-archive
orbit-capable prefix consumer. Its matching receipt must contain
`consumer_sha256`; the runner verifies it and only compiles current kernels.
It does not link new headers against old archives:

```sh
python3 -m unittest discover -s test/experiments/weather -p test_cloud_air_source_reference.py
python3 test/experiments/weather/cloud_air_source_native.py --output /tmp/outshine-weather-air-source --probe /path/to/air-prefix-gpu-orbit --probe-receipt /path/to/receipt.json
python3 test/experiments/weather/plot_cloud_air_source.py --captures /tmp/outshine-weather-air-source --output /tmp/outshine-weather-air-source/comparison.pdf
```

Matplotlib is needed for the PDF. It visualizes actual GPU transfer records with
the same constant background/exposure; it is not a native Place/raster capture.
Source hashes, exact commands, inputs and readbacks remain in system-temp output.
The source-reuse investigation starts from Wronski's SIGGRAPH2014 volume lighting
and integration, already visually inspected and linked in `../render/air_source_grids.md`.
Its local fog range is not evidence for a full Earth volume. Real runtime/Thin,
history/movement, diffuse cloud response, Places and device budgets remain open.
