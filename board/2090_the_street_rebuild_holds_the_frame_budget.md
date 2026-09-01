Type: bug
State: open
Area: engine, world
Tags: measured, performance

# The street rebuild fits in a frame instead of stalling one for twenty seconds

**Benchmark** — Unreal: landscape splines DEFORM the heightmap, and they do it in the editor, once,
because the runtime cannot pay it per frame. RAGE: roads are baked into the terrain a map ships
with. **Both agree** — the road's effect on the ground is COOKED, never assembled on the frame path.
Neither faces our version of the question, because neither derives a road at runtime at all; that
choice is ours, so the bound is ours to hold.

## Measured 2026-09-01, Heidelberg through `shots`

    rebuild: of that, the streets and the water      19014.115 ms   BEFORE any change of mine
    p99                                             23886.46 ms    7 of 120 frames over 16.67

That is a pre-existing stall and it is the largest single number in the tree. It was found while
measuring something else, and the something else made it worse:

    with the corridor yield at a 150k-triangle budget    31710.665 ms
    with the corridor yield at a  24k-triangle budget    22026.612 ms

The yield's own contribution is roughly 3 s at the shipped budget. **The 19 s underneath it is the
item.** A rebuild is not a frame -- the ring is rebuilt when the eye crosses a tile, not every
frame -- but a 19 s stall is a 19 s stall, and `p99` says the picture stops.

## And the yield cannot scale here at all

Heidelberg wants 95 658 corridor pieces. At 3 m refinement that is 27.7 MILLION added vertices and
55.3 million triangles, measured before the budget existed. The budget now takes 7 of them and
REFUSES 95 651, which it says where it prints. So on a city the cut and fill is effectively absent,
and saying so is the honest state rather than a number nobody reads.

The durable answer is the one both references already give: the ground yields to the corridor when
the TILE is cooked, once, not when the ring is assembled, every time. Then the refinement is paid
per tile and cached, the budget stops being the thing that decides, and the frame path carries a
lookup instead of a re-triangulation.

## What will be true

- [ ] `rebuild: of that, the streets and the water` is under 100 ms at Heidelberg, and the number
      is quoted from a run rather than estimated.
- [ ] The corridor yield is cooked with the tile, so `ground: yields the budget REFUSED` reads 0 at
      Heidelberg rather than 95 651.
- [ ] Proving case: a place whose rebuild is timed, with the ceiling recorded so it may only fall.
- [ ] Negative control: put the yield back on the frame path and require the ceiling to go RED.
