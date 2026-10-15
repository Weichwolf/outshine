# Solar-column reuse study

Refs #353, #320, #264; goal #190. This is a measured comparison, not an enabled
runtime profile. The 256-cell per-pixel path remains experimental. A local solar
map reduces repeated cloud-density work, but its complete full-size test must
meet the same RGB limit and the target's complete frame budget.

For a visible solar ray starting below the cloud's inner spherical shell, every
receiver on that same solar line sees the same complete forward cloud column.
Project the receiver onto axes perpendicular to the Sun. The producer finds the
Sun-facing inner-shell intersection and starts one metre before it. This avoids
the invalid assumption that one fixed planar start precedes every cloud column.
Column invariance is checked at Sun cosines0.8/0.2/0.02 with actual Weather density.
It preserves the existing six-sample quadrature; it does not prove its physical
convergence. A supplied low-sun refinement witness challenges that separately.

The experiment compares optical-depth and transmittance interpolation before
testing three local transmittance levels. Their2/16/128m spacings scale along the
solar-horizontal axis by Sun.y. Each513×513 R32F level shares the camera's projected
anchor. Levels blend over16 cells; the outer edge falls back to the direct column.
The last100m below the cloud base also blends to the actual direct query, and
inside-cloud/uncovered points use it directly. Layer, Sun and wind phase belong
to one immutable field state; there is no history, global cache manager or
automatic reuse across changed states. Camera translation and general Sun/wind
coverage still need broader verification before selecting a production profile.

Maps store linear T and apply it only to air DirectEmission before the unchanged
fit. Diffuse/Psi and eye extinction keep their original meaning. This remains a
composition experiment beside the mixed consumer, without a generator-to-render
dependency. Full physical air partition precedes surface queries.

The private compute sampler is set0/binding4. The optional
`air-prefix-gpu ... [--source-map map.bin] [--gpu-timing]` enables exactly that
seventh binding; existing invocations keep six bindings. Medium80/Ray32/Sample32
remain unchanged. Only this optional texture accepts scalar R32F; old LUT formats
and the original arguments retain their interpretation. The experiment's atlas
holds five virtual cameras; a single camera's three levels occupy3,158,028 bytes.
Atlas construction is currently CPU NumPy plus upload. GPU generation, resource
update/lifetime and complete renderer costs remain unproven.

Independent controls retain the homogeneous clear medium and prescribed solar/
Psi textures from #350. Thirty rays add direct-Sun views and1m/10m/25m/50m/100m
queries to the existing ground/cloud/orbit cohort. Repeated/sparse queries,
null clouds, actual native L/T and diagnostic-on/off numeric identity are checked.
The two diagnostic words deliberately use a private packed work layout: source
positions in bits0..9 and solar-density evaluations above bit10; union cells in
bits0..8, cloud density in9..14, cloud sources in15..20 and actual solar texture
samples in21..31. This is not the old experiment's counter interpretation.
Timing shaders disable these diagnostic counters. Both diagnostic and plain
shaders are measured; submit/fence wall time never substitutes for Dispatch time.

The full1920×1080 numerical image uses a55° vertical perspective at1772m, looks
west, and queries the finite transfer at10km. It is a full-size compute image,
not an engine/Place frame. All pixels compare direct versus reused source work;
the eight largest differences plus twelve fixed image positions receive separate
Gauss16/32 RTE refinement. The unchanged1% RGB limit/floor1e-5 is applied to those
independent witnesses. The test reports a rejected candidate explicitly rather
than weakening its requirement. GPU timestamps include their measured contention
case; NVML snapshots before/after the new run record observed load and memory.
No isolated device or A18 result is inferred.

Python3/NumPy/Pillow, the project's C++ toolchain and glslang are required:

```sh
python3 -m unittest discover -s test/experiments/weather -p test_cloud_sun_reuse.py
python3 test/experiments/weather/cloud_sun_reuse_native.py --output /tmp/outshine-source-reuse --api-root /path/to/clean-archive-producer --archive /path/to/liboutshine.a --archive-receipt /path/to/receipt.json --benchmark
```

The receipt must name the archive's `commit` and `archive_sha256`. The runner
verifies its digest and compiles the current consumer only against that clean
producer's headers and central compiler/link declarations. Current GLSL kernels
are compiled separately. It does not relink old archives against new headers.
The study emits input/maps, exact commands, source/artifact hashes and terminal
results to the explicit system-temp output. CPU build wall time is not GPU time.
CPU preparation/reference workers and compilers run with `nice -n 10`;
the controlling CLI and measured native process retain normal priority.

[Lokovic/Veach, SIGGRAPH2000](https://graphics.stanford.edu/papers/deepshadows/),
PDF pages2/3 and figures3/4, motivates storing/transferring solar visibility.
Both pages were inspected visually. This local full-column study is not a full
prefiltered deep-shadow implementation. Wronski's SIGGRAPH2014 separation of
lighting and integration (visually inspected pages25/41, linked in
`../render/air_source_grids.md`) motivates the next comparison: share complete
camera transfers and their integration, rather than only the solar source query.
Its fog range is not a bound for the full Earth atmosphere.
