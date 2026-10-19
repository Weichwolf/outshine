# Regional shadow-atlas updates (#817)

[Contract and current integration](https://github.com/Weichwolf/outshine/issues/817).
The existing camera-centred shadow projections already snap each region separately.
One changed region previously redrew all four atlas tiles. Reuse now follows each
unchanged world projection, while geometry and ground generations still invalidate
all regions. Light, configuration and explicit invalidation remain authoritative.

## Native execution

A submitted compatible atlas is loaded. Changed tiles receive a depth-zero
fullscreen triangle, with depth test disabled and writes enabled, followed by
normal caster rendering. Viewport and scissor cover exactly the changed tile.
Unchanged tiles retain their submitted depths. First construction, all-region
changes, content invalidation and retry after rejected submission use a complete
attachment clear. The existing StageCache/StageSubmission contract determines
acceptance. No shader, atlas format, bias, projection or GPU binding changes.

## Verification

The existing regional GPU test retains its original assertions and adds camera
translation, preserved tiles, a forced complete reference and rejected-submit
retry: 31 checks pass with Metal validation. The same new test linked against the
original 7a02281c0 archive and its matching original headers fails exactly the
work-reduction check: 30/31. Its other depth and retry checks pass.

All nine shadow cases and both affected resource/depth-history preparation cases
pass under Metal validation. Full current compiler invocations analyse all 38
product/header consumers plus the changed test: 39 clang-tidy analyses pass.
Named test constants preserve the original values; no assertions or limits change.

An additional oblique-light GPU control passes 46 checks. Retained regions have
identical coverage; full redraw differs at some occupied depths by at most
1.1920929e-7, one float epsilon, within the existing three-epsilon depth rounding
allowance. Changed regions equal the full reference exactly. Retention avoids
camera-relative float rerounding of an unchanged world projection; it does not
hold an outdated projection or lower shadow resolution.

The Vulkan colleague independently verifies 5,194 native checks with validation:
all four tile locations and an offset rectangle clear exactly to zero, while all
outside texels remain exactly 0.75. The existing Test=false/Write=true mapping uses
ALWAYS comparison with Vulkan depth testing enabled. This primitive check is
separate from complete Vulkan feature integration.

## Initial A18 Pro comparison

Reference 7a02281c0 versus the changed sources, fresh offline ABBA processes,
1280×720/60, exactly 60 frames and 360 degrees per process. Each process records
all 60 GPU times; network bytes and provider starts remain zero. Movement is 6 m.

| Moving measurement | Reference A/A | Changed B/B |
|---|---:|---:|
| GPU mean, ms | 19.986 / 20.072 | 11.995 / 11.197 |
| GPU p99, ms | 35.597 / 35.607 | 31.340 / 28.021 |
| CPU p99, ms | 46.924 / 47.957 | 16.724 / 14.580 |
| Last-frame shadow batches | 13,208 / 13,208 | 3,302 / 3,302 |

Static final PNGs are byte-identical. Moving final PNGs repeat exactly within each
method; 133 of 921,600 pixels differ between methods, 29 by more than one LSB,
maximum channel difference 13/255, at distant shadow boundaries. Images and
amplified differences were opened. The oblique depth control above isolates the
retained-versus-redrawn float-rounding mechanism; it is not a universal error proof.

The comparison driver's initial equality assertion correctly reports the moving
PNG difference; its terminal failure is preserved alongside all eight raw reports.
These measurements prove reduced work in this case, not a general frame-budget
pass. GPU p99 remains above 16.67 ms and all moving frames remain unrefined.

Original commands, CDB, source hashes, reports, logs and diagnostic images stay in
`/tmp/outshine-shadow-regions-817/` on the Metal host. Shared sources and the GPU
regression remain in the repository; private generated artifacts are not published.
