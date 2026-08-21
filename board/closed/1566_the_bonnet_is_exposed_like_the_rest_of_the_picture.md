Type: bug
Area: render
Tags: bug

**The bonnet is exposed like the rest of the picture**

Two related observations from the reviewer's third round:

- the first-person bonnet is a **blown-out white blob** with a hard black crease -- clipped to
  display white while the sky above it sits mid-tone (`km0061.4-first`, `km0267.1-first`)
- the early stations carry a **blue-violet cast** the later ones do not (`km0017.3` and
  `km0036.5` against `km0267.1`) -- one route, one declared time of day, two grades

The bonnet is white paint under a 40 000 lux sun with a specular term and a fixed exposure derived
for the mid-tones; the cast difference suggests the sky's contribution differs between stations
when nothing declared changed. Both are measurements against the SAME declared illuminance, so both
are instruments into the exposure chain rather than taste.

- [x] the bonnet reads as shaded paint with a modelled cowl edge -- the reviewer's fourth round:
      "bonnet no longer clipped to pure white", proven at km0267.1-first (2026-08-22)
- [x] the blue-violet cast is gone and the grade is consistent across stations -- same round:
      "cast gone, sky gradient plausible, sun bloom present"

## Comments

Filed from the reviewer's third round, ranked tenth of eleven.


## Comments

Closed by the skylight ambient (board:0120's irradiance term): the bonnet's white was the specular
sun over a near-zero environment, and the cast was the same imbalance read the other way. The
reviewer's fourth round confirms both from outside, unprompted.
