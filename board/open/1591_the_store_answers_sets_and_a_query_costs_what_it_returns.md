Type: task
Parent: 1583
Area: core
Tags: scene, perf

**The store answers sets, and a query costs what it returns**

Four operations walk the whole pool today, and the model's own navigation is one of them:

| walk | where | cost |
|---|---|---|
| `Remove` hunts owned-by-target dependents | `src/scene/Store.cpp:61` | O(n) per removal, recursive over the cascade |
| `Offering` hunts adverts | `Store.cpp:124` | O(n) per ask |
| `Instantiate` hunts prefab children | `Store.cpp:257` | O(n) per subtree node |
| `CopyOf` resolves a slot name | `Store.cpp:271` | O(n) per lookup -- "no string lookup" bought a pool walk |

The reference's mechanism is precisely what is missing: in Flecs a pair is a value BECAUSE that
makes it contiguously queryable -- reverse index (target -> sources) for the cascade and for
children, tables for iteration. Here the pairs are values and the queries are walks anyway. At a
handful of entities nothing hurts; board:1581's fold puts minds on the frame path, and a mind's
tick asks exactly `Offering`/`CopyOf`. RAGE's rule applies: the frame takes nothing it has to
search for.

What must be true:

- removal and child enumeration read a reverse index, not the pool
- iteration exists: all entities of a role, all holders of a capability tag, all pairs of a
  relation -- the queries a SYSTEM needs, contiguous, without a per-slot branch
- `Column` (`src/scene/Column.h`) is iterable beside it -- a physics step over every `Vehicle`
  is a linear pass, or the column is a lookup table and not a component store
- `Column` binds to its store once, not per call (`Column.h:23,31` accept ANY store; the wrong
  one with a coinciding generation answers wrongly and quietly)

Costs are proven the tree's way: a unit case that counts touches, not a benchmark.
