Type: bug
State: open
Parent: 1862
Area: actor
Tags: measured, graph, routing

# A loose end welds onto an edge that is still there

`Network::Weave` (src/base/spatial/Wayfinding.cpp:110) ties a degree-one node onto the edge it
ends ON. The candidate edges come from `byEdgeCell`, built once at :243 over the edge table as
it stood BEFORE any tie, and the tie then mutates that table:

```cpp
unlink(bestFrom, bestTo);            // :321 -- returns silently if the pair is not there
unlink(bestTo, bestFrom);
link(bestFrom, loose);               // :329
link(loose, bestTo);                 // :330
```

The index is never updated. Two loose ends near the same segment are the normal case at a T
junction where a carriageway pair is crossed twice, and the second one finds `(a, b)` in the
index although the first already replaced it with `(a, end1)` + `(end1, b)`. `unlink` removes
nothing — it has no failure path — and the graph gains `(a, end2)` and `(end2, b)`: a chord
that bypasses `end1`, and two ends lying on one segment that are NOT joined to each other.
That is the exact shape of the disconnection the tie was written to remove.

Measured at 817ea333 on the shipped scenario, 25 tiles around Munich:

```
loose ends tied onto an edge they end on = 2450 ends
pieces the graph falls into = 4193 pieces
pieces holding fewer than four nodes = 2455 pieces
nodes stranded in those = 5584 nodes
```

2455 pieces of fewer than four nodes, against 2450 ties, is the population where a stale
candidate is most likely and nothing in the tree says whether the two are the same nodes.

**The surgery landed unguarded.** Four commits this hour rebuilt the weld and `grep -rn
'Weave\|Wayfinding' test/` finds only `test/run.sh`'s include line and the build audit. There
is no scenario, no oracle, and every number the repair is argued from is a `printf` from one
run of one scenario.

## What will be true

- [ ] `unlink` cannot fail silently: a split that does not find its pair is a refusal, or the
      index carries a generation the tie bumps.
- [ ] Two loose ends on one segment end up joined THROUGH each other, and the count of ends
      that tied onto an already-split edge is published — zero is the number to hold.
- [ ] Proving case, invariant oracle: a declared way set in which two stubs end on one segment
      at known offsets. The oracle is plane geometry, not our shape — after the weld every stub
      reaches every other stub, and the shortest path between the two of them is the distance
      along the segment between their projections, to the digitisation tolerance.
- [ ] Negative control: drop the split (weld by point only) and the same two stubs land in two
      components, which the case reports as unreachable.
