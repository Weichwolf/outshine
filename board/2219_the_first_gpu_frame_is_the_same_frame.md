Type: bug
State: active
Parent: 2188
Depends: 2190
Priority: P0
Area: render, SDL3, test
Tags: determinism, backend, measured

# A static GPU world renders identically from its first submission

## Proven defect

`MipmappedChessRepeatsLinearPixels` changes 252 linear RGB channels between the
first and every later static frame (maximum 0.00195312); depth and alpha are exact.
The same class of defect occurs in the atmospheric repeat case. A discarded warm-up
frame makes later frames equal and is prohibited.

The fault is before atmosphere, resolve and presentation. The cull result, depth
pyramid, temporal state, texture copy grouping, idle waits, explicit LOD and actual
texel reads are not causal. Replacing `texture` with `texelFetch` makes all frames
equal while retaining the same bound mip resource and descriptor. The remaining
producer is the first filtered hardware sample or its resource readiness contract.

Today `SubjectResidency::UploadMip` submits every mip level through the raw SDL submit
function, retains neither a fence nor a texture-upload owner, and returns a bindable
`BoundImage` immediately. That violates the required publication boundary regardless
of whether the backend happens to defer transfer-buffer destruction safely.

## Architecture decision

The idle-wait probe already disproves incomplete transfer execution as the direct
cause. A `SampledImage` owner with one complete mip upload and fence-retained staging
is still required for resource lifetime and belongs under 2190/2191, but it must not
be presented as a repair for this image defect.

2219 first reduces the fault to a minimal SDL_GPU filtered-sampling reproducer:
one immutable texture, one immutable sampler, one static fullscreen primitive and
linear readback. It uses the same GLSL source and exact sampler state as the engine,
then compares first, second and freshly recreated device frames. The reproducer must
also test base-only, nearest-mip and linear-mip textures. It contains no scene graph,
culling, temporal targets, uploads after setup or engine material code.

If the minimal reproducer fails, the defect is an SDL backend/compiler/driver issue.
Record SDL commit, backend, OS, GPU, sampler descriptor and generated shader product,
then test a locally pinned newer SDL or second available backend. Do not carry a
warm-up, idle wait, vendor shader source or altered filtering into Outshine. If the
minimal reproducer passes, its differing input is a causal boundary and the engine
path is reduced from there.

## Implementation order

1. **P0-A:** Build and run the minimal reproducer for every declared mip filter.
2. **P0-B:** Compare its first-frame results with the engine contracts and identify
   the first differing input if the reproducer passes.
3. **P0-C:** If it fails, pin and test a second local SDL/backend product; record the
   exact upstream reproducer rather than guessing at engine fixes.
4. **P1, 2190/2191:** Independently add the `SampledImage` lifetime owner and the
   asynchronous `PendingWorldPublication` transition with failure, shutdown and retry
   proofs. This work must preserve pixels but does not close 2219 by itself.

## Acceptance

- [ ] First, second and redeclared static frames are byte-identical for mipmapped
      chess and atmospheric scenes; their existing negative controls remain red.
- [ ] Minimal SDL_GPU reproducer identifies whether the remaining defect is upstream
      or isolates the first differing engine input.
- [ ] A failed or cancelled texture upload exposes neither a partial mip chain nor
      an invalid descriptor and preserves the previous world and pixels (2190/2191).
- [ ] No warm-up, blocking idle wait, vendor shader path or loosened image threshold
      reaches production.
