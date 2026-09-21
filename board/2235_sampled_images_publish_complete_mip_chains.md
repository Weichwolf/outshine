Type: refactor
State: active
Architecture: ready
Parent: 2223
Depends:
Priority: P1
Area: render, engine, test
Tags: gpu, ownership, state

# Sampled image mip chains use one transactional upload

## Evidence and correction

`SubjectResidency::Upload` now packs every encoded mip level into one checked transfer
allocation, records all copies in one pass and submits once. The local `BoundImage` returns
only after that submit and sampler creation succeed. `FilteredMipSampling` uses an imported
multi-level material and proves one submit, one staging allocation and positive transfer bytes.
`SubjectDraw::BindSurface` likewise builds a local slot. `MipSubmissionFailureRetainsRetry`
injects the final submit failure, receives no image and immediately retries to a complete
texture/sampler pair.

Reference: ../SDL at fa2c02b, include/SDL3/SDL_gpu.h, SDL_UploadToGPUTexture and
SDL_ReleaseGPUTransferBuffer. Subsequent commands see completed uploads; release is
GPU-deferred by SDL. An application fence is not required solely to sample this image
or release its staging owner. This replaces the former mandatory PendingSampledImage
and per-image fence design. Existing synchronous callers and Result contracts stay.
Static draw-input reuse and bounded derivative-filter stability are already proven independently.

## Decision

Keep the existing move-only texture/sampler owner and WorldContent commit boundary.
Prepare the complete encoded chain privately; pack levels in one checked allocation,
record one copy pass and submit once per image. Publish only after submit and sampler
creation succeed. A following draw uses SDL ordering on the same device/owner thread.
Release staging through its existing RAII wrapper after submission; never map or reuse
released storage. Fences remain appropriate for CPU readback and explicit storage reuse,
not an additional asynchronous public world state in this change.

Check size arithmetic before allocating or narrowing to SDL uint32 fields. Validate
per-level dimensions, offsets, format texel alignment and row/layer layout against SDL.
Retain linear-light colour reduction and direction-map normalization exactly. An empty
or invalid source follows the existing validated fallback/error contract, not silent
truncation. Public callers still receive either a complete candidate or an error.

## Bounded implementation

1. In src/render/stages/SubjectResidency.{h,cpp}, replace per-level UploadMip submissions
   with a private packed-chain builder and one upload operation. Reuse OwnedTransfer,
   OwnedTexture and BoundImage; no new renderer transaction framework or public API.
2. Keep src/render/stages/SubjectDraw.cpp slot commit atomic; inject allocation, map,
   acquire, submit and sampler failures, including after earlier images succeeded.
3. Prove public replacement A -> rejected B -> successful B preserves A's material
   handles, revision and linear pixels on failure. Reuse SceneState candidate tests.

## Acceptance

- [ ] Exactly one staging allocation and copy submission per nonempty image chain;
      odd sizes, one texel, non-square images and final 1x1 level have correct readback.
- [ ] Late failure publishes no slot/world and immediate retry succeeds without leaks.
- [ ] Existing mip texels, sampling and pixels match; deliberately corrupting one level
      makes the readback oracle fail. No fence wait or warm-up frame is introduced.
- [ ] Record upload bytes, submissions and peak temporary bytes before/after; this
      establishes overhead reduction, not a bounded per-frame streaming budget (2149).
- [ ] make format; make suite SUITE=outshine/src/render/device/FilteredMipSampling;
      relevant existing public candidate/failure cases; make lint.
