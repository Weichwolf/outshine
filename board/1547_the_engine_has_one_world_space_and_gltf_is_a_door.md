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
    src/content/gltf/Axes.{h,cpp}  2 + 2   <- the door itself, where it belongs

Every vertex, normal, tangent and previous position per upload; every light's direction and
position; the eye with its forward, right and up; and the per-part placements. Ten became
fourteen while the item waited, which is what an unguarded count does.

Every new thing that enters a picture must remember to convert and the compiler cannot help.
That is the defect this item makes unspellable.

## What will be true

- [ ] The conversion happens ONCE, at the reader's door, and no runtime path spells a glTF
      convention.
- [ ] A quantity that has not been converted has no spelling — the type says which frame it is
      in (board:1611).
- [ ] Proving test: a subject read, posed and drawn produces the same pixels as today; negative
      control — the conversion removed at the door, everything is wrong at once rather than one
      forgotten call site at a time.
