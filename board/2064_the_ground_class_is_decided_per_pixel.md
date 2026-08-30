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

## What will be true

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
