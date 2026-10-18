# Native ground classifier inventory

Owner: [#515](https://github.com/Weichwolf/outshine/issues/515).
This measures the variable lists held by the renderer, not their screen coverage
or GPU time. It does not admit a replacement class field or a device budget.

```sh
python3 test/experiments/render/ground_class_work_checks.py
python3 test/experiments/render/native_ground_class_work.py \
  --native-root "$NATIVE_CHECKOUT" --place Koerbersee \
  --output "${TMPDIR:-/tmp}/outshine-ground-class-inventory"
```

Use the existing NumPy experiment environment and a built native client checkout.
The driver takes complete compiler flags from its compilation database and links
its existing archives. A temporary renderer copy exports the classification only
when the final screenshot is read. It independently reads the current GPU buffer
and requires byte equality with the renderer-held words before writing them.
The normal renderer, shaders, assets and frame sequence remain unchanged. The
extra readback/file output occurs after the measured turn; its memory/IO costs
are diagnostic work and are not a normal renderer cost claim.

Outputs stay in system temp: captured words, generated renderer, binary, commands,
source/archive/client hashes, original logs, screenshot and a JSON receipt.
The reader validates section sizes and reachable ranges. Unused records are not
queries. Prefix sums count every visited seed/reference in each cell independently
for each tier; no polygon reconstruction or randomized workload is substituted.

## Observed Körbersee payload

The original full offline world at 1280×720/30, exactly 30 frames/360°, keeps PNG
`076343f4`. Captured CPU/GPU words are identical: 6,132,300 bytes, about 5.85 MiB.

| Tier | Cells | Seed visits | Reference visits | Mean references/cell | Max references/cell |
|---|---:|---:|---:|---:|---:|
| Fine | 128×128 = 16,384 | 17,409 | 43,814 | 43,814/16,384 = 2.6742 | 68 |
| Coarse | 256×256 = 65,536 | 79,736 | 309,549 | 309,549/65,536 = 4.7233 | 172 |

Fine cells with no reference visits: 8,199/16,384 = 50.04%.
Coarse cells with no reference visits: 23,037/65,536 = 35.15%.
Only one fine cell and 54 coarse cells exceed 64 visits. Neither tier exceeds 256.

These are unweighted cell inventories. Actual pixels can concentrate on the
expensive cells; footprint rejection, fine/coarse precedence, depth rejection,
helper invocations and camera movement matter. The tiers must not be added as
though every pixel queried both. No fragment bottleneck or cache benefit follows
from these averages. Compare bounded local arithmetic and actual shading work
before adding a new classifier cache. Reducing squared segment distances before
taking a root did not produce a runtime gain; the unchanged-image comparison is
recorded in [ground_class_distance.md](ground_class_distance.md).
