# Real recipe intermediate-lifetime comparison — #1229

`recipe_product_lifetimes.py` measures the existing seed-718 young-pine detail
and the third fixed source hall shell, without new recipes or geometry. It exports
actual array streams, product fingerprints/metadata and unchanged accounted stats.
`--reference` requires exact types, shapes and array words plus identical canonical
product records. A plan-only plant request is also available as a small control.

Each worker loads one domain to avoid collisions between the experimental domain
folders' `demand` modules. `--kernel` selects an explicitly supplied trusted
`recipe_graph.py` before domain imports; it does not load recipe-provided code.
This is a diagnostic execution option, not an asset/runtime format or public API.
Definitions, domain operators, output names and versioning remain unchanged.

The baseline is the existing retain-all `Prepared.evaluate`. The locally tested
candidate counts remaining consumers per port, pins requested exports, discards
unused product references after their last consumer and drops loop-local argument/
emission references before the next callback. It never changes a payload. The exact
28-line candidate patch against baseline `6afe45b0a` is retained in the #1229
[candidate comment](https://github.com/Weichwolf/outshine/issues/1229#issuecomment-6383422583), not adopted in the kernel or maintained as another runtime path.

The initial two real consumers saved only approximately 0.1% of traced peak.
The extra bookkeeping is therefore rejected for this pilot. This does not reject
last-use release for future larger product graphs. Both real graphs need their
important inputs until the final geometry-producing callback; native callback
scratch and returned products are separate from dead wrapper references. The
accounted emission sum must not be presented as unique live physical memory.
No extra rooflight/capacity series follows without a new concrete need.

Reproduce with the existing `.venv`; prepare the alternative module by copying
the baseline source into a private temporary worktree and applying the patch
linked from #1229. No normal test depends on that private path.

```sh
PYTHONPATH=lib/src:test/experiments/assets OPENBLAS_NUM_THREADS=1 \
  OMP_NUM_THREADS=1 .venv/bin/python -B -W error \
  test/experiments/assets/recipe_product_lifetimes.py --case plant-detail \
  --output /tmp/outshine-lifetime-before

PYTHONPATH=lib/src:test/experiments/assets OPENBLAS_NUM_THREADS=1 \
  OMP_NUM_THREADS=1 .venv/bin/python -B -W error \
  test/experiments/assets/recipe_product_lifetimes.py --case plant-detail \
  --kernel /tmp/outshine-lifetime-candidate/recipe_graph.py \
  --reference /tmp/outshine-lifetime-before --output /tmp/outshine-lifetime-after
```

Use `--case hall-shell` for the other real consumer. The driver records the actual
loaded kernel hash, first/following wall and process CPU, timer-pair overhead,
separate traced peak/live bytes/blocks, and process peak RSS before export. Domain
imports and graph/input preparation are separate. GC precedes every invocation.
Native callbacks can use multiple CPU threads, so process CPU can exceed wall time.

Sequential workers run under other host work; changed host load and native
allocation reuse prevent a speedup claim. RSS includes imports and prior calls;
tracemalloc excludes some native allocations, and live blocks are not total
allocations. Neither measure is a complete UMA or 8-GB-system admission. No
renderer, native architecture, cache/persistence, request or quality contract changes.
The existing [recipe pilot](recipe_graph.md) retains its established dependency/
capability, reservation and domain-validation contracts. Current numbers, precise
scope and terminal results belong to the #1229 delivery/PR.
