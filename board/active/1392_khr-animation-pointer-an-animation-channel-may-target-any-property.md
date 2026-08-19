Type: task
Parent: 1382
Area: gltf
Tags: khronos

**Khr animation pointer an animation channel may target any property**

An animation channel whose target is a JSON pointer rather than one of the four node paths, so a file may
animate a base colour factor, a light intensity, a texture transform or a node's visibility.

**It is a SELECTION shape and the widest one**: the reader must resolve a pointer to a property it
already holds, which means the set of animatable properties becomes an enumeration the reader publishes
rather than a switch it hides.

**Five models use it**, and it is what `KHR_node_visibility` is specified to be used with.

- [ ] The set of pointers this reader resolves is ENUMERATED and published, and a pointer outside it is
  a named refusal rather than a silent no-op
- [x] A pointer that resolves is animated by the same sampler machinery the four node paths use --
  `Pose::FactorsAt` samples the channel's own `Track`, the one the four node paths use, and answers
  which material, which factor and its values at an instant. Proven by
  `test/outshine/unit/gltf/AHierarchyIsPosedAtTheTimeItIsAsked.cpp`, whose fixture drives a
  `baseColorFactor` pointer from the SAME sampler its rotation uses -- so the values the factor must
  read are the file's own `(0, 0, 0, 1)` and `(0, 0, sqrt(1/2), sqrt(1/2))` and not a second set of
  numbers to keep true. **An accessor is untyped data and the format says so**, which is what makes one
  sampler legitimately drive a quaternion and a colour, and the halfway sample is what separates them:
  a base colour interpolates as a vector where `LINEAR` on a rotation means slerp

Specification: <https://github.com/KhronosGroup/glTF/tree/main/extensions/2.0/Khronos/KHR_animation_pointer>. **Fetched, never recalled** -- a rule quoted from memory is a defect one step before the code.

**Shape: selection** (see the parent's table).

## The grammar this reader resolves, and it is declared rather than discovered

**The asset object model is every mutable property the format has**, so "implement the extension" has no
edge. What the corpus actually asks for was measured first, over the three assets that carry it:

| asset | pointers |
|---|---|
| `AnimatedColorsCube` | **one** -- `/materials/0/pbrMetallicRoughness/baseColorFactor`, beside plain `translation` and `rotation` channels |
| `PotOfCoalsAnimationPointer` | two, both `KHR_texture_transform/rotation` |
| `AnimationPointerUVs` | **over a hundred**, every one a `KHR_texture_transform` offset, scale or rotation on every texture slot of every extension |

**So the grammar splits into two shapes and only one of them is here.** A material's own factors --
`baseColorFactor`, `metallicFactor`, `roughnessFactor` and `emissiveFactor` -- resolve now; they are one
two-segment walk and naming all four cost nothing beyond naming them. The texture-transform shape is a
task of its own and it is NOT started, because most of its targets sit inside extensions this reader
does not hold at all.

**A POINTER OUTSIDE THE GRAMMAR IS A REFUSAL THAT QUOTES ITSELF.** *this reader resolves for a
material's baseColorFactor, metallicFactor, roughnessFactor or emissiveFactor and for nothing else* --
because a grammar that silently accepted an unknown path would read an animation and drive nothing,
which is the shape this tree refuses everywhere else.

**And the pointer is WALKED rather than matched against strings**, so the two questions stay apart: is
this a pointer this reader understands, and does the material it names exist. A file with a good
pointer to a missing material must not read as a file with a pointer nobody understands.

## What it bought and what it did not

- [x] **`AnimatedColorsCube` is green** -- criteria 1 of 1, inside the picture bound. It had been
  refused before it could project anything, so its declared frame fraction was a [SET] placeholder of
  zero that said so, harvested now at 0.104956927
- [ ] **`AnimationPointerUVs` still refuses**, and now the refusal quotes the pointer it could not
  resolve rather than only the path name. Its targets reach `KHR_materials_diffuse_transmission`,
  `KHR_materials_transmission` and `KHR_materials_volume`, so it is blocked on `board:1386` and
  `board:1387` as much as on the texture-transform grammar
- [ ] **NOTHING DRIVES A MATERIAL YET.** The channel is read, resolved and carried; `Pose` answers the
  new arm by doing nothing and says why -- a material factor is not a node pose. **Applying it means a
  material row that changes per frame, which is a capability and not a parse**, and it is not claimed
  here in either direction

## The oracle CAN decide a material factor, and no case asks it to

Two measurements, and together they say exactly which half of this extension is testable today.

**Blender imports a `baseColorFactor` pointer as an action on the material's node tree.** [MEASURED] on
`AnimatedColorsCube`, loaded in Blender 5.2: `MATERIAL AnimatedColorMaterial node-tree anim: True`,
carrying the action `Cube Animation`, while its three static siblings carry none. **So the reference
animates the factor** and a case that compared it would decide whether this engine does.

**And Blender refuses a `KHR_texture_transform` pointer**, which is what `PotOfCoalsAnimationPointer`'s
declared reduction records: its oracle renders five identical frames while the file turns two texture
transforms through a full circle.

| pointer shape | the oracle | a case that tests it |
|---|---|---|
| a material's own factor | **animates it** | none -- see below |
| a `KHR_texture_transform` field | renders a still | `PotOfCoalsAnimationPointer`, reduced |

## Why no case tests the half that IS decidable

**`AnimatedColorsCube` replaces its material.** Its manifest declares
`scene.material.source = manifest`, `kind = emission-per-material` -- the runner hands each glTF material
a flat emission keyed by the file's own name for it -- so the file's `baseColorFactor` never reaches the
picture and neither does the channel that animates it. [MEASURED] the case is green at
`picture_p99_delta_code` **0 codes on all four frames**, and that green is about the cube's TRANSLATION
and ROTATION, which is what it does test.

**So the case exists, the oracle is willing, and the declaration steps between them.** Pointing its
material at the file -- `source: gltf`, `kind: metal-rough` -- is what would make it decide this, and it
would go **red**, because this item's own open line says nothing drives a material yet. *That is a red
worth having and it is not taken here: a standing red with no path to green is worse than a named gap,
and the round that implements the capability is the round that should flip the case.*

## The pose answers; the row is still the consumer's question

`Pose::FactorsAt` is a separate call from `At` because it answers a separate question and its result is
keyed by MATERIAL where that one is keyed by node -- and a consumer that shades from the manifest rather
than from the file has no use for it at all. **The remaining line of this item is unchanged**: nothing
applies these values to a material row per frame, and that is a capability with its own cost.

*What changed is that the values now EXIST at an instant, which is what the consumer will need and what
no amount of reading the channel could supply.*
