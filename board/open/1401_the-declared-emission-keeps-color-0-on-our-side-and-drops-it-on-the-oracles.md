Type: bug
Area: harness
Tags: oracle, khronos

**The declared emission keeps `COLOR_0` on our side and drops it on the oracle's**

`VertexColorTest` scores **p99 63 codes against a bound of 1**, with the attribution line reading *the
geometry is in the right pixels and the shading is wrong*. **That description is wrong and LOOKING is
what says so.**

## What the two pictures show

Side by side the plate is in the same place, at the same angle, with the same silhouette, and the same
seven salmon bars in the same positions. **The oracle's plate is one flat blue. Ours carries six extra
quads the oracle does not draw at all** -- a dark red, a green, a blue and three pale blue -- and our
plate's blue is duller than the oracle's everywhere else.

That is not a shading residual. **It is `COLOR_0`**: our renderer multiplies the file's vertex colour
into the colour the manifest declared, and the Blender emission shader the preparer builds does not
read the attribute at all. The duller plate is the same multiplication acting where the vertex colour
is a near-grey.

## Which side is wrong, and it is not ours

*if a primitive specifies a vertex color using `COLOR_0`, then this value acts as an additional linear
multiplier to base color* -- glTF 2.0's own sentence, already pinned by
`test/outshine/unit/gltf/AVertexColourMultipliesBaseColourAfterTheDecode.cpp`. **Our renderer is doing
what the format says.** Teaching it to ignore vertex colour when a manifest declares a colour would put
a test-only behaviour inside the engine, which is the one thing this decomposition forbids.

**So the ORACLE is the side to teach**: the emission node group must multiply the mesh's colour
attribute the way our shader does, and where a subject carries no `COLOR_0` the multiplier is the
identity and nothing changes.

## The population, counted

[MEASURED] **8 of the 148 models carry `COLOR_0`**: `BoxVertexColors`, `CompareBaseColor`,
`IridescentDishWithOlives`, `MeshoptCubeTest`, `PrimitiveModeNormalsTest`, `RecursiveSkeletons`,
`SheenWoodLeatherSofa`, `VertexColorTest`.

## What must be true

- [ ] **The preparer's emission shader multiplies the colour attribute**, and does so in the same space
  our shader does -- `COLOR_0` is LINEAR in glTF and Blender's colour attributes are too, so a decode
  step here would be a second one
- [ ] **A subject with no `COLOR_0` is bit-identical to today**, and that is checked rather than argued
- [ ] **The eight cases are named before and after.** `CompareBaseColor` is green today at p99 0, so a
  change that moves it is a change that reached further than this
- [ ] **`VEC3` and `VEC4` both work**, because the format permits both and the alpha of a `VEC4`
  multiplies base colour's alpha

## Comments

**The metric said "shading" and the picture said "an attribute nobody applied".** p99 63 codes over a
plate whose geometry agrees to the pixel is exactly the shape a missing multiplier makes, and it read
as a shading residual for as long as nobody opened the two files.
