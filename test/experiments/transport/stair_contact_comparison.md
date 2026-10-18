# Shared affine contact extrema (#706)

The [86-source Wiener specimen](stair_closed_ground_comparison.md) retains its
geometry while the whole contact audit avoids repeating the same surface overlay.
The old adapter evaluates minimum(B−A) and minimum(A−B) separately. The new adapter
requests minimum(B−A) and maximum(B−A) in one traversal, then takes the larger
absolute value. Coverage, Double contact and the derived Float32 envelope are
still checked separately by `Reference.check`; no quality or work limit changes.
This is a Python verification improvement, not native/runtime or world acceptance.

## Method and limits

On a positive-area intersection of two affine triangle surfaces, the height
difference is affine. Its extrema occur at intersection vertices, including new
clipping vertices absent from either original mesh. Negating that difference
exchanges its minimum and maximum mathematically. The existing GEOS overlay,
plane evaluation and finite-gap checks supply both extrema; there is no sampling
replacement, domain shortcut, omitted component or new geometry dependency.

`geometry.vertical_clearance(..., include_maximum=True)` adds
`maximum_clearance_m`, `maximum_witness_xy` and `maximum_face_pair` only after a
complete successful traversal. The default result schema and minimum are unchanged.
The option requires a Boolean. Time, face and candidate-pair caps and failure
reasons remain unchanged. Partial failure supplies neither numeric extremum.
A valid range certifies neither coverage nor collision/solid validity.

SIGGRAPH-first background: Cherchi et al., [Interactive and Robust Mesh Booleans,
2022](../../../doc/references/vegetation/siggraph/2022-interactive-robust-mesh-booleans.pdf),
page 2/Figures 2–3, text and figures inspected locally. Floating intersection
arithmetic needs separate scrutiny; mathematical symmetry alone does not prove
bit-identical forward/reverse GEOS clipping. This change is an elementary affine
extremum derivation and reuse of the existing kernel, not that paper's algorithm.
No foreign source code or SDK dependency is introduced.

## Correctness and unchanged image

Code freeze: `0958494da4bae6db28e94eb74950582ccdbae064` (implementation
`b90621bc0`). Source and separately installed wheel each pass **188 controls**,
including analytic signed extrema/new intersection witnesses, holes/disconnected
parts, both Float32 and Double, thin positive overlap, edge-only/disjoint contact,
nonfinite gaps, partial pair-budget failure, existing zero-time/face budgets,
whole contact coverage and moved-cap/Float32-collapse counterexamples.
There are 119 matching reference imports per control/actual-consumer mode.

Fresh 85/86 generation plus independent cold readers in both modes reproduce all
seven canonical artifacts byte-for-byte. All contact errors/envelopes in the old
and fresh producer proofs are exactly equal; only three pair-count fields halve
and measured durations change. Across the measured 816/1516 Double-or-Float32
contact-error calls, every error agrees exactly with the previous two-direction
adapter and every candidate count is halved. All eight timed/profiled trials per
case produce the same module/layout/field hash as the canonical product.
This is evidence for these inputs and controls, not a universal exact-arithmetic
or forward/reverse floating-point equivalence proof.

Fresh actual source/installed readers accept all 7131 physical/logical volumes,
124 old pier contacts, 65 actual building bodies/reservations and four whole plants.
Their accepted product receipts remain untouched; disposable volume clones omit
the old receipt. Original source/frame inputs and all existing assets stay unchanged.
The existing two whole 1280×720 3D perspectives were opened again. All seven render
input artifacts remain exact, so these images remain applicable without rendering
or publishing duplicate PNGs. Sparse coverage, weak contrast and coarse terrain
remain visible; no visual improvement is claimed. Only master publishes images.

## Equal-input costs

Original PBF/DEM/frame and the 84→85→86 input chain are pinned in the linked
closed-ground report. Both alternatives run on the same preloaded immutable inputs.
Three unprofiled trials per variant/case alternate old,new,new,old,old,new;
imports, source/resident IO, GC, result hashing and report export are excluded.
The same small contact-observer wrapper/list is measured in both alternatives.
Separate subsequent Memray traces include Python/native allocations, without
native call stacks. Requested bytes are neither live RAM nor copied bytes.

