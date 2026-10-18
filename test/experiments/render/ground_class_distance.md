# Ground-class segment distance: rejected root reduction

Owner: [#515](https://github.com/Weichwolf/outshine/issues/515).
Input and capture method: [ground_class_work.md](ground_class_work.md).

The existing classifier takes a square root for each segment before minimizing
the distances. The alternative minimizes squared distances and takes one root
per seed. The empty-seed sentinel remains `1e30`; squared minima start at infinity.
No product shader changes are admitted by this experiment.

## CPU control

`ground_class_distance.py` evaluates every original reachable segment at each
captured cell centre with Float32 arithmetic. Explicit expansion limits reject
oversized inputs instead of sampling or truncating them. Degenerate segments,
empty seeds and repeated range references have independent controls in
`ground_class_distance_checks.py`.

Captured input SHA-256:
`e40008b3702bbf767b2bc9184b37077770d3e89404cc5309d051c708715925e0`.

| Grid | Cell centres | Original roots | Alternative roots | Distance bits |
|---|---:|---:|---:|---|
| Fine | 16,384 | 43,814 | 17,409 | Identical |
| Coarse | 65,536 | 309,549 | 79,736 | Identical |

This is a cell-centre arithmetic control, not a pixel-weighted workload or a
proof about all GPU inputs. Fewer source-level roots do not establish a speedup.

## Native comparison

`native_ground_class_distance.py` builds private fragment artifacts and runs
unchanged native client `76f6d3dbf` in fresh offline processes. Körbersee,
1280×720/30, exactly 30 measured frames and a complete 360° rotation. Camera,
content, LOD, normal reconstruction and all other shader artifacts are retained.
Order: A1/B1/B2/A2. A concurrent nice-10 full lint completed during this work;
the observations are not isolated device admission measurements.

| Capture | GPU mean, ms | GPU maximum, ms |
|---|---:|---:|
| A1 original | 18.010 | 47.492 |
| B1 alternative | 18.280 | 37.106 |
| B2 alternative | 21.491 | 38.396 |
| A2 original | 16.667 | 36.789 |

All four PNGs are byte-identical, SHA-256:
`076343f482591dc796031d61805f847a2b728f0a32fc45b0a17ebf8d003c895e`.
Mean of the two A captures: `(18.010 + 16.667) / 2 = 17.338 ms`;
B: `(18.280 + 21.491) / 2 = 19.885 ms`. No runtime gain is demonstrated.
Reject the alternative; do not change the production shader or infer a universal
slowdown from this single noisy comparison.

Private evidence: `/tmp/outshine-ground-distance-ab36fd4c1/receipt.json` and
`/tmp/outshine-ground-class-distance-ab36fd4c1-cpu.json`. The driver records
commands, binary hash, complete frame rows, GPU rows and image hashes.

```sh
python3 test/experiments/render/ground_class_distance_checks.py
python3 test/experiments/render/ground_class_distance.py /tmp/capture/classes.u32
python3 test/experiments/render/native_ground_class_distance.py \
  --native-root "$native_checkout" --output /tmp/outshine-ground-distance-run
```
