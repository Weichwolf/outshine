Type: bug
Area: corpus
Tags: oracle, instrument

**Cycles' first Metal frame costs 200 s and would be attributed to the scene**

Not a defect in the tree yet — a **trap laid for § I.26's oracle**, recorded here because the harness
that walks into it does not exist yet and the number is cheap to lose. Measured on this host, Blender
5.2.0 LTS (`fbe6228777e7`, built 2026-07-14), factory startup cube, 1280×720, 128 spp, adaptive
sampling off, denoising off, `diffuse_bounces = 0`, seed 0, OpenEXR float32:

| device | cold | warm |
|---|---|---|
| Metal, Apple A18 Pro, 5 GPU cores | **200.9 s** | **2.087 s** |
| CPU, Apple A18 Pro | — | 11.6 s (128 spp) · 49.6 s (512 spp) |

The 200.9 s is Cycles compiling its Metal kernels once per kernel-cache generation. A reference run that
times its first frame attributes it to the scene and reports a per-frame cost two orders out. Right: the
oracle harness renders one throwaway frame before it starts timing, and publishes *cold* and *warm*
separately as the instrument's own floor beside the result.

A second trap in the same measurement, and it is the one that produced the CPU column: setting
`scene.cycles.device = 'GPU'` is **not** sufficient — `preferences.addons['cycles'].preferences` must
have `compute_device_type` set *and* `get_devices()` called *and* the device's `use` flag set, or Cycles
falls back to CPU silently and the run is 5.6× slower with no message. A harness that does not assert
the device it got has measured something it did not choose.


---

**Closed by the backlog adjudication (2026-08-22).** in_blender_render.py pins METAL compute, enumerates devices and publishes the backend; cold-frame cost is published by jobs.py and never timed as scene cost.
