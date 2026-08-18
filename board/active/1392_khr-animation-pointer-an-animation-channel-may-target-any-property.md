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
- [ ] A pointer that resolves is animated by the same sampler machinery the four node paths use

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
