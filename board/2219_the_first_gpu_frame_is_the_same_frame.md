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
The current test retains fresh linear/nearest sequences and a linear sequence after the same
Engine rendered a parallel-shifted view with identical projection and comparable mip derivatives.
It keeps texture, sampler, geometry and pipeline alive and exercises the same mip span; the target
still fails identically. Device, pipeline, texture, sampler and minified-level first use are not
causal. Exact equality remains the contract rather than being relaxed.

Raw `FilteredMipSampling/FirstFrameMatchesRepeatedSampling` is green, including
per-level staging/submission, generated mips, eight samplers and interpolated UVs.
Its former RGBA8 target could not observe the failing 1/4096 linear difference; an
RGBA16F target and an sRGB-source variant remain green. In the public reproducer,
disabling mips or selecting the nearest mip makes every frame bit-identical while
linear interpolation between mip levels retains the three-channel first-frame error.
The failing boundary is therefore trilinear sampling with the imported indexed draw,
not merely mip upload, sRGB decode, output quantization or any use of UV derivatives.
The raw control covers fractional LOD, primitive edges, perspective-varying clip W,
separate streams, 32-bit indexed indirect drawing, persistent 1280x720 RGBA16F/D32 targets,
reverse-Z and the 2048x2048 source dimension. It now also loads the pinned asset through the
importer and uses its packed first-part positions, indices, UVs, uploaded sRGB mip chain,
sampler and exact failing camera. It now also uses the production flat vertex/fragment products,
packed material uniform, emission stream, placement storage and full SubjectView uniform. Its
visible first and repeated frames remain bit-identical. Its target/depth usage, store policy,
paged buffers, allocation order and clustered indirect draw now match the engine. The public path
still fails at the original three channels even with only the subject pass. Imported raster inputs,
shader products, material layout, target state and later passes alone are excluded.
Deindexing the imported part made its own frames stable, but its resident image differed
from the indexed resident image in six channels by at most 1/4096. It is therefore not an
equivalent negative control and was removed. This topology sensitivity does not authorize
vertex duplication as a fix.
Perspective native quads are green with generated/imported images and four imported
UV pairs. A copied imported part becomes green with constant UV0. Source review:
SubjectProxy::Lit excludes Unlit, selecting the flat Position+Uv0 layout.

The internal snapshot proves the inputs differ: first-frame GPU culling emits 121440 indices;
the repeated frame emits 116448, with the first index difference at 13056. Bypassing clustered
indirect output or setting CullView.Occludes to zero makes every pixel exact; an eight-ULP and
even 0.01 normalized-depth bias do not. Temporal Hi-Z cluster occlusion is causal and the gap is
not roundoff. Depth remains equal while two pixels differ by one half-float step, so the next
decision must distinguish false-positive cluster rejection from a valid hidden-topology change.

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
2. Completed input comparison: production shader/material/raster inputs are stable; temporal
   occlusion changes indirect arguments and compacted indices before the subject draw.
3. Identify the first rejected cluster, verify its sphere and projected Hi-Z interval against
   the first-frame depth footprint, then fix the conservative occlusion contract. Do not disable
   clustered or temporal occlusion, discard the first frame or introduce an empirical bias.
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
Vulkan repeatability reference: ../Vulkan-Docs at 7d39c898c95b, appendices/invariance.adoc;
identical pipelines and inputs on one device must produce identical results. Precision limits
do not excuse different results for repeated identical state.

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
