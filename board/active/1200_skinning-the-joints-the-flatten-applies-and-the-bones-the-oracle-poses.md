Type: task
Parent: 0079
Area: gltf
Tags: khronos, oracle, instrument

**Skinning: the joints the flatten applies, and the bones the oracle poses**

The next row of `board:0079`'s sequence that a render can actually prove. **Impact 6 in-scope models**;
tier 1 by that section's own rule — reader plus draw list, no new resource and no new lobe. Rows 2 and 3
are behind it for reasons already recorded: `TEXCOORD_1` is delivered (`board:1182`) and the occlusion
texture cannot be decided by this oracle at all (`board:0079`, three gates).

## What is there and what is not, measured rather than recalled

| | |
|---|---|
| `skins`, `JOINTS_0`, `WEIGHTS_0` in the reader | **absent.** The only hit under `src/gltf/` is a comment in `Types.h` deferring which of them a vertex layout holds |
| the vertex-layout question `board:0079` raises and does not answer | **answered by precedent.** `Gltf::Subject` already carries `Uv`, `Uv1`, `Normals`, `Tangents` and `Colours` as separate optional streams with `Has*()` beside each. Joints and weights are two more of the same, and no fifth layout and no fat layout is needed |
| where the skin is applied | **the flatten, on the CPU.** `Parity.cpp` already re-poses geometry every frame through `PoseGeometry`, so linear blend skinning lands beside the pose it composes with rather than in a GPU pipeline |

**`RiggedSimple` at the pin**, read here: one skin, `joints [3, 4]`, `skeleton 3`,
`inverseBindMatrices` accessor 9; node 2 is the `Cylinder` carrying `skin 0`; the primitive declares
`JOINTS_0` and `WEIGHTS_0` beside `POSITION` and `NORMAL`; **the only animated node is 4**, a joint.

## The oracle's half, and it is the part that is not obvious

[MEASURED] Blender 5.2.0 importing `RiggedSimple`:

```
OBJ Armature   type=ARMATURE  action=Anim_0  paths=['pose.bones["Bone.001"].location', ...]
     bones: ['Bone', 'Bone.001']
OBJ Cylinder   type=MESH      parent=Armature
     vertex groups: ['Bone', 'Bone.001']   modifiers: [('ARMATURE', 'Armature')]
```

**A joint is a POSE BONE and not an object**, so `board:1198`'s baker cannot reach it: it resolves a node
to `bpy.data.objects[name]`, and `Bone.001` is not there. **That is the refusal it was built to give** —
*the glTF names an animated node and no imported object carries that name* — so this arrives as a loud
stop rather than as a rest-pose armature rendered beside a moving one.

- [ ] **The baker learns pose bones**, and the conversion is its own claim: a pose bone's `matrix_basis`
  is **relative to its rest pose**, not the node-local transform glTF states, so the value written is
  `rest⁻¹ · local` and not `local`. **It is checked the same way the axis conversion is** — against the
  importer's own first key, up to sign for a quaternion — so a wrong hypothesis refuses instead of
  rendering a plausible wrong pose. *Three such conversions have already been wrong this round and none
  of them crashed*

## Done when

- [ ] `skins`, `node.skin`, `inverseBindMatrices`, `JOINTS_0` and `WEIGHTS_0` are read, with the
  component types the format allows for each — joints are `UNSIGNED_BYTE` or `UNSIGNED_SHORT`, weights
  are `FLOAT` or normalised integer — and a file whose weights do not sum to one is **stated, not
  silently renormalised**
- [ ] `Gltf::Subject` carries the two streams and `HasSkin()`, on the pattern the other five follow
- [ ] The flatten applies linear blend skinning: `Σ w_i · (globalOf(joint_i) · inverseBind_i) · v`, with
  the skinned node's own transform handled by the format's rule rather than by ours
- [ ] The baker poses bones, with the rest-relative conversion checked against the importer
- [ ] **`RiggedSimple` is a render case, frame by frame**, and it is the proof — a unit test standing in
  for a picture is what `board:0079`'s closing line forbids
- [ ] A deliberately dropped joint weight is shown red, so the case is known to be able to fail

## Comments

**The vertex-layout question was the expensive-looking part and it is already settled.** `board:0079`
records it as open — *a fifth layout per capability is the wrong answer, and so is one fat layout* — but
`Subject` answered it in the meantime by carrying each semantic as its own optional stream and packing
at the draw. **The open question was about the GPU layout and the flatten never had one**, which is why
the answer arrived without anybody deciding it.
