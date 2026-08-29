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

**AND IT WAS SAID TO BLOCK THE NEXT MOVE. THAT WAS WRONG, AND THE ERROR IS THE USEFUL PART.** This
item held that the engine's own `Surfaces.cpp` and its 32 spellings could not move behind the door, because the
two functions produce a `SurfaceTable`, and *"it is an ENGINE type, so an importer producing it would
have to reach the engine ... a cycle, and the audit refuses it"*. So the resolution was to be a
HANDOVER of neutral descriptions rather than a file move.

Measured instead of reasoned about, `SurfaceTable` names four things: `Render::SubjectMaterial`
(render), `Raster` (`src/content/shade`), the door's `Material`, and `int`. **Not one of them is
the engine.** It was an engine type by RESIDENCE -- it sat in `src/engine/` and in namespace
`Core` -- and residence is not dependency. Moving it to `src/render/Surfacing.h`, which the importer
already reaches, dissolved the cycle that was never there, and the two functions went to
`src/import/surface/Surfaces.cpp` unchanged.

    src/engine  79 -> 43 spellings, src/render 0, the six places' pictures unmoved to three decimals

**AND THE MOVE FOUND ITS OWN BOUNDARY.** Put beside the document reader in `src/import/`, the two
functions dragged `Image.cpp` and SDL3_image into every suite that links the importer -- `outshine/
content` and `outshine/fuzz` parse glTF and have no business owning an image decoder. Eighteen cases
went BUILD before the shape was right. So the bridge stands in `src/import/surface/`, which only a
suite that renders sweeps, and the reader stays as light as it was. The rule this measured: **a
tier's door is what its clients must link, not what its author finds convenient.** The same run also
showed that `src/content/shade` was being swept as a SOURCE by suites that wanted only its headers,
which is why they were asking a parse case to link SDL3 at all.

The handover design is still owed, and it is board:1949's -- a generator and an importer reaching
the engine the same way. But it was not what unblocked this, and the cycle was never the obstacle.
**A cause written in an item is a hypothesis; this one stood for a round without anyone asking the
compiler.** `Raster` living in namespace `Core` while its file lives in `content` is the same defect
one layer down and is filed as its own item.

## RE-MEASURED AGAIN, and the framing correction widens what this item owns

This item counted the AXIS CONVERSIONS. The owner's correction to board:1949 makes the scope the
whole namespace: glTF is an IMPORT PATH, so `Gltf::` belongs behind `src/import/` and nowhere
else -- the importer reads a document and hands over the internal value, and past that nobody knows
a file was involved.

MEASURED, `Gltf::` outside `src/import/`:

    engine Surfaces.cpp          32   <- now src/import/surface/Surfaces.cpp + src/render/Surfacing.cpp
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
