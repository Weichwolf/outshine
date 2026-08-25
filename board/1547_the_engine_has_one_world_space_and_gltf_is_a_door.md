Type: feature
State: open
Area: gltf, render
Tags: scope, layering

# The engine has ONE world space, and glTF is a door rather than a coordinate system

Unreal converts Y-up right-handed content into its own world once, at load; RAGE normalises in
the asset pipeline. **The conversion is a door, not a step in the frame.** Here
`EcefFromGltf` appears ten times in `src/engine/GltfStudio.cpp` on the RUNTIME path: every
vertex, normal, tangent and previous position per upload; every light's direction and position;
the eye, its forward, its right and its up; and the per-part placements, added the day they
became the thirteenth thing somebody forgot.

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
