# Native light-list control — restart checkpoint

Refs #448; goal #190. This is an unfinished private raster experiment, with no
production renderer, shared binding, public API or runtime light-limit change.
The existing stage port #328 remains the sole renderer transition.

`export_local_light_native.py` prepares full-camera tiled/logarithmic lists using
the source-preserving Vienna lamp recipe from #455. Native cases retain 1920×1080
and four complete receiver planes at 25/250/500/5,000 m. These are procedural
test receivers, not road/terrain geometry. Mixed controls author spots, two
directional sources and one unbounded point. The declared shadow owner has its
original world index, deliberately different from its global-list ordinal.

`local_light_native_shaders.py` copies the actual `subjectLighting.glsl` body to
the explicit private output. It changes enumeration, original-ID shadow lookup
and a support counter, preserving the BRDF/attenuation body. Material type comes
from the actual material declaration. Black environment and a prescribed shadow
texel isolate direct shading. Eight material stripes exercise existing direct
responses, without accepting textures, Coverage, transparent composition or the
full framepipeline. The support counter is diagnostic work; its GPU times are
not production frame times.

`local_light_native.cpp` uses matching native headers and the regular production
archive. Private storage slots are 2/24 (existing 64-byte Light records) and
2/25 (uint CSR/global/depth-edge words), cluster frame 3/4 (64 bytes), with an
additional private camera scaffold at 3/5. The latter is not a runtime proposal.
Existing Lighting stays 3/1. Sparse bindings are native-only; no SDL-root audit
or shader guard is changed. Each render-only submission is timestamped, then
readback runs separately. Methods alternate order between repetitions.

Verified before the requested Linux restart:

- All 1,787 hashes of native closure `3af55ff02` match; the private copy retains
  its complete source/header/archive/shader manifest.
- Strict C++ build and native 17-light mixed control complete successfully:
  RTX 3060, Vulkan synchronization validation, 294 checks / 0 failures, twelve
  actual RGBA32F images at 1920×1080. All eight tiled/log versus full-sum image
  comparisons have zero RGB difference, zero missing-support counts and no
  nonfinite values. Every plane is nonempty.
- Ruff and clang-format pass. All four current shader variants compile:
  brute, listed, deliberately first-16-only and deliberately wrong shadow owner.
  The negative variants have not yet been executed.
- Focused LLVM21 analysis terminates but still reports eight owned-source
  findings and existing harness-header findings. This is not tidy-clean.

Next: finish the analyzer findings; replay the current generated positive shader
and execute both negative controls; run 1,024/8,192 lights, overlap/overflow,
turn/orthographic/movement; inspect actual images in a PDF; compare GPU cost
without support-counter overhead and implement/measure native list construction.
No native winner, runtime feature, complete allocation/copy/device budget, Place
or A18 acceptance is claimed. Concrete CSR/queue/lifetime and Metal contracts
remain in #448, before runtime attachment.

The linked draft PR and #448 identify the durable restart artifact bundle and
its checksum. Normal new private outputs return to the system temp directory.
