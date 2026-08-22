Type: bug
Area: actor
Tags: pathfinding, determinism
Regresses: 1540

**The weave's canonical key spells the whole way, and refusal precedes the sort**

1540 closed on "a route over the same ways is the same route, to the bit". The canonical
order (src/actor/path/Wayfinding.cpp:79-88) keys on points and Count ONLY — two ways with
identical geometry and count but different attributes are comparator-EQUAL, `std::sort` is
unstable, and the node merge takes the FIRST positive Lanes (Wayfinding.cpp:167). Arrival
order still reaches the graph through that gap. Reproduced today: two coincident ways,
lanes 2 and 4, laid A,B vs B,A → `Route::Legs[0].Lanes` = 2 vs 4. Same way-SET, different
answer — the closed claim's exact shape. HalfWidthM (max, :166) and MaxGradient
(min-positive, :168-170) are order-independent; Lanes is the leak, and Lanes flows into
Leg and onward to the carriageway.

Second defect in the same block: the three refusals — no ways (:118), kMaxNetworkPoints
(:122), SnapM (:127) — run AFTER the sort and the full five-array rebuild (:76-116). An
over-bound network pays O(P log W) comparisons plus five reallocations before it is told
no. Refusal before work.

Demanded: the comparator tie-breaks on HalfWidthM, MaxGradient, Lanes after points and
Count; the refusals move above the canonicalisation block; the proving test
(test/unit/actor/path/ARouteIsAFunctionOfTheWaysAndNotTheirArrival.cpp) gains the
coincident-pair case, arrival-swapped, asserting Leg attributes to the bit.

---

Closed: the canonical order compares the WHOLE way -- points, count, then HalfWidthM,
MaxGradient, Lanes -- so geometry-equal ways that differ in an attribute are ordered by the
attribute and the merge winner is the declaration's, never std::sort's. The three refusals
moved BEFORE the sort and the five-array rebuild (refusal before work). Proving test:
ARouteIsAFunctionOfTheWaysAndNotTheirArrival now lays geometry-equal lane-2/lane-4 twins on
half the corners and CHECKs Legs' Lanes and HalfWidthM equal across both arrival orders --
negative-controlled: with the attribute keys removed from the comparator the test goes red.
10/10 with the drive suite.
