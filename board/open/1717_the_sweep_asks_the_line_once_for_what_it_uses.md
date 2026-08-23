Type: bug
Area: actor
Tags: hygiene

**The sweep asks the line once for what it uses**

`Sweep` (src/actor/path/Ribbon.cpp):

- Lines 80-88 and 89-98: the two per-station loops call
  `StandAt(along, atM, acrossAt[which], 0.0)` with IDENTICAL arguments — every station pays
  `2 * kRibbonAcross` resections (binary search + trig each) where `kRibbonAcross` would do.
  Batch over per-item is the house rule even off the frame path; here the doubling buys
  nothing.
- Lines 152-157: a loop that calls `along.At(atM, on)` and never reads `on` — the call is a
  validity gate for stations the loop at line 71 already validated (it returns an error on
  the first failure), so the whole loop reduces to
  `TopAreaM2 = (stations-1) * stepM * (acrossAt[3] - acrossAt[0])`. Dead work plus a silent
  `continue` that could under-count area on a path that cannot occur.

Also `out.Index` is the one buffer never `reserve`d (lines 106-119, 145-147) while its three
siblings are (55-57); the count is exactly derivable from stations and kRibbonAcross.
