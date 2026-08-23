Type: issue
Area: actor
Tags: performance, pathfinding

**The cell index the weave builds answers Nearest and Within**

`Network::Weave` builds a spatial hash and stores it — `Cells_ = std::move(byCell)`
(src/actor/path/Wayfinding.cpp:199) — and then NOTHING reads it: the only references to
`Cells_` in the tree are the clear (line 71) and the move (line 199).

Meanwhile `Nearest` (Wayfinding.cpp:212-226) and `Within` (228-235) linear-scan every node
with a haversine each — `kMaxNetworkPoints = 4000000` (Wayfinding.h:12) — and `Plan` runs
Nearest twice plus Within once per route. That is up to ~12M sin/cos/asin per plan for an
answer the stored index gives in a ring walk, on the planner path 1503 wants request-shaped.

Either Nearest/Within walk `Cells_` (growing the ring until a hit, the same neighbourhood
walk Weave already does at lines 146-158), or `Cells_` is dead state and goes. Carrying a
built index that nothing queries is the worst of both: the memory of the fast path, the cost
of the slow one.
