Type: bug
State: open
Parent: 1890
Area: render, world
Tags: measured, picture, medium

# The ground the sky paints and the ground the compositor draws wear one atmosphere

Measured through the windscreen at 873f8f65, first person, one frame:

| | |
|---|---|
| the composed ring, 2–5 km out | **(78, 94, 109)** |
| the sky stage's ground below it | **(34, 42, 32)** |
| the sky itself above | (45, 73, 108) |

The ring is CORRECT. Its albedo is the medium's own `GroundAlbedo` and its distance is 2 to 5 km,
so the atmosphere veils it toward the sky's colour — that is aerial perspective and a photograph
of Munich does the same thing.

**The sky stage's ground does not do it.** It is one flat olive tone at every distance, because it
is painted as `GroundAlbedo` seen through a transmittance that does not vary with the ground
point's range. So the two grounds meet at a seam: real terrain fades into the haze, the painted
plane behind it does not, and the horizon shows the join.

## What will be true

- [ ] One ground, one atmosphere: whatever paints below the horizon is veiled by the same
      transmittance the composed ring is, so the two are indistinguishable where they meet.
- [ ] Proving case: the pixel where the drawn ring ends and the painted ground begins differs by
      less than the frame's own quantisation. Negative control: the painted ground unveiled, and
      the seam is the 44 counts measured above.
