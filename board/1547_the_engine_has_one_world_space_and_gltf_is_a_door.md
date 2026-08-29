Type: feature
State: open
Area: gltf, render
Tags: scope, layering

# The engine has ONE world space, and glTF is a door rather than a coordinate system

**Benchmark** — Unreal: one world space, and an importer converts into it at the door. RAGE: one map space. **Both agree** — glTF is a door, never a coordinate system the engine adopts.

Unreal converts Y-up right-handed content into its own world once, at load; RAGE normalises in
the asset pipeline. **The conversion is a door, not a step in the frame.**

**RE-MEASURED, because the file this item named no longer exists.** `EcefFromGltf` in
`src/engine/GltfStudio.cpp` is gone -- the file became `src/render/SubjectProxy.cpp` and the
call is spelled `Gltf::InEcef` / `Gltf::PlacedInEcef` now. The defect did not go with the
rename. Fourteen runtime sites:

    src/render/SubjectProxy.cpp   10
    src/engine/Live.cpp            4
    src/import/Axes.{h,cpp}  2 + 2   <- the door itself, where it belongs

Every vertex, normal, tangent and previous position per upload; every light's direction and
position; the eye with its forward, right and up; and the per-part placements. Ten became
fourteen while the item waited, which is what an unguarded count does.

Every new thing that enters a picture must remember to convert and the compiler cannot help.
That is the defect this item makes unspellable.

## THE CONVERSION ITSELF IS UNRESOLVED, and the probe is what says so

Before the nine `Gltf::InEcef` calls can leave `src/render/`, it has to be known what they DO, and
reading does not settle it. `Site::Push` writes plain ECEF component order --
`Origin_[c] + P.E * East_[c] + P.N * North_[c] + Z * Up_[c]` for c = 0,1,2 -- while `PackVertices`
applies `InEcef`, which is `(x,y,z) -> (y,x,-z)`: a real swap, and an involution. Both cannot be
true of a correct picture.

**PROBE, and the pictures were LOOKED AT rather than scored.** `InEcef` made the identity:

    Rothenburg    1.948 -> 1.951    a correct town either way
    Central Park  1.194 -> 1.176    SAME composition -- bright towers left, dark right,
                                    the same structure in the same place
    Shibuya       1.960 -> 1.898

A swap of x and y is a MIRROR, and a mirror puts Manhattan's bright cluster on the other side. It
does not move. So POSITIONS are unaffected while the statistics shift by fractions, which fits the
conversion mattering for NORMALS -- `PackVertices` applies it to those as well.

Three observations that cannot all hold, and no image separates them. **The next step is an
instrument, not a tenth hypothesis**: print one known vertex either side of the conversion. Nine
have died in one session, every one from reasoning off a picture or a count instead of measuring
the thing itself.

## THE TIERS TURNED, AND THE NEXT MOVE IS A HANDOVER RATHER THAN A MOVE

`src/import/` is `src/import/` now, a tier of its own reaching `base content render`, and
`--audit-layers` holds it: the render tier CANNOT name the importer any more, whoever wants to. That
is the owner's correction -- the importer knows the engine -- put into the build rather than into a
count somebody maintains. The fence found two dead includes the moment it went up: `Axes.h` and
`Subject.h` in `SubjectProxy.h`, left over from the axis conversions.

**AND IT BLOCKED THE NEXT MOVE FOR A GOOD REASON.** `src/engine/Surfaces.cpp` carries 32 of the 84
remaining spellings in two functions that take a `Gltf::Document` -- genuinely import work. Moving
them behind the door fails to compile, because they produce a `SurfaceTable`, which holds `Raster`
from `src/content/shade/Image.h` and is used by `Live` alone. **It is an ENGINE type**, so an importer
producing it would have to reach the engine, and the engine already reaches the importer: a cycle,
and the audit refuses it.

So the resolution is not a file move. **The importer hands over a NEUTRAL description** -- the door's
own `Material` rows and the images -- and the engine builds its own table from that. That is a
handover, and it is what makes the remaining 32 leave without the engine learning a file format.

## RE-MEASURED AGAIN, and the framing correction widens what this item owns

This item counted the AXIS CONVERSIONS. The owner's correction to board:1949 makes the scope the
whole namespace: glTF is an IMPORT PATH, so `Gltf::` belongs behind `src/import/` and nowhere
else -- the importer reads a document and hands over the internal value, and past that nobody knows
a file was involved.

MEASURED, `Gltf::` outside `src/import/`:

    src/engine/Surfaces.cpp      32
    src/render/SubjectProxy.cpp  27   <- the renderer knowing a FILE FORMAT
    src/engine/Live.cpp          18
    src/engine/Asset.h           10
    ten more files               42
    -------------------------------
    129 in 14 files

**The renderer is the sharpest of these.** board:1995 already refuses a SUBJECT noun in
`src/render/`; a file format is the same defect one step worse, and the guard that holds the first
does not know the second word.

The item's own warning has come true twice over -- it wrote "ten became fourteen while the item
waited, which is what an unguarded count does", and the wider count is 129. So the guard comes
BEFORE the removal: a claim beside `TheRendererNamesNoSubject`, walking for `Gltf::` outside the
importer, with a declared number that may only fall.

## What will be true

- [ ] The conversion happens ONCE, at the reader's door, and no runtime path spells a glTF
      convention.
- [ ] A quantity that has not been converted has no spelling — the type says which frame it is
      in (board:1611).
- [ ] Proving test: a subject read, posed and drawn produces the same pixels as today; negative
      control — the conversion removed at the door, everything is wrong at once rather than one
      forgotten call site at a time.
