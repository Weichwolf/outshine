Type: bug
State: open
Area: engine, render
Tags: measured, owner

# The ground shows no black sliver at Heidelberg where two lattice sheets meet

**Benchmark** -- Unreal's Landscape and CDLOD stitch every level boundary and skirt the rest so
no back face or void is ever visible between two height patches; RAGE's terrain is one baked
mesh with no seams at all. **Both agree**: a viewer never sees between two ground pieces.

## Where it stands, seen 2026-09-05

`build/shots/reference/Heidelberg-c0926138.png`, bottom centre, a thin BLACK sliver about
60 px long between two houses at roughly x 560-620, y 625-632 (reported by the owner, looked
at). Black, not sky, so it is a lit surface facing away or unlit -- most likely a REAL level
boundary (two DEM tiles of different zoom) where board:2115 left the T-junction to the skirt:
`ScoreTheLatticeMeetsItselfAtALevelBoundary` reads 74 real edges at 339 m worst at Jura, and
the skirt hides a crack from above, not from a street-level eye looking along it.

## The solution

Measure first: `pixels.py` on a frame with the skirt drawn in a marker colour names whether the
sliver is the skirt's back face, a gap, or a house's foundation. Then either the real-level
stitch of board:2115 (a provider serving both zooms with one field, so the virtual rule
applies) or a skirt drawn two-sided and lit as ground.

## What will be true

- [ ] Heidelberg's reference has no pixel darker than the darkest lit ground in that window,
      with the count and the picture in the item
- [ ] Negative control: the skirt switched off makes the sliver sky-coloured and the count rises

Measured 2026-09-05 with `pixels.py`'s near-black count (every channel under 20/255):
Heidelberg reads 164 such pixels in the reference and 473 after board:2101's junction press,
the biggest cluster at (576..600, 624..632): the same wedge, wider, because a corridor yield
pressed the lattice node beside it. Its colour is (9, 7, 6) -- darker than any shadowed
wall (70) or roof (90) in the picture, so it is not a lit surface at all: a back face or a
gap. The stamp made it bigger; the stamp did not make it.
