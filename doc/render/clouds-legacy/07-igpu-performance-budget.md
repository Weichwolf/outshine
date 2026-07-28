# G — Iris Xe Performance Budget

> **Legacy studies of the demolished FBCloud* chain; kept for the noise/raymarch groundwork, see [../flightbox/render/clouds.md](../clouds.md).**

Source: Hillaire, SIGGRAPH 2016 Frostbite course (slide 44, EXACT — Xbox One measured costs); Nubis³
2023 (slides 46–50, EXACT — PS4/PS5 measured costs); Schneider 2015 (slide 92, EXACT — PS4 target);
hardware TFLOPS figures from GPU spec aggregators (GadgetVersus/CPU-Monkey/NeoGAF-sourced console
figures — cross-checked against multiple listings, flagged as such below); WGSL `f16` status from the
WebGPU/Chromium spec tracker.

## 1. The comparison class — is "transfer the PS4/XB1 number" even valid?

| GPU | Peak FP32 TFLOPS | Memory bandwidth | Era |
|---|---|---|---|
| **PS4** (used by Schneider/HZD, 2015) | **1.84** (18 CUs, GCN1) | 176 GB/s (shared with CPU) | 2013 |
| **Xbox One** (used by Hillaire/Frostbite, 2016) | ~1.31 | 68 GB/s (ESRAM 204 GB/s for a 32 MB pool) | 2013 |
| **PS5** (used by Nubis³, 2022/23) | ~10.3 | 448 GB/s | 2020 |
| **Intel Iris Xe (96 EU, Tiger Lake/Alder Lake mobile)** — FlightBox's target | **~1.5–2.2** (varies by clock/TDP; multiple sources disagree, see below) | **shared system RAM, ~50 GB/s (task brief figure) — much lower than any of the above** | 2020 |

The FP32 compute figure for Iris Xe (96 EU) puts it **in the same rough class as the PS4/Xbox One
GPUs** the two primary cloud talks were optimized for — this is the load-bearing justification for
using their millisecond budgets as a starting reference point at all (the PS5-class Nubis³ numbers
are NOT a valid direct comparison — 5–8× more compute, 2–8× more bandwidth). Sources for the Iris Xe
figure disagree by a wide margin (1.5–2.4 TFLOPS depending on clock bin/vendor SKU/TDP) because "Iris
Xe 96EU" spans several actual clock configurations across Tiger Lake and Alder Lake mobile parts —
treat **~1.7 TFLOPS** as a reasonable central estimate, not a precise spec.

