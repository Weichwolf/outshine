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

- [ ] reproduce: sweep km 266-268, dump both edge polylines, find the step and its size in metres
- [ ] name which of the three suspects it is
- [ ] the stitched edge is continuous to within the sweep's own step length, measured over the route

## Comments

Found by the magazine-reviewer round; the same round confirmed the rest of the ribbon is free of
z-fighting and cracks, so this is a join rule, not a tessellation defect.

The reviewer's third round looked at the same station from the driver's eye and reports the right
edge CLEAN -- "road present, continuous, edge smooth" (`km0267.1-first`, 2026-08-22). Plausible
causes for the disappearance: the notch was in a previous build's sweep and one of the corridor
fixes since removed it, or the new camera angle hides it. The item stays open until the edge
polylines are dumped over km 266-268 and the step is measured absent.
