Type: bug
Area: actor
Tags: hygiene, recurrence

**Plan's withinM is read or gone**

The same defect class as 1704 (closed this round: DriveTick hauled a Vehicle it never read),
recurring one layer down, so it is re-filed harder: the signature lies to every caller.

- `src/actor/path/Wayfinding.h:58` declares `Plan(from, to, tightestM, withinM)`;
  `src/actor/path/Wayfinding.cpp:237-238` — `withinM` appears in the parameter list and
  NOWHERE in the body. `grep -n withinM Wayfinding.cpp` returns line 238 alone.
- Callers pay real values into it: `src/sim/DriveAssembly.cpp:219`,
  `test/unit/actor/path/ANetworkIsWovenFromWaysThatShareNoIdentity.cpp:100,135` (`10.0`),
  `ARouteIsAFunctionOfTheWaysAndNotTheirArrival.cpp:62`. Every one of those numbers does
  nothing.

Either the parameter carries a meaning (a search corridor bound? then it must bound the
search and a test proves a node outside it is never expanded) or it goes from the signature
and all call sites in one commit, the 1704 way. A parameter that survives two reviews unread
earns a claims-gate discussion: the tick-product gate (1703) shows the shape.
