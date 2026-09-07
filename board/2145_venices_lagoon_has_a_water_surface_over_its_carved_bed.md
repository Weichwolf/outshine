Type: bug
State: open
Area: engine, world
Tags: measured, owner

# Venice's lagoon has a water surface over its carved bed

**Benchmark** -- Unreal's `WaterBodyOcean`/`WaterBodyLake` carve the landscape AND stand a
water mesh at the declared level; RAGE's water is a plane per body over a carved bed. **Both
agree** a water body is two things, a bed and a lid, and neither draws one without the other.

## Where it stands, seen 2026-09-05

`build/shots/reference/Venice-96bbca8d.png`: the lagoon at the lower left shows the CARVED
BED (the basin yield of board:2115 pressed it, batter walls and all) and no lid; the sea in
the distance has its plane. So the body reached the press as a `Stamp::Basin` yield
(`Laying.cpp`, `WaterBodies().Surfaces()`) and did not reach the lid pass (`Laying.cpp`, the
water surfaces after the press) -- one of the two walks refuses it: the lid's
`last > points.size()` guard, a ring too large for its buffer, or a multipolygon whose outer
ring the lid pass never sees while the basin pass does.

## The solution

Publish per body which of the two passes took it and why not (`water: bodies carved`,
`water: bodies lidded`, `water: bodies refused a lid, by reason`); the difference is the
defect and names its own repair. A body carved and not lidded is then a case with a red
oracle, not a picture somebody notices.

## What will be true

- [ ] Venice's reference shows water over the lagoon's bed; the digest moves with its count,
      its window and the picture in the item
- [ ] `water: bodies carved` equals `water: bodies lidded` at every reference place
- [ ] Negative control: the lid pass switched off makes the two counts differ by every body

## The webcam corpus shows the same defect on two more waters, 2026-09-07

Measured against the photographs, looked at in the frames:

- **Husum**: the harbour basin is a STAIRCASE. The surface follows the DEM's quantised bed in
  shelves with hard risers instead of standing flat at one level, and the quay beside it is a
  stepped ramp rather than a wall with a face. In the photograph the basin is one plane with the
  town reflected in it
- **Malcesine**: where Lake Garda meets the far shore the waterline is a vertical SAW-TOOTH comb,
  a row of dark stripes the full height of the bank. The lake's surface and the terrain are cutting
  each other rather than one lidding the other

Both are the same rule this item states -- a body of water is a SURFACE at one level over a bed --
and both are visible from a camera position a photograph can be laid against, which Venice's
overhead view could not show. Whatever fixes the lagoon is checked at these two.
