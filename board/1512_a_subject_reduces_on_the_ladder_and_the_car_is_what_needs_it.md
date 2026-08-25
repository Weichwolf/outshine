Type: bug
State: open
Area: render
Tags: perf, layering

# Geometry carries its own ladder, and the pool that holds it is a slot table and a ring

`ClusterDag` — clusters, an edge-collapse simplifier over a cost heap, and `DagSelect` choosing
by projected pixel error — is reached by `src/ground/TilePool` and by nothing else. A glTF
subject has no ladder, a generated part has none, and the car that most needs one has none.

**The defect is not that the subject path misses out; it is that a NOUN owns a mechanism.**
Terrain is one generator's output among many, and it owns *quantise*, one of the four verbs the
decomposition gives the compositor. The engine must not know what terrain IS.

The same class holds the second defect: `class TilePool {` (src/world/ground/TilePool.h:35) keeps 3
`std::mutex`, a `std::condition_variable`, a `std::map` and a `std::set` of pointer-chasing
nodes where a slot table and a ring would do — RAGE's reference is a decisionless pool, and a
decisionless pool holds no tree.

## What will be true

- [ ] Geometry carries a DAG whoever produced it — tile, car, tree, building, bridge deck,
      subject — reached through ONE mechanism named for what it does.
- [ ] The key is `(kind, params, seed, rung)` as a value, quantised to the global ladder before
      it becomes a key; no strings, no distance ratio.
- [ ] The pool is contiguous and pointer-free: a slot table, a ring, one lock at the seam and
      none on the frame path.
- [ ] A grep proves it: no layer named for a noun owns a verb.
