Type: bug
Area: actor
Tags: robustness

**The fit's simplify recursion is bounded**

`KeepBetween` (src/actor/path/Fit.cpp:32-48) is Douglas–Peucker by naked recursion: each
level splits at the worst vertex and recurses BOTH sides. Worst-case depth is O(points) —
a route whose deviation grows monotonically (a long sweeping spiral of legs) peels one vertex
per level. The input is `CorridorLay` feeding route legs (src/sim/CorridorLay.cpp:55), and a
route may carry up to `kMaxRouteLegs = 262144` legs (Wayfinding.h:13) — a quarter-million
stack frames is a stack overflow, which is a crash and not a refusal.

The tree's own rule is refusal at assembly over runtime faults and bounded terms: either an
explicit stack (a vector of (from,to) ranges, capacity opened once, the standard DP form) or
a proven depth bound with a loud refusal past it. The proof is a fixture that hands Simplify
a monotone-deviation polyline of kMaxRouteLegs vertices and returns instead of faulting.
