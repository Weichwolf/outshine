# Geometry buffer growth

Real A18 Pro/Metal Grow/Cross requests compare capacity policy before native changes.
No shader, mesh, quality, cache or backend-format change. Fixed inputs retain every
request and its observed pre-state; owner addresses alone become stable small IDs.
The manifest pins compressed/decoded bytes and actual final native buffer capacities.

```sh
python3 -B test/experiments/render/buffer_growth/checks.py
python3 -B test/experiments/render/buffer_growth/replay.py \
  test/experiments/render/buffer_growth/fixtures/Tokyo.csv.gz \
  --output "${TMPDIR:-/tmp}/outshine-buffer-growth.json"
```

`doubling` reproduces the original repeated-power loop and verifies every recorded
capacity, use mask, presence and copy counter. `doubling_max` reserves
`max(required, 2 * old)` with the same uint32 saturation, retaining a sufficient old
capacity on usage changes. This is also the installed libc++ `vector::__recommend`
strategy; the C++ standard does not prescribe a vector growth factor.

The baseline preserves unaligned byte requests exactly. Only the fractional
controls use four-byte alignment. Other controls use factors3/2 and5/4, or exact demand.
All policies replay **all** requests, including those where the original did not grow.
Cross replacements clear/replace the specified stream, rather than pretending every
allocation goes through Grow. Deferred/fence retirement is deliberately not modeled.
Results are buffer capacities, allocation attempts/requested bytes and copy bytes;
they are **not** CPU/GPU timings, physical memory peaks or a whole-engine budget pass.
Captured ordinary runs contain no failed uploads; the native admission/retry test
covers failure behavior separately. The model does not infer arbitrary rejection
outcomes from successful traces.

The six unchanged complete1280×720/60 no-vegetation diagnostic scenes all retain
less capacity with doubling_max. Tokyo799119024→590071616B and
661157160→492265300Bcopied; Wien562553680→475221732B and
411382128→354008936Bcopied. Koerbersee/Malcesine use slightly more copied bytes,
so this is no per-scene dominance/optimality claim. Tiny-factor/exact growth trades
headroom for many extra allocations/copies. The stable-usage geometric growth bound
is retained; no automatic shrink, paging, new allocator or memory manager is added.

Native paired measurements, exact calls, source versions and open performance limits
belong to [#1503](https://github.com/Weichwolf/outshine/issues/1503).
Private instrumentation, binaries, raw traces and renders stay in system temp.
