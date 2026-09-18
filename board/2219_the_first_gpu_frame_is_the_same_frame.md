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

## Architecture decision

`SubjectResidency` publishes a texture as a complete immutable `SampledImage`:
image, sampler, all declared mip levels, transfer completion and a monotonically
assigned content revision form one owner. A material slot may bind that owner only
after the upload submission which writes its final level has completed successfully.
The renderer consumes a published content revision for the whole frame; a rejected
or unfinished upload retains the former material world. Texture staging allocations
live through their submission fence and retire only after it signals.

The contract must use the normal asynchronous publication path. It forbids an idle
wait, a hidden render frame, a test-specific sampler, a vendor shader source or a
changed filter semantic. GLSL stays source of truth. SDL_GPU/Metal details stay in
the backend adapter behind this resource contract.

## Implementation order

1. **P0-A:** Add a focused `SampledImage` state test with a controllable transfer
   fence: incomplete texture A cannot bind, failure preserves published A, completion
   publishes B exactly once, and staging survives until the fence.
2. **P0-B:** Route all subject material maps through that owner. Batch levels of one
   image into one declared upload product; do not expose a partially populated mip
   chain. Keep colour sRGB and data/normal linear.
3. **P0-C:** Make `SceneRenderer` admit only completed content revisions at the
   existing world-candidate publication boundary. A failed command submission must
   retain old pixels and allow retry.
4. **P0-D:** Run the chess and air repeat contracts on the normal client path. If
   filtered sampling still differs, capture the SDL backend, OS, driver and generated
   shader product in one compact diagnostic and reproduce on a second available
   SDL_GPU backend before changing backend code.

## Acceptance

- [ ] First, second and redeclared static frames are byte-identical for mipmapped
      chess and atmospheric scenes; their existing negative controls remain red.
- [ ] A failed or cancelled texture upload exposes neither a partial mip chain nor
      an invalid descriptor and preserves the previous world and pixels.
- [ ] Upload staging and GPU texture owners are released only after their final use.
- [ ] No warm-up, blocking idle wait, vendor shader path or loosened image threshold
      reaches production.
