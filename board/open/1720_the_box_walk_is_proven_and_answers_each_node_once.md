Type: bug
Area: actor
Tags: tests, correctness

**The box walk is proven and answers each node once**

1711 closed on a proof that never runs the code it claims to prove. Both `Within` CHECKs in
test/unit/actor/path/ATurnRefusalBelongsToTheApproachNotTheNode.cpp:80-95 use SnapM = 2 m on
a six-node net: at reach 650 m the box is 653² = 426409 cells against ~6 occupied, so
`cells > Cells_.size()` (src/actor/path/Wayfinding.cpp:245) routes BOTH checks into the
linear fallback scan. The box walk — the arm every real network takes — has no unit proof.

And it has a defect the proof would catch: within a poleward row `lonCell(rowLat)` exceeds
the query's stride `lonCell(of.LatDeg)`, consecutive columns floor to the SAME key, and the
walk pushes that cell's candidates AGAIN — `Within` returns duplicate node indices, a
`.size()` over them lies, and `Plan` seeds duplicate virtual states whose count it publishes
as `StartedFrom` (Wayfinding.cpp:294).

Demanded: the walk dedupes at the key (skip a key already visited this query — the visited
ring is small and bounded by the box), a unit arm builds a net dense enough that
`cells <= Cells_.size()` and proves box-walk results equal the scan's set exactly (no
duplicate, no miss, boundary node included), and `StartedFrom` counts distinct seeds.
