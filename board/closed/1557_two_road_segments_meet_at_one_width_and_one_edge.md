Type: bug
Area: corridor
Tags: bug

**Two road segments meet at one width and one edge**

`km0267.1-first.png` shows a hard notch in the carriageway's right edge near x=900: a step
discontinuity where two segments meet and do not agree on width or lateral position. A shipped road is
stitched with continuous width and a shared boundary vertex row -- the join is invisible.

Suspects, in order: a width change between OSM ways quantised per segment instead of blended over a
transition length; a lateral offset where two ways' centrelines meet at an angle; the sweep restarting
its cross-section frame at a way boundary.

- [x] measured: km 266-268 swept at 2 m, both outer edges, 2000 steps -- mean deviation 0.010 m,
      worst 0.050 m at km 266.446 (`tools/driver/TheRoadEdgeIsContinuousWhereSegmentsMeet.cpp`)
- [x] named: none of the three suspects stands in the geometry TODAY -- the notch was
      photographed before the session's corridor-frame fixes (board:1556's one-frame rule,
      board:1529's history) and does not reproduce in the swept polyline
- [x] the stitched edge is continuous to within a fortieth of the step length over the reviewer's
      own station, and the case is permanent

## Comments

Found by the magazine-reviewer round; the same round confirmed the rest of the ribbon is free of
z-fighting and cracks, so this is a join rule, not a tessellation defect.

The reviewer's third round looked at the same station from the driver's eye and reports the right
edge CLEAN -- "road present, continuous, edge smooth" (`km0267.1-first`, 2026-08-22). Plausible
causes for the disappearance: the notch was in a previous build's sweep and one of the corridor
fixes since removed it, or the new camera angle hides it. The item stays open until the edge
polylines are dumped over km 266-268 and the step is measured absent.


## CLOSED -- the edge is continuous by measurement, and the case outlives the photograph

Both outer shoulder edges over km 266-268: 2000 steps of 2 m, mean deviation 0.010 m, worst
0.050 m -- a fortieth of a step, where the photographed notch would stretch a step by its full
metre-class depth. The reviewer's fourth round independently reported the same station's edge
"clean" from the driver's eye. Whatever produced the round-two photograph -- most plausibly the
pre-fix relay geometry that board:1556 closed -- it is not in the swept corridor now, and the
permanent case will say so again the day it returns.
