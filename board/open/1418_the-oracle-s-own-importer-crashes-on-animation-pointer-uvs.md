Type: bug
Area: corpus
Tags: oracle, khronos

**The oracle's own importer crashes on AnimationPointerUVs, so the case has no reference at all**

[MEASURED] Blender 5.2.0's bundled glTF importer raises inside its own `KHR_animation_pointer`
handling:

```
if anim_idx not in gltf.data.materials[int(pointer_tab[2])]
        .extensions[pointer_tab[4]][pointer_tab[5]]['extensions']["KHR_texture_transform"]["animations"].keys():
KeyError: 'animations'
```

**So the preparation exits 9 and no oracle is produced.** This is not a disagreement about a picture;
there is no picture on the reference side.

## Why it is filed against the corpus and not against the reader

**This engine reads the file.** `board:1392` made an animation channel it cannot drive a counted
shortfall rather than a refusal, so the subject loads and draws its rest pose; the asset declares over a
hundred pointers and every one of them is a `KHR_texture_transform` offset, scale or rotation.

**The oracle is the side that cannot.** The ladder's first rung -- fix the engine -- has nothing to fix,
and the second -- reduce the oracle -- cannot reduce a render that does not happen.

## What must be true

- [ ] **The case declares that its oracle cannot be produced**, with this traceback beside it, and
  reports UNPREPARED rather than red -- which is the state the harness already has for a case whose
  oracle is missing, and is the honest one here
- [ ] **It is re-checked against the Blender version the manifest pins.** A fixed importer turns this
  from a limit into a case, and nothing else about the asset has to change
- [ ] **It is NOT patched around.** Editing the asset to drop the pointers would produce an oracle for a
  file the corpus does not contain

## Comments

**This is the second thing this asset has taught in one round.** It is also the reason
`ADerivedCameraIsTheFramingRuleAndNotAQuotation` reports *the case's subject reads* against it -- the
prepared directory holds no `scene.gltf`, because the preparation died before it wrote one.
