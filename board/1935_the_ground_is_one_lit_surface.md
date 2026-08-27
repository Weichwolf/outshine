Type: bug
State: open
Parent: 1575
Area: render
Tags: measured, picture, driver, lighting

# The ground under the car is ONE lit surface, and no straight terminator crosses it

**Benchmark** — Unreal: one opaque lighting path, and a shadow terminator is a bias problem inside it rather than two paths meeting. RAGE: the same. **Both agree** — a surface is lit once.

Measured at bb9472db in the stakeholder's own worktree, chase view, two Munich routes,
`--every --frames 1800 --stills 8`.

## The step

`shots-river-chase/along05.png`, the drive 48.1310,11.5820 -> 48.1290,11.5930. One scanline,
two pixels apart, on a flat continuous ground plane at the same distance from the camera:

| y | x of the jump | left of it | right of it | ratio |
|---|---|---|---|---|
| 350 | 831 | (5,14,26) | (67,83,68) | 13:1 |
| 400 | 1036 | (4,11,22) | (65,81,65) | 15:1 |
| 450 | 1256 | (4,11,21) | (65,80,64) | 15:1 |

The three crossings are collinear and run to the horizon: it is the projection of one straight
world-space line, not a caster's silhouette. There is no caster in the frame -- the tallest thing
in the world is the car, and a 42 deg sun gives it a 1.6 m shadow, not a 400 m one.

## The subject rides the same term

The car's own box (x 530..760, y 410..600) across the eight stills of that route:

    along03  mean 28.0    along05  mean 28.1    along06  mean 60.6    along07  mean 60.5

The car doubles in brightness at the same crossing the ground does. Whatever darkens the ground
darkens the subject standing on it, so it is a light-visibility term and not a material.

## What it does to a still

Stills 01 to 04 of that route render the ENTIRE ground at (2..4, 7..11, 12..22) while the sky
above is unchanged at (13..22, 28..39, 52..59): a midday scene whose ground sits 4 stops under
its own sky, and 400 m later the same ground reads (61,76,60). The route is flat, the clock does
not move, and the picture changes by a factor of 15.

## The candidate, not asserted

board:1575 states the only executed shadow path is a per-pixel software BVH ray per light. A
bounded acceleration structure returns "occluded" inside its extent and "clear" outside it, and
its extent is a box -- which is what a straight terminator sweeping across the world looks like.
The run prints `the shadow atlas, least depth = 1 / its most = 1`: the atlas holds one value, so
whatever samples it cannot be discriminating either.

## What will be true

- [ ] One flat unoccluded ground under one sun renders at one luminance, and the case measures
      the maximum step between neighbouring pixels of a single ground plane at NIL.
- [ ] Where a terminator DOES cross the ground it belongs to a caster in the frame, and the
      case names the caster.
- [ ] Negative control: a caster placed on the plane produces a step, at the bearing and length
      the sun's elevation gives.
