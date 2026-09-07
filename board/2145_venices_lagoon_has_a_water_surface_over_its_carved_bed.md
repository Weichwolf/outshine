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
