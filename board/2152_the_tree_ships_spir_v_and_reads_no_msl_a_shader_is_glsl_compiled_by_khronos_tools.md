Type: debt
State: open
Area: render, build
Tags: architecture, owner, audit

# The tree ships SPIR-V and reads no MSL; a shader is GLSL compiled by Khronos's tools

**Benchmark** -- Unreal: shaders are HLSL, cross-compiled per platform by the ShaderCompileWorker
(DXC, SPIRV-Cross for Metal); no `.metal` source stands in the engine. RAGE: HLSL through the
console vendors' compilers, the same shape. Filament: `matc` compiles one material source to
SPIR-V, MSL and GLSL at build; the runtime reads the package. **All agree**: one source
language, vendor forms are build products. SDL3's own answer is `SDL_shadercross` (HLSL or
SPIR-V in; SPIR-V, DXIL, MSL out) and `SDL_GPU` accepts SPIR-V on every backend it has, Metal's
included. Decided with the owner 2026-09-05: Khronos and Vulkan are the reference, the engine
reaches the GPU through SDL_GPU only, and what Apple does is not this tree's problem.

## Where it stands, measured 2026-09-05

```
  src/render/shaders/*.msl          32 files, 2 022 lines, Metal Shading Language
  SDL_GPU_SHADERFORMAT_MSL          8 sites; the device is created for MSL
                                    (src/render/SceneRenderer.cpp:245)
  ShaderText().Reads(...)           the stages concatenate .msl fragments by hand, a
                                    preprocessor of the tree's own
  CLAUDE.md                         until today asked for tile shading, mesh shaders and
                                    ray tracing to be MEASURED; SDL_GPU exposes none of the
                                    three, so the paragraph now stops where the API stops
```

## The solution

- every shader is GLSL (Vulkan dialect); `glslangValidator` compiles it to SPIR-V at build
  and the tree hands SDL_GPU SPIR-V (`SDL_GPU_SHADERFORMAT_SPIRV`); `#include` through
  glslang's includer replaces the by-hand concatenation, and the vertex arms generated from
  VertexArms.h are generated as GLSL
- the device asks for SPIR-V; MSL is derived by SDL (its Metal backend runs SPIRV-Cross) or by
  the build, and no `.msl` stands in the tree -- a claim holds `src` free of `msl` and
  `metal`
