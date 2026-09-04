# A coarser building keeps its colour

State: open

A level of detail may take a building's SHAPE. It may not take its COLOUR, and the tree does both.

## What happens today

Reported from the picture, 2026-09-04: the far field of OldTown reads grey where the town reads
red. Traced to two places:

```
  Box()      drew Facade::RoofFlat unconditionally, so a pitched roof lost its kind along with
             its shape                                                  -- FIXED in this commit
  RoofOf()   returns Flat unless `s.Fill >= kPitchableFromFill`, and a MERGED BLOCK spans several
             houses with gaps between them, so its fill is low and it is always flat -- OPEN
```

The second is the one that shows: a block stands in for thirty houses, and those houses had tiled
roofs. What a viewer reads at that distance is the dominant hue, so the hue is exactly what must
survive the merge -- shape is what LOD is allowed to spend.

## Why it cannot be patched with a rule of thumb

"A block of many members gets a pitched roof" is a guess about a place. In an old town it is right;
in an industrial estate it is wrong, and both are real places this engine renders. The roof kind of
a merged block is not a new decision -- it is a SUMMARY of decisions already made about its
members, and it is only wrong because nothing carries them from the field into the mesher.

## What will be true

`BuildingField::Lumped` carries what its members WERE -- how many are pitched, how many flat --
and `StructurePlan` hands the dominant kind over. The mesher then keeps deciding shape and stops
deciding colour, which is the split it should have had.

## What will show I was wrong

The far field of OldTown against its foreground, by eye and by the hue of the pixels that differ.
Today: red town, grey distance.
