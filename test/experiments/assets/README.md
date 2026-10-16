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
