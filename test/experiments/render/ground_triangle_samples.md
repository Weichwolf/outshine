# Shared fine terrain cell stencil: not integrated

Owner [#1182](https://github.com/Weichwolf/outshine/issues/1182), terrain [#515](https://github.com/Weichwolf/outshine/issues/515).
Baseline719d74b27 includes the fine source shadow receiver from #1173.

Four 2×2 gathers cover the four corner heights and central height differences
needed by either source triangle. Normals and shadow reconstruction share those
values. The original source has nominally15 scalar fetches before compiler CSE,
but only10 distinct texels. The candidate returns16 values through4 gathers.
Fewer source instructions therefore do not establish fewer physical reads,
cache misses or bytes. Atlas/padding, assets, bindings, geometry, LOD and bias stay
unchanged. The earlier per-node two-gather experiment is a different negative
control; see [ground_height_gather.md](ground_height_gather.md).

`ground_triangle_samples.py:sources` emits three private shader replacements;
it does not modify product shaders. The independent NumPy check covers all
512 atlas slots across two layers and1024 cells per slot:524288 cell placements.
It compares Float32 normalized-coordinate addressing, all halo bounds, four
random heights and eight central differences against scalar atlas reads.
This is finite coordinate/value evidence, not a general GPU rounding proof.

All21 patch fragment variants compile. Actual Metal API and shader validation
passes the unchanged normal, raster/fraction and sun-shadow cases on the same
control/candidate binaries:96/423/17 checks. No new tolerance or source bound.

Fresh ordinary offline processes use1280×720/60, exactly60 frames and360°,
the same binary/assets, no validation or hook. The full lint finishes before
these eight captures. Every measured frame is refined and every GPU query valid.
Each candidate PNG is byte-identical to its control; both image pairs were opened.

| Scene | A1 mean ms | B1 | B2 | A2 |
|---|---:|---:|---:|---:|
| Jura |13.847073|13.606646|13.577615|13.957512|
| Körbersee |16.666635|16.787294|16.747676|17.465192|

Each mean divides all60 submission times by60. Equal sample counts give
Jura13.902292→13.592130ms,−2.231%; Körbersee17.065913→16.767485ms,−1.749%.
Jura's candidate runs are separated from both controls. Körbersee's first control
is faster than either candidate; the0.298ms difference is smaller than its
0.799ms control spread. This bounded comparison does not justify a general
default change. No further ordering, parameter or register-pressure sweep.
All GPU maxima still exceed16.67ms. No whole-device or movement admission.

An earlier first control under concurrent nice10/two-job lint had6/60 unrefined
frames despite the same final PNG. It aborted the comparison and is excluded
from the eight qualified captures, not counted as a budget pass. Load state
and `--stats` differ from that invocation; no unique cause is claimed.

Private evidence is located through `/tmp/outshine-terrain-stencil-task.txt`:
`shader-build.json`, `native-cases.json`, `world-abba.json`, `conclusion.json`,
`incomplete-baseline.json` and original logs/selected images. Product shaders
remain unchanged. The existing Geometry Clipmaps SIGGRAPH2004 reference motivates
fine appearance independent of geometry; the concrete gather stencil is local
texel algebra, not a paper result.

```sh
OPENBLAS_NUM_THREADS=1 python3 -B -W error \
  test/experiments/render/ground_triangle_samples_checks.py
```
