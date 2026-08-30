Type: feature
State: active
Area: render, world
Tags: picture, measured, gpu

# The ground's class is decided PER PIXEL, and a field edge is an edge

**Benchmark** — Unreal: landscape layers are WEIGHTMAPS sampled in the material, so a layer
boundary resolves at texel rate and `LandscapeLayerBlend` decides the mix per pixel, never per
vertex. RAGE: terrain materials carry a per-texel index blended in the shader. **Both agree, and
the matter is closed**: the class is a per-pixel lookup, not a vertex attribute. Neither of them
would recognise a per-vertex tint as terrain material.

## Measured

`Picturing.cpp:559-581` evaluates the class ONCE PER VERTEX and writes the material's albedo as a
vertex tint:

```
const int which = World.Stack.Classes().ClassAt(*classes, lat, lon, &edgeM, &second);
tinted[one * 4] = wore.Albedo[0];
```

| quantity | value |
|---|---|
| tile span at the finest zoom | 795.654 m |
| `GroundPatchwork::Grid` | 33 |
| **vertex spacing** | **24.86 m** |
| ZurichPlan at 3000 m span | 2.34 m/px |
| **border width in that picture** | **~11 px of interpolated mush** |

A field edge on an aerial photograph is one pixel. Ours is eleven, and it gets worse as the camera
comes closer because the mesh does not.

## Three things already exist and nothing reaches them

1. **`ClassStructure::Words()` and `Bytes()` have NO CALLER.** `git grep` finds the declarations
   and nothing else. `Pack()` already flattens the whole structure into a contiguous, pointer-free
   `uint32_t` array -- somebody built the GPU representation and no declaration reaches it. That
   is CLAUDE.md's commonest defect, sitting on the exact thing this item needs.
2. **`Evaluate(e, n, distM, runnerUp)` is SHADER-SHAPED**: a two-level acceleration grid, per-cell
   seed lists, per-seed edge segments, winding for polygons and half-width for ribbons. Bounded
   loops, flat arrays, no allocation. It ports to MSL as it stands.
3. **`distM` is a SIGNED DISTANCE to the class edge and both call sites discard it**
   (Picturing.cpp:572, GroundSupport.cpp:16), as does `runnerUp`, the neighbouring class. Those
   two are exactly what a hard-but-not-aliased border is made of: the edge lands where the
   evaluation flips, and one pixel of coverage from the distance keeps it from crawling.

## THE GROUND IS DRAWN AS A glTF SUBJECT, and that is what blocks this

Tried and taken back the same round: the device side is easy -- `SceneRenderer::SetGroundClasses`
uploading two storage buffers, `SubjectDraw::ClassFrom`, `FragmentStorageBuffers` 1 -> 3 -- and it
builds. What has no clean answer is HOW THE SHADER KNOWS IT IS DRAWING GROUND. Every route was
walked:

| route | why not |
|---|---|
| a field on `Material` (include/Material.h) | it is a strict glTF metallic-roughness row and this engine reads all of glTF 2.0 (board:1382). A `WearsGroundClass` there is a non-glTF invention inside a glTF struct |
| a new `SurfaceKind` | `SurfaceKind` is the ALPHA axis -- opaque, masked, blended, transmissive. Terrain is not an alpha mode and conflating two axes is how a variant table stops being readable |
| the surface's NAME, `addSurface("ground", ...)` | pins a SPELLING rather than a property, which CLAUDE.md names as a mis-specified check |
| `SubjectMaterial::NormalScale`'s neighbourhood | `SubjectMaterial` IS engine-internal and is the right home for the flag. `Surfacing.cpp:15-40` copies it from the public `Material`, but `Live.cpp:255-268` already reaches back into `Table_.Slots[slot]` after the fact for surface overrides, and `Picturing` holds `ringSurface.index()`. **This route stands** -- an index, not a spelling |

So the flag has a home. What it exposes is the shape underneath: `Picturing` builds the whole world
as one `outshine::Geometry` and hands it to `Live::Restand`, which runs it through the SAME
`Build()`, the same `Table_`, the same `SubjectDraw` as a glTF asset. **Unreal gives Landscape its
own primitive and its own material domain; RAGE gives terrain its own shaders.** Both would call
this the defect, and every clean answer above is clean only because it works AROUND the routing.

The device plumbing was written and then TAKEN BACK rather than landed, because a storage buffer
no shader reads is the exact defect this tree keeps finding in itself -- `Words()` with no caller
is why this item exists at all, and a second one beside it would be worse than none.

## What will be true

- [ ] The ground reaches the device by a GROUND path rather than as a glTF subject, or the
      subject path grows an engine-internal `WearsGroundClass` on `SubjectMaterial` that
      `Picturing` sets by surface INDEX. The first is what both references do; the second is
      what this tree can carry today. **The choice is written down before a line is written.**
- [ ] The packed structure is a storage buffer on the device and `Evaluate` runs in the ground
      fragment shader. The class is decided at pixel rate; the border is where it flips.
- [ ] The 17 materials of `ground-materials.json` are a palette buffer the shader indexes. The
      file's albedos are already linear reflectance locked to a measured broadband value, so
      nothing about the colour is re-decided here -- only where each one applies.
- [ ] `distM` gives the border one pixel of coverage. HARD, not blurred: the width is the pixel,
      not the mesh.
- [ ] Prepared for what comes after, which is why the class must be a lookup and not a colour:
      a material shader reads the class to pick roughness and a normal map, and the vegetation
      placer evaluates the SAME structure so a tree cannot stand on a class the ground does not
      wear.
- [ ] Measurement that shows this is wrong: the width, in pixels, of the transition across a
      known land-cover edge in ZurichPlan -- the lake shore, or the forest edge on the
      Zurichberg. It is ~11 px today and must be 1-2. Negative control: the same edge measured
      on the vertex-tinted build still reads ~11.
- [ ] The CPU `Evaluate` and the shader's are ONE source (board:1580), or the two disagree about
      where a border is and the vegetation stands in the wrong field.
