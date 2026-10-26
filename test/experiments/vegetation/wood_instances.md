# Reuse complete cached wood prototypes before vertex expansion

Owner: [#573](https://github.com/Weichwolf/outshine/issues/573), under #182.
The [wider spruce experiment](spruce_growth.md) supplies the same complete cached
input. This compares assembly representations, not new growth or fewer axes.
The default expanded portrait remains available. No library/native/material
contract, cached recipe, physical needle window or source ID changes.

## Why this experiment

The existing close reference transforms four order-specific wood templates into
1,272,118vertices and7,045,584corner normals on the CPU, then copies expanded
buffers into Blender. The shapes are already identical within each order.
`window_wood_instances.py` reuses the existing `blender_instances.instance_group`
with4templates and48,927point transforms. No object per twig, realization of
instances, growth execution, full needle placement or atlas baking is needed.
All axes, source order/identity mapping, analytic normals, windows and light
blockers remain. Instancing does **not** reduce2,348,528wood triangles or the
complete2,814,716tree triangles including three window roles.

First consulted local SIGGRAPH1995 Weber/Penn §5, PDFp.6 (also opened optically):
keep one geometric description and reinterpret it at range instead of building
separate full trees. This experiment implements prototype reuse only, not their
range degradation or primitive substitution. Existing local Ghidra output for
SpeedTree10.2 `CBatchedLeafNode::ComputeBatchWorker` (`01d1d5b0`) was inspected
as a structural batching reference; its unresolved types do not establish a
vendor instancing algorithm, cost model or equivalence. No vendor code/assets.

## Transform and buffer correctness

`wood_templates`/`wood_orders` are small extractions of the existing expanded
builder. All expanded output positions, triangle indices, corner normals and
dtypes are byte-identical to frozen `48b14fba8`. The independent scalar controls
also compare transformed prototype vertices and topology against that builder.

The existing instance helper writes FLOAT32 origins/Euler rotations/scales to
point attributes. Small controls inspect actual Blender dependency-graph
instances, transforms and prototype corner-normal attributes, with rotated,
curved and differently scaled inputs. Invalid counts fail before mesh creation.
`wood_instance_replay.py` checks the complete actual dependency graph in1024axis
batches. Every original axis appears exactly once; all1,272,118vertices and
7,045,584corner normals are compared against the cached FLOAT64 poses. Maximum
position component error3.274µm, normal component error4.603e−7. This is a
local-coordinate/float-transport measurement, not an image or arbitrary world-
coordinate precision guarantee. Renderer shader transport is checked separately
in the complete lit image below.

## Measured work and memory

Frozen producer/renderer: `fed9021ffca54b16e890d7f32642990ad0a71216`.
Host:4logical CPUs/16GiB, Python3.13, NumPy2.5.4, bpy5.2.2, softwarellvmpipe.
CPU preparation is a same-process sequential comparison while a separate
software render is active; it is not a general speedup/device/frame claim.

| Scoped CPU preparation | Expanded | Prototype groups |
|---|---:|---:|
| Three-run median, s | 0.8656 | 0.0340 |
| Returned array bytes | 127,994,760 | 6,284,352 |
| One-call traced peak bytes | 132,874,812 | 10,264,704 |
| One-call traced live bytes | 128,002,280 | 6,296,356 |
| One-call traced live blocks | 140 | 223 |

Prototype groups retain copied pose/ID arrays plus21,696template source bytes;
they avoid per-axis vertex/normal transform arrays. Live block count increases;
it is not a cumulative allocation-event count. Traced Python/NumPy peaks exclude
untracked native allocations and are not process/GPU peaks. Explicit Blender
buffer writes shrink from146,782,984 to1,775,356bytes: positions, triangle-loop
ranges/indices, analytic corner normals and the36bytes/point TRS attributes.
This does not count all internal Blender/driver copies. Final point/prototype
payload accounting differs from intermediate CPU group arrays.

The actual instance dependency-graph check takes3.031s with402.984MiB peakRSS;
its temporary validation matrices/arrays are diagnostic work, not renderer
preparation. Full assembly-only processes and actual lit render have separate
receipts. No GPU memory/time, RTX3060/A18, loading, wind or LOD gate is claimed.
Thin wood still needs geometric detail selection before a forest-scale draw.

| Fresh complete process | Expanded | Instanced |
|---|---:|---:|
| Assembly-only wood preparation/binding, s | 2.675 | 0.132 |
| Assembly-only whole population, s | 3.830 | 1.498 |
| Assembly-only process peakRSS, MiB | 778.688 | 477.957 |
| Lit render time, s | 387.013 | 394.394 |
| Lit process peakRSS, MiB | 3377.137 | 2946.324 |

Assembly-only omits renderer/dependency-graph work and has no GPU backend/time.
The full render retains all instances through actual shading/shadow evaluation.
The historical expanded render and current instanced render are single local
software runs under different small-check load; **no render-time gain** follows.
Keep instancing for its measured CPU/buffer/peak savings, not a faster frame.

## Opened complete image and decision

Both2560×1440/32sample portraits were opened on the same flat camera/light stage.
Wood, crown and ground shadow retain their appearance. `test/scripts/pixels.py`
finds3,622/3,686,400pixels differ (0.0983%);89pixels differ by more than1/255,
maximum4/255, mean RGB-channel error0.000413/255. These small instance/float/
renderer differences are not bit equality. This admits the local assembly
alternative, not universal image parity. The upper crown stays too narrow and
sparsely occupied; this experiment improves no botanical form or materials.

Instanced PNG SHA256 is recorded with the frozen renderer receipt in the private
proof. Before: #562 `lit-irregular/portrait.png`; after: `lit-instanced/portrait.png`.
No new presentation images are published under `doc/`.

## Verification and reproduction

Three new instance controls plus seven existing portrait controls pass from
source and the same installed reference wheel, no skips. The unchanged library
permits reuse of #562's installed wheel; all54previously loaded package-module
hashes still match current source. Wheel SHA256:
`a1d5a7cbf8e91ff6718d6c595f2a7d22cb5eed7f48d3fba86c7cbb78bdadc7bb`.
Ruff format/check, functions<=120lines and `make lint-docs` pass. Native gates
are outside this Python-only experiment.

Use `.venv`, `PYTHONPATH=lib/src:test/experiments/vegetation`, explicit `/tmp`
outputs, and the complete cached product/catalog recorded in #562. Commands:

```
.venv/bin/python -m unittest wood_instance_checks window_portrait_checks
.venv/bin/python test/experiments/vegetation/wood_instance_replay.py \
  --plant-product INPUT --catalog CATALOG --output /tmp/replay.json
.venv/bin/python test/experiments/vegetation/wood_instance_cost.py \
  --plant-product INPUT --catalog CATALOG --output /tmp/cost.json
.venv/bin/python test/experiments/vegetation/window_portrait.py \
  --plant-product INPUT --execution INPUT/execution --catalog CATALOG \
  --wood-mode instanced --output /tmp/portrait
```

`--wood-mode expanded` selects the unchanged reference. `--assemble-only`
reports assembly without creating a PNG or reporting a GPU backend/render time;
it does not force all renderer dependency-graph work. Research images remain
private. Evidence root `/tmp/outshine-vegetation-wood-instances-proof-48b14fba8/`:
`render-source.json`, `replay.json`, `cpu-cost.json`, `assembly-expanded/`,
`assembly-instanced-checked/`, `lit-instanced/`, source/installed check logs and
`reused-wheel-check.json`. Botanical crown/material/LOD/stand quality remains
open in #182/#131/#168/#1 regardless of this assembly comparison.
