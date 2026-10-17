# Long-phase cloud advection

The contract, findings and eventual native acceptance belong to
[#447](https://github.com/Weichwolf/outshine/issues/447).
This experiment preserves the existing field/profile reference and the unchanged
native density kernel. It measures the precision of advected coordinates separately
from physical spherical height, temporal history and lighting.

First build the existing density probe against a complete regular archive:

```sh
python3 test/experiments/weather/cloud_density_native.py \
  --archive-root "$PWD" --output /tmp/outshine-weather-density --untimed
python3 test/experiments/weather/cloud_wind_precision.py \
  --native-root /tmp/outshine-weather-density \
  --output /tmp/outshine-weather-wind-precision
python3 -m unittest discover -s test/experiments/weather \
  -p test_cloud_wind_coordinates.py
```

Use the project's compiler/SDK and Vulkan-validation environment. The supplied
native binary and shader must match the original density receipt. Generated point
inputs, native commands/outputs and the comparison PDF remain in system temp.

The direct control packs complete metre drift into FP32. The modulo-hour control
limits the number while changing the original field; it is an intentionally failing
alternative. The split candidate keeps independent integer origins and local
fractions for the broad field and its3.1-frequency erosion. Cumulus-body hashing
uses the broad global cell while geometry uses its small local fraction.

The candidate uses simulated FP32 coordinate/density arithmetic and the same FP64
spherical height for all candidates. This isolates precision; it does not prove a
native shader, payload, runtime default, image-quality or device/frame budget.
Python elapsed times and tracemalloc peaks do not substitute GPU/native costs.
Four covers and both existing profiles use the original independent FP64 field.
Tests additionally cover negative phases/winds,30/60Hz increments and camera anchors.

The source study begins with Perlin, *Improving Noise*, SIGGRAPH2002:
[author PDF](https://mrl.cs.nyu.edu/~perlin/paper445.pdf), local
`doc/references/clouds/siggraph/2002-improving-noise.pdf`.
Page1, its lattice/interpolation equation and Figures1a/b were visually inspected.
Integer cells and relative coordinates support the numerical decomposition; Outshine
keeps its own existing value-noise hash, cubic interpolation and cloud profiles.
No gradient-noise implementation or third-party source code is copied.

## Actual native candidate

The next experiment retains the original48B control and tests the split field on
the native GPU, including the original FP32 spherical height calculation:

```sh
python3 test/experiments/weather/cloud_wind_native.py \
  --archive-root "$PWD" --control-root /tmp/outshine-weather-density \
  --output /tmp/outshine-weather-wind-native --measure-cost
python3 test/experiments/weather/cloud_wind_native_report.py \
  /tmp/outshine-weather-wind-native
```

The complete regular archive and its matching headers are reused through the same
compiler/link helper as the original density probe. The control's binary, shader
and all eight original inputs must match their receipt; the extended C++ host
must reproduce every original density byte. Two independent recordings per capture
check output identity and staging/resource lifetime. Vulkan validation is enabled;
diagnostics fail the capture. The actual reflected uniform range is checked.

This private prototype has a112B std140 payload at2/0, sixteen-byte members:

| Offset (bytes) | GLSL type/member | Meaning |
|---|---|---|
|0|vec4 Shell|planet radius, physical base, thickness, extinction|
|16|vec4 Field|cover, existing profile, unused, unused|
|32|vec4 CoordinateOrigin|coordinate anchor xyz in metres, unused|
|48|uvec4 BroadBase|integer lattice xyz modulo2^32, unused|
|64|vec4 BroadFraction|fractional lattice xyz, unused|
|80|uvec4 DetailBase|independent3.1-frequency integer lattice xyz, unused|
|96|vec4 DetailFraction|independent fractional lattice xyz, unused|

CPU packing computes `(anchor - velocity * phase) /1400 * frequency` in FP64,
splits floor and fraction, and preserves integer bits in little-endian words.
The shader subtracts the coordinate anchor only for field lookup. Physical
spherical height still receives the original physical point. Changing the
coordinate anchor therefore preserves field identity; it does not translate the
planet or change an Engine world frame. The existing hash uses its original
modulo2^32 semantics. No phase reset or periodic replacement field is introduced.

Native cohorts cover both profiles, four covers, six phases,48 signed/diagonal
wind steps at30/60Hz,24 steps crossing broad/detail lattice boundaries, and twelve
coordinate anchors. A separate horizontal native
density section captures individual-frame sticking/progress and30 seconds of wind.
It is a diagnostic section rather than a rendered sky or Place. The PDF names this
scope and includes amplified differences. The independent FP64 field is unchanged.

Optional cost runs use the original1048576 points and interleave direct/split
order across three processes per case. Every process retains both sets of sixteen
actual GPU timestamp pairs; warm first dispatches and all tails are included. PDF
whiskers span the full range. Background load, device and terminal results belong
in the delivery evidence. These isolated dispatches do not prove a frame budget;
the inaccurate long-phase direct control is not a quality-equivalent alternative.

Logical per-capture work remains32 dispatches,64 threads per workgroup and the
same original density queries, eight-corner hashes and nine-cell Cumulus search.
Splitting adds integer cell-origin additions; the anchor adds one vec3 subtraction.
At1048576 points, point storage/upload/readback/output buffers total41943040B per
recording before driver overhead, input copy16777216B and output copy4194304B.
The private uniform grows48→112B; its sixteen identical dispatches reuse one
snapshot in the existing Vulkan uniform store. Source-level buffer/copy counts are
not measured allocator totals or a device peak. Full CPU allocation traffic and
GPU residency remain separate acceptance work.

No production API, CloudLayer packing, default profile, common pass/binding or
runtime renderer changes here. Agree the final representation with Light and
Performance before integration; production packing, moving sky/Place/history and
device acceptance remain in #447 and its owners.
