# Native facade phase and classification

Issue: [#317](https://github.com/Weichwolf/outshine/issues/317), parent #285.
The existing native encoding is `256*(style+11*(field+3*variant))+bay`.
Interpolating that large value loses fine window phase. Perspective interpolation
can also move a nominally constant integer group across its decoding boundary.

The production vertex now emits the small bay at location 11 and a flat float
classification at location 12. The fragment decodes the latter as an integer
inside the procedural branch. A flat float avoids converting arbitrary texture
UVs to integers in the vertex. Original UV0/UV1 remain available to textures.
Native producers give all vertices of a primitive the same classification;
generated bay counts are bounded to 255. CPU-side quantization of fractional
encoded input UVs is a separate limitation; this change cannot restore it.

## Actual raster comparison

Baseline: b6c2ebac4. Checked source: 7370a743b, followed by a master merge with
unchanged shader dependencies. Both actual production vertex/fragment pairs
render the same wall into three 160×96 RGBA32 attachments, without lighting.
The common `NativeFacadeProbe.cpp` consumer uses its real native tangent frame.
Its frozen headers match its regular production archive, SHA256
`bb3af3c877736f995688cd40755fb43f5ea42640afe3c16c84768330b7022206`.
No new headers are linked against an older backend archive.

Integer bay endpoints isolate interpolation from initial CPU quantization.
Material base colour cancels each native variant tint, giving identical final
source colour. Independent projected-quad coverage and GPU coverage checks
reject invalid fixtures. The comparison tolerance remains 2e-4.

- 54 orthographic groups: three styles × three fields × six views, each with
  eight variants and old/new shaders. Views include near/far, bay 255, camera
  shift and tile anchor. Maximum variant-dependent error falls from 0.455 to
  5.9604645e-8. Six excluded plain/negative-code controls are exactly unchanged;
  original UV0 is byte-identical, including values outside [0,1]. UV1 retains
  its zero default. Total: 878 captures.
- Nine perspective groups vary clip W across each primitive and exercise the
  same styles, fields and variants. Maximum new error is 5.9604645e-8. A direct
  diagnostic of group 260 finds 96 misclassified old pixels and zero new ones.
  Merely separating the bay had retained this classification failure. Total:
  146 captures, including both direct diagnostics.

Each capture passes all 35 native checks with zero failures/skips/unprepared/
partial results: 35,840 checks across 1,024 captures. Vulkan synchronization
validation is enabled on RTX 3060. Actual albedo and roughness comparison PDFs
are optically inspected; outputs stay in system temp.

## Reproduction and cost boundary

Build the ordinary native facade consumer through the existing
`facade_coating_raster.py` workflow. Extract the baseline shader directory with
`git archive b6c2ebac4 src/render/shaders`, then point both scripts at that
directory. Provide GLSLANG and the Vulkan driver/validation environment.

```sh
python3 test/experiments/render/facade_phase_raster.py \
  --binary /tmp/native_facade --baseline /tmp/baseline/src/render/shaders \
  --output /tmp/outshine-material-phase
python3 test/experiments/render/facade_phase_perspective.py \
  --binary /tmp/native_facade --baseline /tmp/baseline/src/render/shaders \
  --output /tmp/outshine-material-phase-perspective
```

The fixture is 896 bytes; each capture reads back 737,280 bytes. The product
adds two 32-bit scalar stage values, one smooth and one flat, with small vertex
arithmetic. This logical eight-byte interface addition is not a measurement of
hardware traffic, physical allocation or frame cost. Vertex inputs, placement,
uniforms, resources, cache and public API are unchanged. No texture/sample/pass
is added. Existing window/coating filters remain unchanged.

All 20 vertex and 252 fragment old/new SPIR-V/MSL 2.3 pairs check the matched
new stage locations and flat fragment qualification while preserving every
other stage IO, resource layout and MSL resource slot. Translation is not Metal
execution. C++ consumer code is unchanged; its existing focused tidy evidence
applies. Master owns integrated lit Places, Metal/A18 and actual frame/memory
counterchecks. Concurrent host work precludes an isolated timing claim here.