The shared WSL2 host is an i5-1135G7 with four exposed logical CPUs and
16,190,028 KiB RAM. Measurement workers use normal priority; the focused
qualification runs finished before this benchmark. Other agents/host activity
remain uncontrolled; raw before-load averages are retained. This is a trend
comparison with identical work, not an isolated device timing acceptance.
Python 3.13.5, NumPy 2.2.4, SciPy 1.18.1, Shapely 2.2.0/GEOS 3.14.1,
Clarabel 0.11.1 and Memray 1.20.0. Controls/actual consumers use the existing
Buildings environment with NumPy 2.5.4; generation uses 2.2.4 to preserve pins.

| Case / variant | Median unprofiled s (min–max) | Profiled s | Allocations | Requested B | Incremental tracked peak B |
|---|---:|---:|---:|---:|---:|
| 85 / two-directions | 4.142479 (4.032258–4.302347) | 11.698598 | 15,830,568 | 2,066,501,302 | 2,017,612 |
| 85 / range | 3.267434 (3.137938–3.676075) | 8.551951 | 12,508,394 | 1,709,728,859 | 1,992,249 |
| 86 / two-directions | 8.877314 (8.734161–9.018676) | 17.182128 | 27,390,566 | 3,642,727,253 | 3,656,764 |
| 86 / range | 6.808099 (6.737172–6.883972) | 12.716238 | 21,154,424 | 2,978,861,047 | 3,656,688 |

The 85/86 whole local-step median falls by 21.12%/23.31%; these are not frame-time
savings. Positive allocation counts fall by 20.99%/22.77% and cumulative requested
bytes by 17.26%/18.22%. The tracked heap peak barely changes; no whole-process
RAM saving is established. Contact traversals/search trees halve from 1632→816
and 3032→1516; input-surface plane preparations halve from 3264→1632 and
6064→3032. Actual candidate intersections halve from 30112→15056 and 56726→28363.
Whole old pier checks remain 124. Unchanged-world contacts, domain unions and
other field checks still allocate substantially; native bounded planning,
regional products/LOD and device budgets remain open. Measured work reduction
must not be mistaken for a complete optimized transport generator.

All four traces and official stats commands completed with terminal status 0;
metadata end times, Python-allocation mode, positive-allocation histogram and
allocator totals were checked. Tracked peaks are incremental new heap after
preload, not the whole live process. The benchmark is one long-lived process;
its recorded lifetime RSS high-water marks cannot compare per-variant peaks.
Total copied bytes/bandwidth and native device time remain unmeasured. The old
64,736/77,880 B local road working arrays and all retained asset bytes are unchanged.

## Reproduction and evidence

The reusable `stair_contact_cost.py` accepts the same explicit resident, prior
module, fixture and baseline inputs as `stair_local_cost.py`, plus
`--variant two-directions|range`. Outputs and optional Memray traces must be fresh
explicit paths under `/tmp`; the worker has 180 CPU-s/3 GiB address-space limits.
For example, using the frozen input paths from the closed-ground report:

```sh
PYTHONPATH=lib/src:test/experiments/transport "$TRANSPORT_PY" \
  test/experiments/transport/stair_contact_cost.py \
  --resident "$TRANSPORT_RESIDENT78" \
  --prior-module "$TRANSPORT_MODULE79" \
  --prior-module "$TRANSPORT_MODULE82" \
  --prior-module "$TRANSPORT_MODULE84" \
  --prior-module "$TRANSPORT_MODULE85" \
  --baseline "$TRANSPORT_REFERENCE85" \
  --fixture test/experiments/transport/fixtures/stair-seed-roundabout-footway-vienna.json \
  --variant range --output "$TRANSPORT_COST_OUTPUT"
```

Private root: `/tmp/outshine-transport-cost-2758f5198/`. It retains original
pstats, all 16 run reports and four raw allocation traces/stats, terminal command
records, original input paths, canonical proof differences, wheel, import checks,
fresh generation/reader products and actual-consumer receipts. Older canonical
inputs/images remain under their documented roots; no receipt/pin is overwritten.
Wheel SHA256: `e9e8b20e4c27e3bc1397d3e989ed2cbc2da7d255a3e2e360f2d1afefe75d29b6`.
Ruff/format, function-size check, diff check and docs lint pass. Python/native
ownership and shared spatial contracts are unchanged. #434/#568/#48 and the
worldwide transport goal remain active.
