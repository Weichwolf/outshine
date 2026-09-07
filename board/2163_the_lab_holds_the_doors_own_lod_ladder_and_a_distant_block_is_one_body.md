# The lab holds the door's own LOD ladder, and a distant block is ONE body

**State:** open · **Lab rank:** 6 · **Waits on:** board:2164 · **Found:** 2026-09-07, reading `Generate.h` beside `visible.py`

## What is wrong

**Two sources for one rule, and the lab holds the one the door already recorded as WRONG.**

`include/generate/Generate.h` states the ladder as RAGE's -- `Detail::Fine`, `Shell`, `Massed`,
`Skyline` for HD, LOD, SLOD1, SLOD2/3 -- picks the rung with `Unseen(errorM, focalPx, awayM)`, and
says in as many words why:

> IT IS THE ERROR AND NOT THE SIZE. What a subject MEASURES on screen answers "is it visible at
> all"; what its simplification MOVES answers "is the simplification visible", and only the second
> licenses replacing geometry. Measured here the difference is the whole thing: judging by size,
> twenty houses were merged into one block whenever the houses fell under a couple of pixels --
> while the block itself, a hundred metres across, was plainly there and plainly wrong.

`test/lab/visible.py:rung_for` judges by SIZE. It compares a pixel's ground size against a table of
feature sizes (0.10 m, 0.30 m, 1.00 m) and returns 3, 2, 1, 0. That is exactly the rule the door
paid for and threw away, wearing different numbers and different names.

## What else is missing

**There is no MERGE.** `Building.body(lod)` at rung 0 and 1 keeps the mass's own solid wall, which
is `Detail::Shell`. Nothing in the lab produces `Detail::Massed` -- a city block as ONE body -- and
that is the rung that decides what a distant town costs. Measured at OldTown 2026-09-07 from the
client's own camera, 60 m up: **1 330 of 5 709 bodies survive the cull**, and every one of them is
meshed on its own. A block of thirty terraced houses is thirty bodies, thirty roofs and thirty sets
of walls where RAGE draws one.

The merge is also what makes a distant skyline STOP SHIMMERING, and the door says why: the merge is
stable because the block is.

## The decision

**Mirror the door. One rule, one place, and the place is `include/`.**

| | |
|---|---|
| the ladder | `Detail.Fine / Shell / Massed / Skyline`, the names the door uses |
| the rung | `DetailAtRung(rungsCoarser)` and `Coarser(a, b)`, copied field for field |
| the choice | `Unseen(errorM, focalPx, awayM)`: `errorM * focalPx <= kErrorPx * awayM` |
| the error | what the SIMPLIFICATION MOVES, not what the subject measures |
| `focalPx` | `width / (2 tan(fov/2))`, and not the small-angle `fov/width` the lab uses now |

`rung_for` goes. What replaces it takes the error a rung introduces -- for a building that is the
depth of the relief the rung drops -- and asks the door's own question.

**The merge unit is the BLOCK, and OSM already names it**: bodies that share a party wall, which
`buildings/gallery.py` and the terrace generator already find. A block merges to one shell whose
error is the depth of the setbacks it flattens.

## The proofs

| | claim | negative control |
|---|---|---|
| **D1** | the lab's `DetailAtRung`, `Coarser` and `Unseen` agree with the `static_assert`s in `Generate.h`, case for case | change one bound -> red |
| **D2** | a merged block's error, projected from the distance the merge is chosen at, is under one pixel | merge one rung earlier -> over a pixel -> red |
| **D3** | the picture at `Massed` and the picture at `Shell` differ by under one pixel of silhouette | the size rule instead of the error rule -> red |

## What Unreal does, what RAGE does

**RAGE**: HD, LOD, SLOD1-3, chosen by `lodDistance`, merged at build time, and a coarser entity
REPLACES the finer ones. **Unreal**: HLOD clusters built offline per World Partition cell, chosen
by screen size; Nanite picks a cluster when the error IT introduces is under a pixel. They agree
on everything except the unit of the choice, and the door has already taken Nanite's -- the ERROR
-- because it is the one that can be stated without knowing how big the subject is. Nothing here
is a third answer.
