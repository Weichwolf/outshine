Type: bug
State: active
Architecture: ready
Parent: 2188
Depends:
Priority: P0
Area: render, SDL3, test
Tags: determinism, backend, measured

# A static GPU image has no first-frame exception

## Evidence and limits

Recorded local failures, to reproduce before changing code:
`MipmappedChessRepeatsLinearPixels`: 252 RGB channels, maximum 0.00195312;
`ImportedChessNativeGeometryRepeatsLinearPixels`: 504 channels, maximum 0.220703;
`ImportedChessSinglePartRepeatsLinearPixels`: 3 channels, maximum 0.000244141.
Depth/alpha match in the original case. The native clone fails through setGeometry,
so Scenario::Asset loading is not necessary to trigger it.

Paired public single-part test on 2026-09-20 uses a fresh Engine per sequence. Plain
render/linear-read pairs and the original additional depth/screenshot sequence both fail
all three repetitions: 3 channels, first index 2039986, maximum 0.000244141. Extra readbacks
are not necessary for this reproducer. An explicit plan without temporalResolve fails at
the same index, count and magnitude. History, jitter and temporal resolve are therefore not
causal. Every resident repeat matches its preceding resident frame exactly in all three
sequences. This proves a one-time first-frame transition rather than continuing instability.
The test retains all three discriminating sequences and exact first-frame equality; nine
assertions fail, rather than suppressing the defect. Next action is draw-input comparison.

Raw `FilteredMipSampling/FirstFrameMatchesRepeatedSampling` is green, including
per-level staging/submission, generated mips, eight samplers and interpolated UVs.
Perspective native quads are green with generated/imported images and four imported
UV pairs. A copied imported part becomes green with constant UV0. Source review:
SubjectProxy::Lit excludes Unlit, selecting the flat Position+Uv0 layout.

These controls narrow the reproducer; they do not exclude upload, geometry, precision,
shader compilation or sampler interactions for the failing draw. Constant UVs hide
sampling sensitivity; they do not prove a derivative bug. Different changed-pixel
counts do not prove that multipart packing amplifies one common cause. Equal rounded
depth values do not prove identical raster inputs. Cause and responsible layer remain
unresolved; no vertex-side UV rewrite or backend workaround is approved without proof.

## Decision and bounded diagnosis

Keep one smallest failing native fixture and the existing raw device matrix. Extend
these fixtures with controlled variants instead of adding another standalone copied
test for each hypothesis. Preserve placementOf(part), indices, vertex data, viewport,
camera, material, texture chain and actual selected shader products when reducing.
A changed camera/topology cannot eliminate a subsystem from the original failure.

1. Completed paired control: reproduce the single-part failure and compare render/linear-read pairs
   without intermediate depth/screenshot calls, then the original sequence. Live::Screenshot
   performs an additional ReadPixels, not an explicit draw. Audit that readback's commands
   and state changes; attribute a difference to the sequence only after paired evidence.
2. Compare first/second draw inputs at SubjectProxy, SubjectDraw and submission:
   vertex/index bytes, placement/origin, frame/material uniforms, texture descriptors,
   render state and selected pipeline. Capture immutable test snapshots, never mutable
   borrowed pointers or periodic production logs. Report only the first differing field.
3. If inputs differ, fix the producing owner/commit boundary and add a negative control.
   If they match, transfer the exact failing indexed draw and state to the existing raw
   SDL fixture. Green fullscreen sampling alone is not an equivalent reproducer.
4. A failing raw draw requires validation of the fixture and SDL contract first; only
   then classify a possible backend/compiler defect. Record SDL commit, GPU/backend,
   OS and shader product. Compare another locally available backend when available;
   its absence does not block unrelated engine work.

After these comparisons, either implement the evidenced fix or record the first
unexplained boundary under Architecture: question, with captured evidence and the
next discriminating experiment. Continue the next ready WI from 2188. No indefinite
matrix expansion, speculative ownership rewrite or cosmetic threshold change.

## Ownership, files and commands

CPU world/camera owners remain authoritative; renderer snapshots their declared state.
No API, sampler, coordinate or publication contract changes are authorized by diagnosis.
Relevant files: src/render/SubjectProxy.cpp, src/render/stages/SubjectDraw.cpp,
src/render/stages/SubjectResidency.cpp, src/render/SceneRenderer.cpp,
src/engine/Live.cpp and the existing public Outshine repeat fixtures.
Local platform reference: ../SDL at fa2c02b, include/SDL3/SDL_gpu.h.

make format; make suite SUITE=outshine/include/Outshine/ImportedChessSinglePartRepeatsLinearPixels;
make suite SUITE=outshine/src/render/device/FilteredMipSampling; make lint.
After a fix run original chess, native geometry and atmospheric repeat cases as well.

## Acceptance

- [ ] First, repeated and fresh-device static frames match with their declared rendering
      settings; no discarded first frame, idle wait, fixed LOD or relaxed tolerance.
- [ ] Paired reproducer and negative control demonstrate the repaired causal boundary.
- [ ] Unlit pipeline, texture transforms, UV selection and colour-space contracts hold.
- [ ] Relevant public/device suites and lint pass; changed PNGs are opened and assessed.
      WI 2235 may reduce upload overhead but is not presumed to fix this defect.
