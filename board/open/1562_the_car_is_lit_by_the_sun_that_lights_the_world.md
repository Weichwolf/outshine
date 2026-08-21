Type: bug
Area: render
Tags: bug

**The car is lit by the sun that lights the world**

`km0017.3-third.png` (2026-08-22): the F31 stands on the carriageway, correctly placed and sized --
and it is a black silhouette. The ground and the carriageway around it take the key light; the car
does not.

The caveat first: the asset carries its own materials (dark paint among them), so the harmless
explanation is a dark car in a picture whose exposure is set for 40 000 lux -- ruled IN or OUT by
one measurement: the GLASS is black too, and glass at any albedo specular-reflects a 42-degree sun.
A car whose windows return nothing is unlit, not dark.

Suspects, in order: the key light reaches only declared-surface parts (the file parts carry their
own materials through a different table); the emitted-radiance path zeroes file-part shading; the
light direction is in the wrong frame for file parts (the same conjugation family board:1551 just
closed).

- [ ] measure: render the car alone under the same key light, read the body pixels' luminance
- [ ] the glass shows a specular sun
- [ ] the paint reads as paint in daylight

## Comments

Found the moment board:1551 closed -- the car became visible and visibly unlit in the same still.
