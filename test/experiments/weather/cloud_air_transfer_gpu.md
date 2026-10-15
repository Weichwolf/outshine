# Resident GPU air field

Refs #353/#320, goal #190; continues `cloud_air_transfer_reuse.md`. The existing
homogeneous-air factorization is now generated and consumed on the GPU in one
submission. There is no field readback, CPU normalization, upload or CPU wait
between its dependent dispatches. This remains an experiment at fixed10km depth,
not the missing general-depth/cloud-overlap renderer contract.

The actual source kernel still evaluates Weather's density and solar quadrature
before the unchanged physical air-source fit. An optional experiment-only
`SOURCE_FIELD_STATE` uniform carries coverage and advected east/south displacement
in16bytes at2/1; Medium80 remains2/0. Host size/alignment/offsets and reflected
layouts are checked. The uniform does not introduce a second weather recipe.
All old callers retain their exact default values and six/seven-binding layouts.

`SOURCE_VISIBILITY_OUTPUT` optionally normalizes the resulting air transfer into
the existing spectral weighted visibility and stores RGBA32F at1/0. RGB is the
field, A is its exact logical source-position count. The next dispatch samples
the same resource at0/4. The alternative staged path writes the original32byte
coarse samples, normalizes them in another compute dispatch, then reconstructs.
Both alternatives are measured with the same dynamic state, rays and targets.
No assumption that fewer dispatches are faster replaces this comparison.

The field, pipelines and buffers are resident through the whole diagnostic
sequence: covered/37s advection, clear, covered/zero advection, return to the
initial state. Per-state uniforms are copied into commands; the same field is
overwritten and sampled. Returning to the initial state must restore identical
field/image bytes. Reusing the covered image for the clear state must fail its
independent closed-form expectation. Final image/field readbacks exist solely
after the last dependent dispatch for offline diagnosis. Initial upload, bounded
offline waits and artifact IO are outside the measured GPU span.

The live wind control exposes a bilinear error missed by the earlier one-state
cohort. Reconstruction therefore additionally compares a separable cubic Hermite
filter on the same field. Interior central secants yield Catmull-Rom on regular
nodes; actual node distances handle the shorter final cells, and clamped node
indices give one-sided edge derivatives. It performs16 logical texel fetches per
output pixel. No value clamp conceals ringing or errors; numeric/positive and
independent RTE controls decide whether a candidate is acceptable. The original
bilinear shader remains the default and a reproducible rejected control.

The control is1920×1080,55° vertical FOV, eye1772m, west-facing,10km query and
10000km requested physical ray. It keeps homogeneous coefficients and prescribed
solar/Psi inputs to isolate source sharing. Actual frame state and arbitrary
depth are not inferred from it. Every pixel compares against the direct native
source consumer and an independent optical-depth closed form; selected worst and
fixed points receive Gauss16/32 RTE refinement with unchanged1%-RGB/1e-5-floor and
2e-6-T limits. New state and interpolation controls do not loosen these limits.

Each submission retains raw GPU timestamp endpoints, units, valid bits and
ordinals. The full dependent GPU span includes transition gaps, rather than
adding asynchronous or CPU wait times. Median samples, observed contention and
the absence of full-frame p99 acceptance remain explicit. Logical buffer bytes
by requested placement, texture payload, initial/capture copies and uniform
payload are reported separately from sampled global NVML memory and process
VmHWM. None is relabelled as a physical isolated app/device budget. Sampling cost
and missing platform measurements are recorded.

Reproduce with Python3/NumPy, the project's C++ toolchain and glslang:

```sh
python3 test/experiments/weather/cloud_air_transfer_gpu_native.py --output /tmp/outshine-air-gpu --api-root /path/to/clean-archive-producer --archive /path/to/liboutshine.a --archive-receipt /path/to/receipt.json
python3 test/experiments/weather/plot_cloud_air_transfer_gpu.py --captures /tmp/outshine-air-gpu --output /tmp/outshine-air-gpu/comparison.pdf
```

The archive SHA and clean producer/header commit must match its receipt. CPU
preparation/reference workers and compilers run nice10; measured children retain
normal priority. Current commands, terminal statuses and source/input/output/
shader hashes are preserved. CPU normalization in the older runner remains a
comparison, not a dependency of the resident path. Plots require Matplotlib.

The new reconstruction follows the cubic/Hermite basis discussed in
[Mitchell/Netravali, SIGGRAPH1988](https://www.cs.utexas.edu/~fussell/courses/cs384g-fall2013/lectures/mitchell/Mitchell.pdf),
pages3/4 (equations8/12, figures13/14), inspected optically. Its reconstruction
principles do not imply a bound for this measured atmospheric field. Wronski's
SIGGRAPH2014 lighting/integration separation remains linked in #353.

Next requirements remain GPU-resident depth-dependent sharing and correct
composition with actual cloud cells from #344/#266, nonhomogeneous atmosphere,
Sun/night/camera motion, state publication/history, Places, full-frame p99,
physical peak memory and A18 proof. Production renderer/bindings are unchanged.
