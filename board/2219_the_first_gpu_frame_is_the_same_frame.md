Type: bug
State: active
Parent: 2188
Depends:
Priority: P0
Area: render, SDL3, test
Tags: determinism, backend, measured

# A static GPU image has no first-frame exception

## Proven defect

`MipmappedChessRepeatsLinearPixels` changes 252 linear RGB channels between the
first and later static frames (maximum 0.00195312). Depth and alpha are exact.
A discarded warm-up frame is prohibited. Idle waits, culling, depth pyramid,
temporal state, texture-copy grouping, explicit LOD and texel reads are not causal.
Replacing `texture` with `texelFetch` makes the frames equal with the same descriptor.

`LinearTextureSamplingRepeats` is green: a public-API 2×2 unlit quad with linear
min/mag filtering and `MipFilter::None` is exact on first and second frame.

`FilteredMipSampling/FirstFrameMatchesRepeatedSampling` is green on the local
SDL_GPU device: base-only, nearest-mip and linear-mip variants agree byte-for-byte
between their first draw, second draw and a fresh-device first draw. It uses the
engine's `HalveInPlace` values, then tests both one packed chain submit and the
actual `UploadMip` shape: one short-lived staging buffer and submit per mip level.
It also binds eight actively sampled material descriptors. It owns only texture,
immutable samplers, fullscreen GLSL `texture()` pipeline, target and direct readback.
Raw filtered sampling, generated mip values, per-level submission, transient staging
descriptor-table cardinality and standard interpolated UV derivatives are therefore
not the defect.

`ImportedChessNativeGeometryRepeatsLinearPixels` clones the adapter's native snapshot,
applies the same unlit surface condition, and sends it through `Engine::setGeometry`.
It changes 504 linear channels (maximum 0.220703) after the first frame. The failure
therefore survives without `Scenario::Asset` loading and is inside native Geometry to
Subject construction or its render pipeline.

`PerspectiveNativeMipImagesRepeatLinearPixels` is exact with the Chess camera and
1280×720 target using Linear magnify/minify, Linear mip selection and Repeat
addressing. `ImportedChessImageOnNativeQuadRepeatsLinearPixels` replaces only that
checker with the selected imported base-colour image and remains exact.
Clearing all tangent vectors, UV1 data or vertex colours from the native Chess clone
separately leaves the exact 504-channel, 0.220703 defect. A copied single native part, including its original images/material and attributes, is also
red but only changes 3 channels (maximum 0.000244141). Multi-part packing amplifies
rather than solely causes the defect. `ImportedChessSinglePartConstantUvRepeatsLinearPixels`
is exact after replacing only UV0 by a constant. The defect requires varying UV0 derivatives
inside the real Subject pipeline; material data and geometry positions remain unchanged. Atmosphere is already disproved by the pre-atmosphere evidence.

## Decision

Build the diagnostic as a raw SDL_GPU device test under
`test/outshine/src/render/device/FilteredMipSampling/`. It owns its windowless device,
RGBA texture, one immutable sampler, fullscreen pipeline, target and readback. It
uses the shipped GLSL-derived SPIR-V product and the exact engine sampler descriptor,
but neither Engine, SceneRenderer, importer nor material code. The fixture uploads a
complete mip chain before recording its only static draw; no warm-up, idle wait or
later upload is allowed.

Run three inputs at the same projected footprint: one level with mip disabled, a
complete chain with nearest-mip selection, and a complete chain with linear-mip
selection. For each, compare first draw, second draw and a fresh-device first draw
byte-for-byte. Print only changed-channel count, first offset and maximum delta.

A red raw test is an SDL/backend/compiler/driver defect. Record local SDL commit,
backend, OS, GPU, shader product and sampler descriptor; reproduce on one other
locally pinned SDL/backend product. Do not add an engine workaround. The current
green test requires the next reducer to add one missing engine input at a time.

The raw matrix is green. The engine repair therefore belongs to `SubjectDraw`, not
to SDL submission or image upload. `SubjectProxy::Lit` correctly excludes
`Material::Unlit`; the red Chess clone consequently selects a flat `Position+Uv0`
layout despite carrying source normals. The defect is in that real flat Subject
pipeline, not an accidental choice of a lit fragment program. Layout, selected
SPIR-V products and final texture-coordinate inputs are test-visible diagnostics.

The candidate repair is to apply every material texture transform before
rasterization and interpolate the final sampling coordinate. The fragment program
then samples that varying directly. Adopt it only if the reducer proves the current
fragment-side affine transform causal. It must preserve Khronos texture-transform
semantics, UV-set selection and linear colour-space. Do not replace filtering with
texel fetch, force a LOD, warm a frame, or change a tolerance.

## Order

1. [x] Add and run the raw matrix. This WI has no dependency on broad GPU ownership work.
2. [x] Add the engine's generated mip texels and both one-submit and one-submit-per-level
   staging forms to the raw fixture.
3. [x] Bind and actively sample the complete eight-slot material descriptor table.
4. [x] Prove a raw static vertexbuffer with interpolated UV derivatives exact.
5. [x] Send a cloned imported native Geometry through `setGeometry`; it is red.
6. [x] Prove the Chess perspective on a single native mipmapped quad exact.
7. [x] Reduce to one copied native part: residual is 3 channels; multi-part packing amplifies it.
8. [x] Replace only the single part's UV0 with a constant: it is exact.
9. [x] Prove the unlit clone selects flat `Position+Uv0`; the linear/repeat generated
   checker control is exact, so sampler policy and normal selection are excluded.
10. [x] Replace the generated checker with the selected imported base-colour image:
    it remains exact, excluding image content and its mip chain.
11. Feed four declared UV pairs from the imported part into the same quad, then add
    its indexed triangles without changing camera, sampler or image. Compare first
    and second frame at every boundary; distinguish coordinate values from topology.
12. Only a failing transform boundary permits the vertex-side final-UV candidate;
    otherwise continue reduction without redesigning material program selection.
13. WI 2235 separately repairs incomplete image ownership and publication. It must
    preserve pixels and is not claimed as this defect's repair.

## Acceptance

- [ ] The raw matrix reports exact first/second/fresh-device equality or an upstream
      reproducer with its complete local product record.
- [ ] Existing chess and atmospheric repeat checks become exact without a warm-up,
      blocking idle wait, vendor shader path, changed filter or relaxed threshold.
- [ ] An explicitly unlit native mesh selects the flat `Position+Uv0` pipeline even
      when its imported source has normal/tangent attributes; UV-set and texture-
      transform semantics remain identical to the Khronos adapter contract.
- [ ] Negative controls still expose a changed mip, descriptor or texel path.
- [ ] Relevant device/public suites and `make lint` pass.
