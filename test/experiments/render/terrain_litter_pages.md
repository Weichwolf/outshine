# Terrain litter page costs

Owner: [#1934](https://github.com/Weichwolf/outshine/issues/1934).
The R8 field is one material component, not the full static material basis.
The production opt-in remains a qualification path, not a performance improvement.

## Rejected lookup reuse

`terrain_litter_page_filter.lookup_counts` counts ordered filter-tap lookups,
including both mip levels, negative page boundaries, fade and the first miss.
One retained address per mip reduces the complete native corpus from
21,048 to 6,362 searches (21,048 / 6,362 = 3.31), holes from 16,837 to 6,055,
and empty tables from 3,084 to 3,084. These are logical operations, not timings.

Two GLSL trials retained either the slot or the physical texture origin.
Neither improved native GPU time. Register/branch costs are a hypothesis, not
an isolated cause. Both implementations were removed; the model remains to
prevent treating fewer searches as a demonstrated GPU win.

## Rejected larger admission queue

Increasing shared publication capacity from four to 32 jobs retained the same
workers, 32 tickets, 128 MiB input limit and 128 KiB receipt limit. It delivered
more pages but worsened frame time. The second 32-job run published 7,346 pages
into 1,024 slots over four rotations: publication count is not unique residency.
Sparse used-page feedback and possible working-set oversubscription need review
before raising admission. The shared production limit remains four.

## Native comparison

A18 Pro/Metal, 1280x720/60, offline Feldkirch, no vegetation, 240 frames/four
rotations; GPU p50/p99 below use the final 60 frames. No concurrent compilation.
Base: `49fa31a3b`; identical compiler flags, source/asset roots and camera.
The retained baseline binary reports `9250976ce+dirty`; relevant CPU code was
compared, not inferred from that build label. Original shader hashes match.

| Trial | GPU p50 ms | GPU p99 ms |
|---|---:|---:|
| Original, two paired controls | 22.547–22.556 | 25.274–25.317 |
| Retain slot, two trials | 22.745–22.774 | 25.331–25.677 |
| Original, origin controls | 22.504–22.571 | 25.115–25.257 |
| Retain physical origin | 22.665–22.793 | 25.520–25.725 |
| Original, admission controls | 22.489–22.572 | 25.273–25.322 |
| 32-job admission | 24.505–24.557 | 27.325–27.461 |

Raw commands, receipts and rejected patches stay in system temp; issue comments
retain the decision. These trials prove neither a saturated GPU cache nor a
complete material basis. Default rendering is unchanged. Both compared cache
configurations exceed the 16.67 ms total frame budget.

## Rejected single-cell polynomial

`ground_litter_polynomial.cell_weights` combines all nine weighted corner
interpolations into four corner coefficients when every Gauss point occupies
the same cell. It handles oblique, sheared and degenerate footprints; crossed
cells keep the original filter. The proof compares the coefficients against
independently evaluated nine-point sums, including negative coordinates.
The sixth moment is 3/1600 for this discrete three-node rule, not the uniform
integral's 1/448. This is an algebraic rewrite of that rule, not exact filtering.

A native GLSL trial passed the existing 115,200-probe material comparison for
box/four/nine-point paths with unchanged tolerances and Metal validation. The
nine-point scalar maximum residual was 2.11e-7; the complete material maximum
was 3.43e-7 against the existing Double component reference.

The full-frame trial was rejected: in two 60-frame Jura rotations, GPU p50 was
22.257–22.453 ms versus 22.078–22.159 ms in surrounding controls. Feldkirch
showed no consistent gain either. Its images changed through finite-precision
evaluation order. No new shader, binding, pass or cache is retained in production.
Inputs and native trial receipts: `/tmp/outshine-master-1934-polynomial-native/`
and `/tmp/outshine-master-1934-polynomial-pairs/` on the owner host. Four Python
tests retain the mathematical comparison, not a GPU performance guarantee.

```sh
OPENBLAS_NUM_THREADS=1 PYTHONPATH=lib/src:test/experiments/render python3 -B -m unittest test_litter_page_lookup_counts
OPENBLAS_NUM_THREADS=1 PYTHONPATH=lib/src:test/experiments/render python3 -B -m unittest test_ground_litter_polynomial
```
