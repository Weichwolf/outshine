# Native metric storey bands

[#269](https://github.com/Weichwolf/outshine/issues/269) adds the opaque storey-band
part of the Python `FacadeFinish` reference from #245/#255 to the native facade
composition. Existing openings, frames, glazing fields, source wall colours and
negative roof/plinth codes retain their contracts. This is not the complete
finite-domain Python facade model or a new geometry/room implementation.

Bands are 0.16 metres wide around existing storey boundaries. Their width is
bounded by the opaque inter-storey gap. The existing surface-gradient operation
recovers storeys per metre from actual position/UV derivatives; the nominal
style height is not substituted for a building's adjusted storey height.
The fallback for a degenerate derivative frame adds no band. The band uses
0.65 times the current linear wall colour and shares its roughness.

Window, frame, wall and band contributions are composed before the existing
single lighting evaluation. Multiplying the filtered wall colour by a filtered
band and then applying a filtered window would introduce a product-of-averages
error. Instead, the band replaces its own share of the remaining opaque wall.
The C++ control explicitly rejects that sequential composition in a minified
case. The original zero-band composition is preserved exactly.

The filter integrates its projected axis-aligned UV box, using the existing
periodic-band primitive. It does not exactly integrate an arbitrarily oriented
pixel parallelogram; the footprint limitations identified in #245 remain.
No new palette field, binding, texture, pass or asset version is introduced.

The native comparison reuses the actual buffer consumer from #268, with the
same checked 32-byte input/output records and four binding slots. It does not
add a renderer or backend. Build `NativeGroundLitterProbe.cpp` from #268 against
the platform's regular `build/liboutshine.a` and its native link dependencies,
using the published C++23/no-exceptions/warning declarations. Then run:

```sh
python3 test/experiments/render/facade_bands_native.py --binary /path/to/native_ground_litter --palette /path/to/ground-litter.palette --output /tmp/outshine-material-facade-bands
```

Use #268's pinned `test/outshine/data/materials/ground-litter.palette`; the
consumer's explicit unmapped row is 13. The palette is component input, not an
expected-output oracle. Requires NumPy, Pillow and `glslangValidator` (or the
`GLSLANG` environment path), plus the platform backend dependencies. Shader
compilation uses nice10; GPU capture uses normal priority. Outputs must be in
system temp. The script removes an old success marker before each attempt and
writes `comparison.json` only after real GPU capture and numerical checks.

The independent reference clips explicit interval copies instead of evaluating
the shader's periodic primitive. It covers five facade families, five physical
storey heights, negative/high phases, five footprint widths, axial/sheared
Jacobians and previous/new controls. Six additional 320×160 albedo panels show
two near facades and a minified case, each before/after. The output image is
unlit material colour, not a Place, BRDF or native frame-quality proof.
The compute probe also checks the actual surface-gradient recovery against
known physical storey heights; the complete fragment shader must be translated
separately with all Make-declared lit variants.

Source basis: EGSR2010 *Grammar-based Encoding of Facades*, PDF page3/Figure2,
optically inspected; the material-composition course basis remains in #196.
Shader translation/layout checks, numerical correctness, lit Places and device
costs are separate. Identical lit Places, native frame/memory costs and A18
execution remain required before final integration acceptance.
