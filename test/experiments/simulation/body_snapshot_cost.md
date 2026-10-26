# Native body publication

Owner [#1318](https://github.com/Weichwolf/outshine/issues/1318).
World data/reader contract: [#1231](https://github.com/Weichwolf/outshine/issues/1231).
This compares active physical poses, not static world assets or complete WorldSnapshots.

`body_snapshot_cost.py` checks compact full copies against independent immutable
pages under identical edits. It retains an old reader, verifies both publications,
and reports copied storage bytes, payload blocks and directory slots. Python
runtime includes interpreter/NumPy overhead; it is not native A18/Iris timing.

Each modeled record is24 UInt64 words =192bytes, matching the current native body
storage including padding. Pages have64 records, the width of the native dirty
mask. Sparse edits can copy fewer records; dense edits copy the same payload as
a compact copy and pay more allocation/directory work. Neither wins universally.

The shrinking-suffix adversary rejects a tempting shortcut: sharing one large
allocation across updated pages can retain unused suffixes in later snapshots.
Independent page backing keeps one snapshot's logical payload at N×192bytes;
coalesced suffix backing can retain P×(P+1)/2 pages for only P logical pages.
This excludes metadata, allocator overhead and other held snapshots.

Native integration uses independent page lifetimes, shares unchanged pages and
reuses the whole publication for unchanged state. The final partial page stores
only its live rows. Registry/row-layout changes rebuild the native lookup and
publication; old readers keep their original identities. No asset-cache format,
LOD-error rule or global WorldEpoch follows from a body revision.

```sh
OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1 python -B -W error \
  test/experiments/simulation/body_snapshot_cost.py \
  --output /tmp/outshine-body-publication.json
```

Native correctness and publication costs are separate CTest cases under
`test/outshine/src/world/entity/BodySnapshot/`. Device timings, adversaries and
tradeoffs belong in the delivery issue; copied-byte counts are storage work,
not measured memory-controller traffic or a complete application RAM budget.
