Type: bug
State: open
Parent: 1953
Progress: streaming
Area: world, engine
Tags: architecture, measured, owner, benchmark

# There is ONE world in the tree, and everything else is a view of it

Owner: *"intern sollte es ja nur eine representation der Welt geben. im zweifelsfalle wie immer an
rage und unreal orientieren."*

Both benchmarks answer it the same way and neither hedges. Unreal has ONE `UWorld`; streaming
brings its cells in and out, actors live in it, and the renderer takes a SCENE PROXY of it -- a
view, never a second world. RAGE streams one map by node and `fwEntity` lives in that one.

## outshine has two, and they overlap by three members

    Ground::World       TilePool  GroundStream  ClassField  OsmField
                        BuildingField  WaterField  StreetField
    Ground::GroundStack TilePool  GroundStream  ClassField

`GroundStack` is a strict SUBSET of `World`. The same three member types, held twice, opened
twice, streamed twice -- two tile pools fetching the same tiles, two ground streams stitching the
same grids, two class fields building the same OSM layers, for one program looking at one place.

The drive path opens the stack; the picture path opens the world. That split is why board:1805's
generators looked unreachable, why board:1924 had to move a class field, and why the ground the
car stands on and the ground the sky paints could disagree at all (board:1918).

**One of those three members is mine, from this session.** board:1924 needed a per-point surface
class on the drive path and put a `ClassField` on the stack rather than asking why the drive path
had a second world at all. The seam was right; the place was a duplicate, and the duplicate grew.

## What will be true

- [ ] ONE type holds the world's fields, and it is opened once per program.
- [ ] What the drive needs is a VIEW: a bounded, non-blocking read of that one world, which is
      what `Sim::Underfoot` already is on the ground half (board:1937).
- [ ] What the renderer needs is a VIEW: the scene proxy shape, not a second set of fields.
- [ ] No two objects in a running program hold a `TilePool`, a `GroundStream` or a `ClassField`
      for the same place.
- [ ] Proving case: a program that stands both paths reports ONE tile pool, and the count of
      tiles fetched for a place is the count fetched once. Negative control: the second world
      restored, and the same place is fetched twice.
