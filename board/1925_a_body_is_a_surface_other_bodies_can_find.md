Type: feature
State: open
Area: sim, actor
Tags: traffic, perception

# A body's occupied space is a surface any other body can find

A car reacts to what is under it and to what is around it, and the second half does not exist.
`Sim::Underfoot::At(lat, lon)` answers from the world's own ground: terrain height and the
surface class at that point. Nothing a second body occupies is in that answer, so:

- two vehicles pass through each other
- a kerb, a barrier and a parked car are all absent for the same reason: none of them is ground
  and none of them is a body the query knows about
- a mind cannot perceive traffic, so it cannot yield, follow or overtake

CLAUDE.md's actor chain already names the seam: a controller PERCEIVES through *bounded spatial
queries: bounds · ground · sight*. `Underfoot` is the ground half. The bounds half has no
implementation and no caller.

`src/world/generators/OccupancySink` and `Generators::Body` (`BodyId`, `Em`, `Nm`, `RadiusM`,
`HeightM`, `Contact`) already model an occupied volume for generated content, and
`ContactMaterial` on it is an empty enum -- a placeholder with no table behind it. Whether that
is the right store for moving bodies is the first question this item has to answer, not assume.

## What will be true

- [ ] A body's occupied volume is queryable by another body, bounded, without a search over all
      bodies.
- [ ] `Underfoot` composes it: a wheel over a second body's roof stands on the roof.
- [ ] A mind can ask what is ahead of it within a distance and get bodies back, so following and
      yielding are expressible.
- [ ] Proving case: two declared vehicles on one corridor, the following one asked to hold a gap,
      and the gap holds without either passing through the other. Negative control: the occupancy
      query removed, and they interpenetrate.
