Type: bug
State: open
Parent: 1575
Area: render
Tags: measured, picture, regression

# The shadow frustum and the casters it is built for stand in ONE space

Measured on the reference drive at HEAD, with the light matrix's own output printed:

    the shadow radius it stood on        2.63727 m     (the car's extent -- correct)
    its centre, east / up                -0.0811 / 523.038 m
    the residency anchor, east / up      6.37814e+06 / 0 m
    the first cast batch, in LIGHT CLIP  x -171.381   y 213.871   z 15.2579

The clip volume is [-1,1] in x and y and [0,1] in z. The first caster lands **171 units** outside
in x and **214** in y -- times the 2.637 m radius, that is 452 m along the light's right and
**564 m** along its up. The car is four metres long.

So the frustum is built for the car, correctly, and the casters arrive half a kilometre away from
it. Nothing lands inside and the atlas comes back empty.

## What it was before, and why the number moved

board:1921 closed with `least 0.000, most 0.618, 779086 texels` on a subject that was the car
ALONE. The ground ring joined the subject afterwards (board:1890), and with it the residency's
anchor and the placements changed. Two readings since:

- with the ring casting: every one of 4194304 texels at depth **1** -- an 815 m ring drawn into a
  2.6 m frustum, every fragment beyond the near plane and clamped
- with only the carried parts casting (`CastsBelow`, this session): **0** texels

Both are wrong and they are the same defect seen from two sides: what the frustum is centred on
and what the casters' translations are do not stand in one space. 564 m is close to the ring's own
relief span (489.789 m lowest vertex against a car at 522.802 m) which points at an ALTITUDE
datum, but that is a lead and not a finding -- the number that will settle it is the light-space
coordinate of a caster whose world position is independently known.

## What will be true

- [ ] `Live::PlacedBounds`, `SubjectResidency::AnchorM` and `DrawBatch`'s placement translation
      are stated to be in ONE named space, and the light frustum is built in that space.
- [ ] Proving case: a subject at a known world position casts into an atlas whose written texels
      lie where the closed form puts them, and a caster moved by a known distance moves the
      written region by the corresponding number of texels. Negative control: the anchor added on
      one side only, and the region leaves the atlas.
- [ ] `CastsBelow` stays or goes with a reason: an 815 m ring must not cast into a frustum sized
      for a car, but the answer may be cascades (board:1926) rather than a filter.
