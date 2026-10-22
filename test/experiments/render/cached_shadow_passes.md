# Cached shadow passes preserve data without GPU load/store work

Owner #16; native integration #531. A submitted unchanged shadow atlas made
LightVisibilityStage::Encode return immediately. SceneRenderer had already begun
a 4096×4096 depth render pass with Load/Store. The cached frame therefore touched
the attachment despite requiring no shadow draw.

The fast path applies only to a compiled pass containing LightVisibility alone
and an existing valid shadow cache. It still executes the CPU stage: ShadowedBy
must refresh the current-frame lighting bindings and matrices. It omits the GPU
encoder. Dirty caches, changed casters/light/ground and rejected submissions keep
the existing real recording path. No shaders, bindings, layouts, cache formats,
resolution, world content, simulation, jitter or measurement windows change.

## Verification

The extended real-device FramesPublishOnlyAfterSubmission case counts atlas
encoder starts. An unchanged submitted atlas must preserve every depth texel
without beginning a new pass; invalidation followed by submission failure must
still record a real pass when retried. Normal and GPU-validated builds pass.
The independently rebuilt original SceneRenderer fails exactly the new cached
pass assertion; every other assertion still passes. Tests were not relaxed.
Existing string-membership assertions retain their meaning with C++23 contains.

Full production compilation-database flags pass clang-tidy for SceneRenderer.
The case uses every actual compiler flag from its successful runner invocation;
object/library/framework link inputs are separated from its compile database.
Apple-only options inferred in a compiler-expanded MJ fragment were not silently
dropped or substituted for the actual driver arguments. Initial setup failures,
commands and terminal results remain explicit in the private receipt directory.

All 12 ordinary fresh offline Places at 1280×720/60, 60 frames/360°/60 GPU queries are
byte-identical to the accepted images. The current Vienna and Zürich images were
also opened. Preparation remains separate from the complete measured window;
no bad first frame is omitted and no extra frame is added.

Two matched A18 Pro counter pairs preserve the complete GPUROW submission sums.
Each original 60-frame trace contains 60 depth-only 4096² encoders; the candidate has
none. The real preparation shadow encoder remains. Measured ordinary frame
encoder count falls from 11 to 10. Separate overlapping stage durations are not
summed or treated as exclusive utilisation.

| Profiled full window | Original GPU mean ms | Candidate GPU mean ms | First frame original/candidate ms |
|---|---:|---:|---:|
| Koerbersee |13.226272|12.762719|29.646 /21.558|
| Tokyo |14.092416|13.957313|25.759 /24.452|

These individual pairs prove the missing redundant work and image preservation;
they do not establish a general speedup, full device budget or completed startup
fix. First and later frames can still exceed 16.67 ms. Full-lint admission of the
complete native branch remains open.

A separate private first-touch control actually cleared/stored 18 new 2D attachments
before use. Images and 60-frame windows stayed unchanged, but first frames still
cost 26.891–27.398 ms and later frame 2 could be slower. Attachment initialization
alone does not prove complete readiness; that prototype was not retained.

Originals, final source/binary hashes, actual flags, negative/positive controls,
complete 12-place receipts and raw counter traces remain under
/tmp/outshine-cached-shadow-passes-3aa9d219c/. The separate attachment control is
under /tmp/outshine-metal-attachment-readiness-3aa9d219c/. Reproduction uses the
ordinary place driver and the opt-in Metal encoder profiler from
[metal_encoder_profile.md](metal_encoder_profile.md).