- pictures: the arithmetic is the same but the compiler path is not (glslang → SPIRV-Cross →
  Apple's compiler), so a digest may move by a rounding; every move is accepted with
  `pixels.py`'s count, its window and the look, or it goes back
- order: this opens every shader, and so does board:2149; the two are one round after the
  map of board:2101

## What will be true

### Migration evidence, 2026-09-07 (incomplete)

Depth pyramid and the subject cull/scan/compact compute kernels now compile from GLSL to SPIR-V
at build time. SDL_shadercross consumes SPIR-V and creates the backend pipeline; SDL itself does
not transparently accept SPIR-V in its Metal backend as the original text above assumed.
Reflection checks the bindings and workgroup against `ComputeShape` before creating a pipeline.
It caught the scan kernel's unused view uniform; the unused declaration and CPU upload are gone.
Husum remains byte-identical at `e1f70238`, visually opened, p99 4.10 ms and 0/120 over 16.67 ms.
This verifies the first four kernels only; graphics and atmosphere shader migration remain open.
`make shader-tools` builds pinned glslang and SDL_shadercross under `build/deps/install`;
SDL3, SPIRV-Cross, CMake and Ninja are prerequisites. Source revisions are in
`test/scripts/shader-toolchain.json`. The current bootstrap enables SPIR-V/Metal, without DXC.

### Atmosphere and fullscreen migration, 2026-09-07 (incomplete)

All eight compute kernels now use GLSL/SPIR-V. Sky, aerial perspective, present, transmission
composite, tonemap, temporal resolve and overlay also compile at build time. The medium's scalar
core remains shared with C++; its constants and the static-velocity sentinel use shared include
files. GLSL obtains pi from `acos(-1)`; the no-digit-pi claim now covers GLSL and include sources.
SPIR-V graphics and compute loaders check reflected binding counts. Binary words are aligned and
checked for header/length before reflection. Material/terrain/depth graphics shaders remain MSL.

Husum stayed byte-identical (`e1f70238`) through the atmosphere, sky, aerial and present migration.
The display migration exposed the old `std::to_string(exposure)` quantisation. Exposure now arrives
as a float uniform, along with the chosen transfer curve. At Husum the precise output `e6bc1f75`
differs at 319684 / 921600 pixels (34.6879%), by at most 1/255 per channel, across the frame.
Temporarily quantising the uniform exactly as before restores `e1f70238` through the new shaders.
At Malcesine the same negative control restores `eb6d098f`; the precise output `eebaea2a` was opened
alongside the webcam. Terrain defects and excessive contrast are still visible and not accepted.
Controls were removed afterwards. Logs: `build/glsl-display-*` and `build/glsl-fullscreen-*`.

### Acceptance (still open)

- [ ] `find src -name '*.msl'` reads 0 and a claim keeps it there
- [ ] `SDL_GPU_SHADERFORMAT_MSL` appears nowhere; the device is created for SPIR-V
- [ ] the nine references bit-identical, or each move named pixel by pixel with its cause
- [ ] Negative control: a `.msl` file placed under `src/render/shaders` fails `make lint`

## Depth migration, 2026-09-07 (incomplete)

Subject and terrain depth vertices and their empty fragment now use GLSL/SPIR-V.
Their MSL sources and obsolete public source-generator declarations are removed.
Ground grid constants are shared through `GroundConstants.inc`; skirts remain pending WI 2166.
SPIRV-Cross reflection gives DepthView 80 bytes, matrix offset 0/stride 16 and shift
offset 64, matching the CPU's 20 floats. Placements use mat4 array stride 64.
Husum stays byte-identical (`e6bc1f75`, p99 2.99 ms, 0/120 over). Malcesine first
produced `017db4a3`, then without any edit `eebaea2a`, identical to pre-depth migration.
The two differ at 1103 of 921600 pixels, 307 above one channel step, maximum 21/255.
Both were opened. Repeated-output variation remains unexplained; no claim of
deterministic equivalence follows from the matching run. Logs: `build/glsl-depth-*`.
The terrain shadow draw has no caller (see WI 2167), so these renders only validate
its pipeline creation, not terrain shadow pixels.

Depth binding negative control: removing the placements SSBO from a temporary compiled
vertex made `make shots PLACE=Husum` refuse `lightVisibility` with the exact reflected
binding mismatch (exit 2). The original SPIR-V bytes were restored in a finally block;
a subsequent Husum render succeeds. See `build/glsl-depth-negative.log` and
`build/glsl-depth-restored-husum.log`. No control remains active.

## Lit terrain material migration, 2026-09-07 (incomplete)

The lattice's lit vertex and fragment use GLSL/SPIR-V with build-time output variants. `groundLattice.msl` and `groundLatticeCore.msl` are deleted. Material storage preserves the existing 77 packed scalars: 35 base values + 6 UV matrices * 6 scalars + 6 UV selectors = 77; times 4 bytes gives 308. Reflection verifies all member offsets 0,4,...,304, and the lighting block remains 1088 bytes. Sheen/GGX tables are computed by the same C++ quadratures at build time (`test/scripts/shader-tables.cpp`), with the existing quantisation. Source material semantics are unchanged in this migration.

SDL_shadercross reflects ACTIVE resources, unlike a plain SPIRV-Cross `--reflect` inventory. The terrain has its own dense binding layout: one shadow sampler, two class/palette buffers, two fragment uniforms. This does not fix the still-unread sky irradiance. Subject/material variants still use MSL; a broad shared resource count will not work for those either, because inactive resources are omitted during conversion.

Husum stays `e6bc1f75`; Malcesine stays `eebaea2a`. Both outputs opened. The initial GLSL shade function copied `Lights` (including its array) by value: Malcesine p99 15.91 ms, 1/120 over. Reading the uniform directly gives p99 4.96 ms, 0/120 over. Replacing only the compiled fragment with the by-value control reproduces p99 13.22 ms and 1/120 over, with identical pixels. Original SPIR-V restored in finally. Logs `build/glsl-ground-*`; control sources are only under `build/material-migration/`.


## Unlit subject migration, 2026-09-07

`subject.msl` is removed. Flat vertices and opaque/masked/blended/transmissive fragment variants now compile from GLSL includes to SPIR-V at build time. The CPU selects dense reflected bindings; a pipeline change invalidates the cached material binding. Lit/mapped subjects still use MSL; twelve MSL files remain.

Controlled old-MSL versus restored GLSL runs produced byte-identical PNGs for AlphaBlendModeTest, UnlitTest, VertexColorTest and TransmissionTest. Logs: `build/flat-control/old-msl-render.log`, `build/glsl-flat-restored.log`; old PNGs retained under `build/flat-control`. Both camera cases and both UV cases also retain their earlier results. AlphaBlendModeTest remains red (6339 pixels); this is not a new GLSL regression. The harness loses material alphaMode when overriding rows, and the scenario reader does not read it. TransmissionTest's emission lowering does not establish transmission correctness. Full material semantics remain unverified; no thresholds changed.

Places: Husum e6bc1f75, p99 3.38 ms; Malcesine017db4a3, p99 5.05 ms; zero/120 over for both. Both images opened and visually unacceptable. Malcesine's second digest was observed before this migration too; nondeterminism remains unresolved.


## Lit migration and controlled comparison, continuation 2026-09-07

All active shader paths now load build-time GLSL/SPIR-V. The remaining twelve MSL files and runtime ShaderText/source-generation helpers are removed. Lit/mapped vertices and material fragments use regular includes and dense SurfaceBindings; ground uses its existing GLSL fragment. SceneRenderer requests SDL_shadercross-supported SPIR-V backend formats. Lit shading reads the global light uniform instead of copying its 1088-byte structure. This completes the source-path migration, not the full work-item acceptance: broader lint/entry guard, backend packaging and rendering conformance remain open.

`build/glsl-lit-places.log`: Husum e6bc1f75 p99 3.43 ms, Malcesine eebaea2a p99 4.92 ms, both 0/120 over. These are unchanged previous digests, not a lighting or terrain repair.

`build/lit-control/run.py` temporarily restored the old lit MSL implementation plus its original source builders, leaving flat GLSL intact. The script snapshots all touched bytes and restores them in finally. `build/lit-control/old-msl-render.log` and the positive rebuilt `build/glsl-lit-restored.log` produce byte-identical PNGs for NormalTangentTest (6294e171), NormalTangentMirrorTest (07d3bb0d), SpecularTest (fbe35755). Full byte comparisons and all restoration hashes checked. No temporary control remains; current binary is GLSL. All three images and their oracles opened. The corpus results were red identically under both languages; subsequent orthographic-camera and declared-environment corrections are recorded in board:2096, and leave substantive shading differences open.


GPU integration exposed a pre-existing execution-contract defect beyond source translation: IrradianceBuffer was only a Handle in the catalogue, so its compute output was not bound; independent zero-based compute buffer layouts were also merged incompatibly. Table/stride and merge checks are repaired, with before-red/after-green binding regression and visible GPU-light negative controls. See board:2167's continuation. This is why successful shader compilation alone never established runtime correctness.
