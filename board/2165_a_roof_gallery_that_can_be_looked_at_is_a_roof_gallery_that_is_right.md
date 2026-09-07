# Every roof in the gallery is right when LOOKED at, not when counted

**State:** open · **Lab rank:** 8 · **Waits on:** board:2156 · **Found:** 2026-09-07, by opening all fourteen

## What is wrong

Seven of seven renders opened are defective, and the best of them passes every number the bed
has. `gabled` measures `comb p95 0.0`, `open 0`, `wound 0`, a positive volume and nothing
unwelded -- and there is a BLACK HOLE at its gable through which the inside of the house is
visible. A viewer needs one second; the suite needs an oracle it does not have.

| shape | what the picture shows |
|---|---|
| onion | the comb over the WHOLE bulb, a fold down the middle where the surface crosses itself, the top out of frame, and it is a rounded BOX rather than a solid of revolution |
| spire | out of frame, and a step in the surface at one corner |
| sawtooth | ONE tooth (the bay is as long as the building) and a black OPEN face where the glazing should be |
| butterfly | the plate overhangs every wall and floats, with a black gap between it and the masonry |
| mansard | a comb along the KNUCKLE, the whole way round, where a sharp horizontal belongs |
| barrel | the comb along the eaves (p95 6.3 after `pq30YY`, and still a row of teeth) |
| gabled | a black hole at the gable; dormers that are solid boxes; a chimney that is a white box |

## The causes, and three of them are structural

**A. A HEIGHT FIELD CANNOT HOLD A VERTICAL FACE OR AN OVERHANG.** A sawtooth's north glazing is a
WALL, an onion's bulb is wider than its drum, and neither is a function of (x, y). The mesher
interpolates across the discontinuity or leaves a hole, and the picture shows the hole.

**B. A SOLID OF REVOLUTION OVER A RECTANGLE'S DISTANCE TRANSFORM IS A ROUNDED BOX.** `roofs.py`
already declares `revolution=True` for `dome`, `onion`, `spire` and `pyramidal`. The mesher
ignores it and offsets the footprint instead, so a dome on a square tower comes out square.

**C. THE COMB**, on every curved surface, and at the mansard's KNUCKLE -- which is a crease the
registry does not name: `crease="ridge"` is one line and the knuckle is another.

**D. A GABLE'S APEX IS TWO POINTS.** board:2156 measured it (2 cm apart) and four attempts made it
worse. Three bodies of 1 393 at OldTown; one hole in the gallery's own gable.

**E. NO ROOF HAS AN EAVES.** No overhang, no gutter, no verge, no ridge tile, no barge board. The
covering stops flush with the wall on every one of the fourteen. At 30 m that is the single
loudest thing separating these from the references.

**F. A DORMER IS A SOLID BOX** with no window, no cheeks and no roof of its own.

**G. THE TILE IS A ONE-METRE CHECKERBOARD**, not a bond: the unit is metres where a plain tile is
0.20 x 0.33 m, and the contrast between neighbours is far above what a roof shows.

**H. THE CAMERA DOES NOT FIT THE SUBJECT.** `spire` and `onion` run out of the top of the frame,
so the one thing the sheet exists to show cannot be seen at all.

## What will be true

- Every shape is FRAMED: the gallery's camera fits the body it is drawing, whatever its height
- A shape declares its own PARAMETERISATION -- a revolution is meshed in its own angle, a
  translation along its axis -- and the height field over a distance transform is what is left
- A shape that is not a function of (x, y) says so, and is built as SURFACES rather than a field:
  the sawtooth's glazing and the onion's bulb are the two that ask for it
- Every roof carries its EAVES: an overhang the covering asks for, a gutter, a verge
- The knuckle of a mansard and a gambrel is a declared crease, like a ridge

## The proofs

| | claim | negative control |
|---|---|---|
| **R1** | every shape's body is closed, wound and welded -- and the gallery's own gable has no hole | remove the gable's apex -> red |
| **R2** | `comb` p95 under 5 degrees on every shape | the table's current row -> red |
| **R3** | the drawn subject fits inside the frame, with margin, for every shape | a spire at twice the rise -> red |
| **R4** | a revolution shape's horizontal section is CIRCULAR to within the mesh tolerance | the distance-transform field -> red |
| **R5** | the eaves projects what the covering asks for, and a gutter stands under it | no overhang -> red |
