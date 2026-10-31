# Recipe catalogue capacity

Architecture discussion: [#483](https://github.com/Weichwolf/outshine/issues/483).
`recipe_catalog_loading.py` compares indexed SQLite loading with full JSON decoding.
It does not implement a recipe format, grammar evaluator or regional selection policy.

```sh
python3 test/experiments/assets/recipe_catalog_loading.py "${TMPDIR:-/tmp}/outshine-recipe-capacity"
```

The experiment builds 10k, 100k and 1M small synthetic descriptors, each with two
illustrative operators. Every consumer requests the same 64 IDs and resolves two
shared dependencies. Each strategy runs three times in fresh subprocesses; output
hashes must agree. The closure has an explicit entry budget. SQLite stores a
family/region index, but this experiment queries IDs only; contextual selection
and actual domain recipes remain untested.

`report.json` records times, decoded entries, file sizes and process peak RSS.
The OS file cache is uncontrolled; these are not cold-SSD measurements or total
system-memory figures. Construction time includes exporting both representations.
Generated catalogues are deleted after each successful size; reports remain in
the explicit output directory. Failed inputs remain available for diagnosis.

On 2026-10-16, macOS/Python 3.9, three fresh consumers per strategy: for 1M entries,
median load/lookup was approximately 2 ms for SQLite versus 2185 ms for full JSON;
peak RSS approximately 13.6 versus 1181 MiB. Both resolved the same 66 descriptors.
The descriptors average about 114 bytes; this does not predict real recipe sizes
or geometry generation costs. Real consumers, dependency failures/versioning and
bounded residency are required before selecting the production contract.

The isolated typed recipe graph and real source-hall consumer are documented in
[recipe_graph.md](recipe_graph.md) (#494). It uses two small actual definitions
and keeps trusted domain solvers behind typed ports. This does not replace the
capacity/index experiment or admit the shared/native/cache contract in #483.

The real young-pine/planar-root consumer uses that unchanged kernel:
[Vegetation recipe pilot](../vegetation/recipe_plant.md) (#504). It keeps one
species definition separate from seeds and retained habitat instances; physical
and precision negatives, prior render-input equivalence and scoped costs are
part of the comparison. The common production decision remains open in #483.

The Buildings consumer also has an explicit [glazed hall v2](../buildings/recipe_rooflights.md)
with separately requested logical fields, closed panels and a versioned cut shell.
It leaves the shared kernel and v1 definition unchanged (#500).

## Cache reclamation (#1920)

`cache_compaction.py --output /tmp/outshine-master-cache-compaction.json` compares
NONE/FULL/INCREMENTAL auto-vacuum on the same 24 synthetic 512-KiB packages.
It removes two thirds, checks identical retained bytes and SQLite integrity, and
checks that a pinned WAL reader defers reclamation without changing its snapshot.
Incremental work releases at most 256 free pages per step, with a checkpoint
between steps. Reports include file/WAL/SHM bytes, allocated blocks and timings;
temporary databases are removed. This measures the storage primitive, not the
automatic cache limit, eviction selection, migration or concurrent admission.
The same script demonstrates that `max_page_count` alone cannot cap file usage:
eight fixed-size updates keep the main database page count unchanged while a
pinned reader forces WAL retention beyond three times the initial file usage.
This is a SQLite primitive counterexample, not the immutable asset protocol.
New publication needs admission, WAL reserve and reader-aware backpressure.
