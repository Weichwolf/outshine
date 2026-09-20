Type: bug
State: active
Architecture: ready
Parent: 2188
Depends:
Priority: P0
Area: render, SDL3, test
Tags: determinism, backend, measured

# A static GPU image has no first-frame exception

## Problem

Static imported chess frames differ from the first resident frame while depth is exact.
`MipmappedChessRepeatsLinearPixels` changes 223 channels by at most 0.00268555;
`ImportedChessNativeGeometryRepeatsLinearPixels` changes 322 by at most 0.00268555.
Every later frame matches. Exact equality is the contract; no warm-up, discarded frame,
fixed LOD or relaxed tolerance.

The reduced first-part case originally changed three channels by one half-float step.
Its first cull emitted 121440 indices and its second 116448. Job 34 lost 384 indices.
Its sphere contains its vertices and projects conservatively: reverse-Z sphere nearest
0.533213, actual vertex nearest 0.531105, Hi-Z farthest 0.544359. The rejection is valid,
but history-only topology refinement changes trilinear results at equal depth.

The reduced case is repaired: unchanged camera-relative view and geometry generation reuse
the last visibility, indirect arguments and compacted indices. Cull, scan and compact rerun
after view or geometry changes; moving views retain temporal occlusion. The former per-frame
argument reset was redundant because scan writes every batch count and prevented reuse.

The full native opaque control still changes 99 channels with identical visibility,
arguments, compacted indices and depth. Transmission adds nine changes; the default plan
adds a further 214. This is a second boundary after culling, within multi-part raster or later
passes. Do not attribute it to temporal culling or reopen the proven sphere calculation.
With native shared material slots and transmission disabled, prefixes through part 6 are exact;
adding part 7 `Pawn_Body_W2` exposed the first failure in one run. The two constituent groups
remain exact separately, and replacing part 7 with part 8 also failed once but passed after a
rebuild. Treat this as a multi-draw/resource-layout boundary, not a content-specific threshold.
An idle wait, explicit/fine gradients, manual trilinear filtering and exact coincident-triangle
removal leave the full opaque failure. Constant colour, a fixed texel and mip-zero UV filtering
are exact; the computed derivative LOD itself changes. Vulkan specifies repeatability for
identical side-effect-free commands, while this path consumes buffers written by compute.

## Implementation

Keep the smallest public single-part reproducer, the internal cull snapshot and the raw
`FilteredMipSampling/FirstFrameMatchesRepeatedSampling` matrix. The raw fixture already
uses the pinned first part, production shaders/material packing, generated mips, RGBA16F/D32,
reverse-Z, paged buffers and clustered indexed indirect drawing and remains exact.

1. Preserve the reduced fix and verify camera movement still exercises Hi-Z.
2. Reduce the full native opaque case by part, material slot, image and draw batch while
   asserting identical visibility, arguments and compacted indices. Find the first added part
   that changes a pixel; retain draw order, placements, UVs, texture chain and camera.
3. Compare one multi-part draw using a shared material/image against distinct material/image
   bindings. Inspect upload publication and resource transitions only after that boundary.
4. Add transmissive and default-plan stages separately after opaque multi-part raster is exact.
5. Fix the first evidenced state/lifetime/order defect. Do not deindex geometry, special-case
   the asset, add empirical depth bias or weaken the equality contract.

If identical command inputs still differ, validate the fixture and SDL contract before calling
it a backend defect. Record SDL commit, GPU/backend, OS and shader product. A second backend is
useful when locally available but does not block engine-side diagnosis.

## Ownership and commands

CPU world/camera state remains authoritative; renderer snapshots declarations. No public API,
sampler or coordinate contract changes are authorized. Relevant files:
`src/render/stages/SubjectCullStage.*`, `SubjectDraw.*`, `SubjectResidency.*`,
`src/render/SceneRenderer.*`, `src/engine/Live.*` and existing repeat fixtures.
Local references: SDL `fa2c02b`; Vulkan-Docs `7d39c898c95b` invariance appendix.

`make format`; focused repeat suites; `make lint`. After the next fix run the single-part,
full native, scenario-loaded chess, raw sampling and atmospheric repeat cases.

## Acceptance

- [ ] First and repeated static frames match under every declared rendering plan.
- [x] Reduced paired reproducer proves and repairs the temporal-cull boundary.
- [ ] Multi-part opaque raster and transmission have isolated negative controls.
- [ ] Unlit, texture transform, UV, colour-space and moving-view occlusion contracts hold.
- [ ] Relevant public/device suites and lint pass; changed PNGs are opened and assessed.
