# Material source and RGB transmittance

[#273](https://github.com/Weichwolf/outshine/issues/273) supplies the material
value required by the shared surface/air contract in
[#266](https://github.com/Weichwolf/outshine/issues/266).
`SurfaceTransfer.Source` is local BRDF/emission radiance without transmitted
background. `Transmittance` is a separate RGB factor including the material's
existing tint, absorption and transmission/Fresnel/metallic split once.
The register-only GLSL value is not a uniform or an external buffer layout.

The lit and flat KIND3 producers now construct this value before their existing
background mix. They share the absorption implementation. The extraction does
not add lighting, medium integration, a binding, a pass or an SDL operation.
`coveredSurfaceTransfer` converts geometric coverage a to the affine event
`Source=a*S`, `Transmittance=(1-a)+a*T`. Ordinary alpha is therefore distinct from
RGB absorption. This conversion is available for the agreed new compositor;
the current KIND2 straight-alpha output/blend state remains at its existing
boundary until the common rendergraph migration changes both consumers.

Valid inputs retain the existing material domain: finite nonnegative thickness,
positive attenuation distance or infinity, finite unit-range attenuation colour
and coverage. Source uses linear scene radiance. The original finite-path
attenuation formula and its lower colour bound are preserved. The lit producer
retains its current Fresnel/metallic split; the unlit producer retains its
existing unlit response. This does not redefine unlit materials as a lit BRDF.
Grouping the RGB factor before the background multiplication may change floating
point rounding; bit-identical old raster colour is not asserted.

The native experiment uses #268's existing buffer consumer and shared palette
input; no new backend or C++ resource wrapper. It appends controlled test values
to a private copy of the valid palette, preserving its prefix. Both records keep
the existing32-byte size and set/slot/access contract. NumPy is required.

```sh
python3 test/experiments/render/surface_transfer_native.py --binary /path/to/native_ground_litter --palette /path/to/ground-litter.palette --output /tmp/outshine-material-transfer
```

Build the consumer against the regular platform `build/liboutshine.a` using the
engine's published C++23/no-exceptions/warning flags and native link dependencies.
`GLSLANG` can name glslangValidator. Shader compilation uses nice10 and capture
uses normal priority. Use the pinned #268 palette with unmapped row13; it is
input, not an expected-output oracle. Outputs must be in system temp. Failed
or incomplete runs cannot retain an earlier comparison success marker.

There are4096 fixed material cases and three real shader outputs per case:
raw source/T, covered source/T, and background composition plus absorption.
The controls include zero/one/fractional coverage, opaque/neutral/coloured T,
zero/finite thickness and infinite distance. Eight neutral events must preserve
background RGB exactly. The independent area reference evaluates covered and
uncovered regions separately. It checks transfer algebra tightly against the
captured absorption, independently of the transcendental approximation.

An initially uniform3e-6 test also constrained the old exp/log absorption and
failed near white with long optical paths. That is not a defect in coverage:
GLSL permits absolute log error near one. The test now separates that existing
numerical responsibility and propagates the documented log/division/multiply/
exp precision through its positive normal-distance cases. The conservative
budget uses float32 epsilon, the2^-21 near-one log bound and path length; it is
not a visual error allowance. The much tighter3e-6 area-mixture check remains.
Double-precision power-reference errors and the fraction of the precision budget
are reported explicitly, without claiming global radiance/image accuracy.
Source: [GLSL precision requirements](https://docs.vulkan.org/glsl/latest/chapters/variables.html).

This material delivery alone does not fix the neutral-glass double-air error.
The existing background collapse remains visible in the current rendergraph.
Light owns front-air composition; Master/Performance own ordering, native
bindings and depth/velocity lifetimes. Their shared choice under #266 must
replace that collapse and the old post-fog boundary together. Native raster,
identical lit Places, A18 and integrated device costs remain open.
The independent affine layer reference belongs to Light c2b3a3280; this probe
verifies the material inputs, not a second atmosphere/compositor.
