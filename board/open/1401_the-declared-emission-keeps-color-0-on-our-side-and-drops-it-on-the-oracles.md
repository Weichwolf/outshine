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

## Measured before writing any code, and it complicates the repair

[MEASURED] Blender 5.2.0's glTF importer on `VertexColorTest`:

| | |
|---|---|
| attribute name | **`Color`** |
| domain | `CORNER` |
| data type | **`BYTE_COLOR`** |
| carried by | only the mesh whose primitives declare `COLOR_0`; the sibling `Labels` object has none |

**`BYTE_COLOR` is eight bits and Blender treats it as sRGB-encoded**, converting to linear when a
shader reads it. **glTF's `COLOR_0` is linear** and this file's accessor is not eight bits. So the
attribute is not a lossless carrier of the file's own numbers, and a repair that simply multiplies it
in would be comparing our exact value against the oracle's requantised one.

**That is a TERM, not a blocker**: an 8-bit round trip bounds the error at half a step, and the picture
bound already knows how to carry a quantisation term -- it carries one for the display transfer. What
it may not do is go unnamed.

## What the repair must therefore answer, in this order

- [ ] **What the importer actually stored**, checked by reading the attribute back and holding it
  against the file's accessor on a case whose values are known -- `BoxVertexColors` declares float
  `VEC3`, so any difference is the importer's and not the file's
- [ ] **Whether the round trip is sRGB-encoded or linear.** If the importer wrote linear values into a
  slot Blender decodes as sRGB, the error is not a quantisation term at all -- it is a wrong curve, and
  the answer is to write the attribute as `FLOAT_COLOR` rather than to bound it
- [ ] **Only then** the multiply, with the residual term derived and named

## The curve is right and the loss is quantisation, and that changes the repair

[MEASURED] `VertexColorTest`, whose `COLOR_0` is `float VEC4` and therefore able to answer the
question `BoxVertexColors` could not -- **every value in `BoxVertexColors` is exactly 0 or 1, where
sRGB and linear agree, so that asset is blind to the curve.**

| | |
|---|---|
| the file's value | **0.501961** = 128/255, linear |
| what Blender stored | **0.502886** |
| what a WRONG CURVE would have given | ≈ 0.2140 (sRGB-decoding a linear number) |

So the importer is curve-correct. The residual is an **sRGB-domain 8-bit round trip**: 0.501961 linear
encodes to sRGB 0.7354, lands on byte 188, and decodes back to 0.502886. `BYTE_COLOR` is what
`board:1401` above saw and this is what it costs.

**A worse consequence than the size of the residual**: two distinct file values can land on one byte.
Near mid-grey an sRGB step is a linear step of roughly 0.002, so a gradient finer than that is
flattened -- and a case built to test vertex colour is exactly the case likely to carry one.

## The repair this argues for, and it is not the one this item first proposed

**Do not multiply Blender's `BYTE_COLOR` attribute.** Overwrite the colour attribute after import with
a `FLOAT_COLOR` one built from the FILE'S OWN accessor -- the preparer already reads accessors for
animation, so the machinery exists -- and then multiply that. **Both sides then use the file's exact
numbers and there is no error term to derive, name or carry.**

*A term that can be removed is better than a term that is measured: `CLAUDE.md` prefers the shape that
makes a mistake unspellable over the bound that merely accounts for it.*
