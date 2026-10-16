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
