# Bounded original-source proposals

`outshine_ref.transport.frontiers.discover_source_modules(records, roots, *,
max_sources=8, max_proposals=8, max_visits=2000000, method="index")` proposes
alternative connected source modules. #537 owns the contract and experiment.
It generates no mesh and imports no experiment or SDK.

Each record supplies a unique `source_id` (1–512 characters), an original `nodes`
sequence of signed 64-bit integer pairs, `length_m` (finite 0–1e9), boolean
`eligible` and `reason`. Eligible records have an empty reason; excluded records
have a nonempty reason of at most 2048 characters. The caller preserves original
attributes and establishes frame, node identity and semantic eligibility. MVT
coordinate coincidence is only a provider-topology hypothesis, not proof of an
original OSM node identity. Different levels must be excluded before this call.

Roots have unique `root_id` (1–1024 characters) and a `node`. The root must meet
a new source endpoint; subsequent connections require at least one endpoint.
Two interior vertices alone do not connect in this operation family. There are
no distance, intersection or nearest-neighbour merges. The actual old root
profile and all physical connections require separate whole-geometry checks.

Input limits are 20000 sources, 200000 original node occurrences, 2–4096 nodes
per source and 64 roots. Empty inputs are valid. Limits are explicit integers:
1–8 sources per module, 1–8 roots attempted, 1–2000000 incidence tests per root.
Roots are attempted in identity order, before proposals are ranked by total
source length, source count and root identity. A heap grows each connected
module greedily by descending source length and identity tie-break. This is
neither global optimisation nor guaranteed full coverage. Proposals can overlap;
the caller admits one or resolves overlap before accepting several.

`method="index"` builds one inverted explicit-node index; `"scan"` searches
all eligible original node occurrences on every expansion. They select the same
sources/connections when neither exhausts its work budget. Budget exhaustion
can yield different valid connected prefixes. `stop_reason` names source/work
budget or exhausted frontier. Unattempted roots and every input source have
explicit output dispositions. `proposed_pending_geometry` is a proposal, not an
acceptance; the caller records physical admission or rejection separately.

The kernel copies/normalises input records and returns newly owned result
containers; it never modifies inputs. `node_lookups`, `heap_pushes`, index
nodes/entries and original occurrence count are explicit. Indexed incidence
visits and naive node comparisons are different units, not equal-cost CPU
instructions. Counts exclude input validation/index construction; end-to-end
timings must include both. Source limits bound heap/index allocation; no
unbounded persistent cache is introduced.

`connected_prefixes(proposal)` (#539) returns immutable tuples of whole original
source IDs in the discovery order: full count, floor(count/2), repeatedly down
to one. At most eight sources produce at most four alternatives. It requires a
unique bounded list, matching explicit parent/node witnesses, parents preceding
their children and all initial branches meeting the same original node. This
proves a rooted tree-order witness, not actual source/node/level or geometric
truth. Empty discovery gives no alternatives; cycles can have a valid tree
witness. Inputs are unchanged and output tuples own their identity sequence.

Every prefix stays connected to the same root. It never shortens an original
source or raises any physical admission budget. A caller with three attempts
can test only the first three alternatives; for eight sources this means
8/4/2, with the one-source alternative still untested if all fail. Halving can
skip a larger valid subset, so no maximality or optimality follows. A failed
joint module does not prove any individual source impossible: preserve omitted
sources, original eligibility and actual failed-attempt reasons for later work.
The optional experiment `--strategy prefix` rebuilds all secondary contact and
profile declarations after each source choice, with the same whole-geometry
gates. Default `--strategy whole` retains the complete-proposal comparison.
Exact physical evidence and limits: [prefix experiment](../test/experiments/transport/stair_prefix_comparison.md).

The real-input comparison is `stair_frontier_compare.py`; automatic geometry
admission and independent replay are `stair_frontier_experiment.py` and
`stair_frontier_verify.py` under `test/experiments/transport/`. All outputs need
explicit fresh system-temp paths. The latter preserve frozen parent assets and
bound whole-source attempts to three, retaining the valid baseline if none pass.
See [the experiment](../test/experiments/transport/stair_frontier_comparison.md)
for exact provenance, physical gates, counters, images and remaining limits.

SIGGRAPH starting point: Chen et al., *Interactive Procedural Street Modeling*
(2008), page 5, figures 7–8 and section 6.1, inspected visually. Its graph/seed
discussion informs separating topology from geometry; this discrete bounded
original-source operation does not implement its tensor-field algorithm.
