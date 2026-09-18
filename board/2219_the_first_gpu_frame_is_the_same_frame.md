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
min/mag filtering and `MipFilter::None` is exact on first and second frame. The
fault therefore requires mip selection or an additional input of the imported chess
path. This result removes base upload and ordinary bilinear sampling from the next
search space; it does not prove raw SDL_GPU behaviour.

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
locally pinned SDL/backend product. Do not add an engine workaround. A green raw
test narrows the next reducer to the first engine input absent from this fixture.

## Order

1. Add and run the raw matrix. This WI has no dependency on broad GPU ownership work.
2. If green, add one engine factor at a time: upload batching, generated mip values,
   material descriptor table, imported mesh derivative footprint, then atmosphere.
3. In parallel but separately, WI 2235 makes complete sampled-image ownership and
   asynchronous candidate publication correct. It must preserve pixels but is not
   claimed as this defect's repair.

## Acceptance

- [ ] The raw matrix reports exact first/second/fresh-device equality or an upstream
      reproducer with its complete local product record.
- [ ] Existing chess and atmospheric repeat checks become exact without a warm-up,
      blocking idle wait, vendor shader path, changed filter or relaxed threshold.
- [ ] Negative controls still expose a changed mip, descriptor or texel path.
- [ ] Relevant device/public suites and `make lint` pass.