**The bandwidth gap is the real constraint, not compute.** PS4's 176 GB/s and Xbox One's 68–204 GB/s
(ESRAM) are both **higher** than Iris Xe's shared-system-RAM ~50 GB/s (task brief figure) — and unlike
a console, that bandwidth is shared with the CPU, the OS compositor, and (in FlightBox's case) the
terrain tile streaming pipeline. **Any budget copied from these consoles should be treated as an
compute-only upper bound; the actual achievable frame cost on Iris Xe will likely be bandwidth-bound
below that compute ceiling**, especially for anything sampling the 3D noise volumes at every march
step (128³×4 B texel reads × up to 160 steps × ~86,400 quarter-res pixels at 720p/4 — see §3).

## 2. Reference costs from the primary sources (use as budget anchors, NOT hard targets)

| Source | Configuration | Cost |
|---|---|---|
| Schneider 2015 (HZD, PS4) | Full talk's technique before the 1-in-16-pixel temporal trick | ~20 ms |
| Schneider 2015 (HZD, PS4) | **After** the temporal trick (quarter-res, 1/16 update) | **~2 ms** (stated target) |
| Hillaire 2016 (Frostbite, XB1, 720p) | Hemisphere sampling (scattering & coverage) | 0.090 ms |
| Hillaire 2016 (Frostbite, XB1, 720p) | Main view w/ temporal reprojection, 640×360 | 0.720 ms |
| Hillaire 2016 (Frostbite, XB1, 720p) | Planar reflection (optional), 128×72 | 0.035 ms |
| Hillaire 2016 (Frostbite, XB1, 720p) | **Total** | **0.835 ms** |
| Hillaire 2016 (Frostbite, XB1, 900p) | Total (960×540 main view) | 1.228 ms |
| Nubis³ 2023 (PS4-class fixed-step march) | 4.2 ms | before cone-step accel |
| Nubis³ 2023 (PS4-class cone-step march) | 1.3 ms | after cone-step accel |
| Nubis³ 2023 (PS5, prototype voxel renderer, 960×540) | 4.0 ms | flight-through-clouds prototype, NOT the shipped scheme |

**Realistic FlightBox target given the bandwidth gap above: 2–4 ms**, i.e. toward the pre-optimization
end of this range rather than the 0.8–2 ms figures measured on consoles with 1.4–9× the memory
bandwidth. At FlightBox's fixed 60 Hz / 16.7 ms frame budget, 2–4 ms for clouds alone is a large slice
— this argues strongly for the temporal accumulation scheme in
[05-temporal-reprojection.md](05-temporal-reprojection.md) actually being load-bearing for performance,
not just visual quality, on this hardware class.

## 3. 3D-texture bandwidth budget — concrete for FlightBox's existing sizes

| Texture | Size | Bytes (RGBA8Unorm) | Per-march-step cost |
|---|---|---|---|
| Base shape | 128³ | 8 MiB total | trilinear = 8 texel fetches × 4 B = 32 B/sample (cache-amortized in practice) |
| Detail | 32³ | 128 KiB total | same, smaller footprint, fits in a much smaller cache working-set |

8 MiB fits comfortably in Iris Xe's shared L3 (typically 1.5–8 MB depending on SKU, but the working
SET per frame — the texels actually touched along active rays — is far smaller than the full texture,
since only a thin shell around the current cloud deck altitude is sampled). **Keep both textures at
their current size; this is not the bottleneck.** The bottleneck, per §1/§2, is the **step count ×
pixel count** product — at quarter-res 720p (180×80 ≈ 14,400 px) × up to 160 steps × (1 density sample
+ up to 6 light-march sub-samples) ≈ **up to ~16M texture fetches/frame** in the worst case (deep
cloud deck, quality=1, no early exit) — this is the actual cost driver, not texture size.

## 4. WGSL `f16` — status and applicability

`enable f16;` is a ratified WGSL/WebGPU extension (`shader-f16` feature); shipped in Chrome since
v120, supported in Firefox's and Safari's WebGPU implementations. **Requires the adapter to expose the
`shader-f16` feature** — not guaranteed on every Iris Xe driver/OS combination; must be feature-detected
and have a working fallback (the existing `f32` path), not assumed present. Where it helps: halving
the register/bandwidth cost of the noise-fetch and accumulation math in the march loop (scatter/
transmittance accumulators, density intermediate values) — a plausible **10–20% cost reduction** on
the inner loop is a reasonable expectation for this class of shader (arithmetic-light, fetch-heavy),
not a multi-x speedup; the win is proportional to how bandwidth- vs. compute-bound the loop actually
is, which per §1 is likely significant on this hardware.

## 5. What does NOT fit on this budget class

- **The Nubis³ PS5 voxel-cloud prototype** (§2, 4 ms on 10.3 TFLOPS/448 GB/s hardware) — the compute
  and bandwidth gap to Iris Xe is too large; not a realistic target.
- **Per-step light-march without the 0.3-alpha cheap-switch** ([03-lighting-model.md §6](03-lighting-model.md))
  at full step counts, on every primary march step — this multiplies the fetch count from §3 by up to
  6×; the cheap-switch (or an equivalent early-exit) is not optional on this hardware class the way it
  might be treated as one on console-class GPUs.
- **A full 3D voxel density field at meter-scale resolution** (Nubis³'s "voxel clouds" direction,
  slides 2000+ in that deck) — orders of magnitude more memory and bandwidth than the noise-composite
  approach; not applicable here.
- **16-frame full-scene amortization for near clouds** ([05-temporal-reprojection.md §4](05-temporal-reprojection.md))
  — flagged by Nubis³ itself as not flight-capable regardless of hardware class; doubly inappropriate
  here since the lower bandwidth makes convergence slower, not faster.
