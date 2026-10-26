# Spatial air sources and shadow sampling

Refs #320, #193, #266; goal #190. Weather explicitly confirms the source-point,
Direct/Psi and density-coordinate contract in #320/6199694036.

`airSourceTermsAt` exposes extinction, Direct and Diffuse separately.
`airSourceAt` recombines the original clear terms. A spatial consumer selects
`AIR_SOURCE_PROVIDER` before including `airPrefix.glsl` and passes the same
`AirSourceRay{OriginM, Direction}` to Begin and Advance. This is the metric
density-frame origin at atmosphere entry and a unit direction. A source point
is `OriginM + Direction * (distanceKm * 1000)`. Atmosphere entry and per-pixel
orthographic offsets must already be included, in the same frame as the Sun
and Weather's field/wind phase. The source provider acts before Bernstein fitting.

The explicit spatial overload is required for a custom source; the implicit
origin-free overloads remain available to existing clear consumers. No new
fields are stored in `AirPrefix`, and Medium80, Ray32, Sample32 and host bindings
are unchanged. The default remains 22 quadratic physical-ray intervals.
`AIR_PREFIX_GRID_PROVIDER(AirRay, uint index)` lets an actual source consumer
declare its physical boundaries; it must progress monotonically and reach the
end within `AIR_PREFIX_INTERVALS`. The standard partition's eight physical cuts
remain. Advance is bounded by that declared interval count plus physical cuts.
Its partition and source state do not depend on opaque or intermediate queries.

The experiment uses homogeneous air and prescribed constant solar/Psi inputs
to isolate spatial source fitting. Its absorbing cloud curtain is outside the
horizontal camera ray: cloud solar T is `exp(-tau * exp(-((x-c)/w)^2))` and camera
extinction stays clear. Direct receives cloud T; Diffuse remains prescribed.
It is a controlled RTE problem, not a globally converged solar/multiple-scatter LUT.
Clear coefficients are deliberately sampled at the eye in this control, while
the source's cloud factor uses its actual metric point. This special control
does not replace the production source's altitude dependence.

The independent reference integrates the clear source analytically and subtracts
a separately refined shadow deficit. It segments around the curtain, checks
64/128-node convergence, and preserves the 1% RGB target with a 1e-5 radiance
floor. Native camera T keeps its 2e-6 homogeneous control. The three geometry
families use a small sphere, a thin spherical atmosphere with about 100 km of
horizontal path, and Earth radii 6360/6460 km. Actual FP32 radii/extent are recorded.
135 controls vary curtain position, width and optical depth; three null-cloud
controls retain exact clear output. Sparse/repeated queries preserve full records
bitwise. Separate translated/reversed rays exercise the source origin/direction;
an intentionally wrong coordinate frame must fail. Unshadowed Direct, whole-source
dimming and whole-eye dimming are additional wrong controls, with the same bounds.

The native 256-cell logarithmic **experiment** resolves this declared cohort;
the original 22-cell partition misses narrow shadows on long physical paths.
The experimental profile permits at most `2 * 256 + 19 = 531` logical source
positions, including physical cuts. This is not a production budget increase:
existing clear callers retain their original partition and 63-position bound.
There is no selected runtime profile, full-resolution GPU-time result or general
source/field convergence claim. A callback count also does not measure compiler
hoisting of the prescribed constant inputs or actual hardware instructions.

Reproduce with Python 3/NumPy and glslang. `--probe` is the existing
`air_prefix_gpu.cpp` consumer, already linked against its matching regular archive;
the matching receipt must record that binary under `hashes`. The script verifies
its digest, compiles only kernels and does not relink new headers to an old archive:

```sh
python3 -m unittest discover -s test/experiments/render -p test_air_source_grid_reference.py
python3 test/experiments/render/air_source_grid_native.py --output /tmp/outshine-source-grids --probe /path/to/air-prefix-gpu --probe-receipt /path/to/receipt.json
python3 test/experiments/render/plot_air_source_grid_native.py --captures /tmp/outshine-source-grids --output /tmp/outshine-source-grids/comparison.pdf
```

The PDF additionally needs Matplotlib. All private inputs, SPIR-Vs, captures,
commands and hashes remain in the explicit system-temp output. The consumer's
submit/copy/compute/readback wall value is not dispatch GPU time.

For cost, compare source reuse before adopting more evaluations per pixel.
[Wronski, SIGGRAPH 2014](https://bartwronski.com/wp-content/uploads/2014/08/bwronski_volumetric_fog_siggraph2014.pdf),
pages 22–25 and 41, separates volume lighting from integration and stores shared
intermediate results. PDF pages 25/41 were inspected visually. Its local fog
range and quality are not evidence for Outshine's complete atmospheric extent.
Actual Weather density/Sun-T, field storage/interpolation, movement/history,
joint cloud/Thin consumption and complete Places/device costs remain open.
