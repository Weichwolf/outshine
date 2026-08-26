Type: feature
State: open
Parent: 1573
Area: world, generators, compositor
Tags: picture, driver, measured

# Something stands beside the road: a building, a tree, a water surface

Thirty-two stills at bb9472db, four runs, two Munich routes chosen for exactly this question:

- **Ludwigstrasse, 48.1420,11.5800 -> 48.1518,11.5820.** A boulevard walled on both sides by
  continuous five-storey blocks, the Siegestor closing the far end. 497 m driven, 8 stills,
  first person and chase. **Not one vertical object in any of them.** The horizon is unbroken
  ground meeting unbroken sky across the full 1280 px.
- **The Isar, 48.1310,11.5820 -> 48.1290,11.5930.** The route crosses the river. 441 m driven,
  8 stills each view. **No water surface at any point**, and no bank, no parapet, no bridge
  structure. The run's own reader prints `CARRIES ways the data marks as a bridge = 76 ways`
  in this extract, so the data is there and nothing stands it up.

The consequence is measurable, not aesthetic: between chase stills 05 and 06 of the city route
only **3.8 %** of pixels differ, over 60 m at 90 km/h. A picture with a world in it cannot hold
96 % of its pixels still while the camera moves a car length per frame -- there is nothing to
have parallax against. That number is the definition of a road in a void.

## What will be true

- [ ] A building generator stands OSM footprints along the corridor at a declared density, with
      a height from the data or from the class, and the driver's still shows a street with two
      walls.
- [ ] Vegetation reaches the driver: board:1780's forest is placed from the ground's own class
      field, not only from a corpus case.
- [ ] A water surface is a declared field of the sphere, drawn where the data says water is, and
      a bridge crossing it is above it (board:1813).
- [ ] Proving measurement: consecutive stills of the city route differ by a majority of pixels,
      because the majority of pixels carry something at a finite distance.
