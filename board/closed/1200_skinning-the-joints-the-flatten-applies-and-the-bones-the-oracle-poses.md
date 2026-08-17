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

- [x] **The baker learns pose bones, and it does NOT do what this box first said.** The plan was to write
  `rest⁻¹ · local` onto the bone — deriving Blender's bone conventions ourselves and checking them. The
  built answer inverts that: **the importer's CONVERSION is reused and only its INTERPOLATION is
  replaced**, because a pose bone is rest-relative *in the bone's own axes* and glTF states neither, so
  there is no conversion to check that Blender has not already computed.

  **That makes the bone arm strictly narrower than the object arm, and it says so per channel.** The
  object arm evaluates the file and covers all three modes. The bone arm re-interpolates the importer's
  own keys, which is exact only where the importer stored them exactly — `STEP` and `LINEAR` — and
  **`CUBICSPLINE` on a joint is a named refusal**, because a Bézier handle cannot be turned back into
  glTF's Hermite. Every channel publishes which arm carried it under `carriedBy`, so a green animated
  case cannot hide that a joint took the narrower road.

  [MEASURED] on `RiggedSimple`, Blender 5.2.0: all three channels resolve to pose bone `Bone.001` on
  armature `Armature`, 50 keys resample onto a 17-frame grid, and the resampled quaternion is unit to
  **0.999999993** — so slerp survives conjugation into bone space, which is the one algebraic claim the
  arm rests on. The `CUBICSPLINE` refusal fires.

## Done when

- [x] `skins`, `node.skin`, `inverseBindMatrices`, `JOINTS_0` and `WEIGHTS_0` are read, each through
  `ReadElements`, which is where the format's component types already live — so joints arrive from
  `UNSIGNED_BYTE` or `UNSIGNED_SHORT` and weights from `FLOAT` or a normalised integer without a second
  decoder. **Weights that do not sum to one are used as declared and not renormalised**, because glTF
  says *SHOULD* and not *MUST*; a vertex whose weights sum to zero is refused
- [x] **`Gltf::Subject` does NOT carry the two streams, and the reason is the better answer.** The plan
  was two more optional streams on the pattern the other five follow. But the flatten **bakes** the skin
  into positions, so nothing downstream reads a joint or a weight — storing them would be a stream read
  and thrown away, which is exactly the defect `board:0079`'s occlusion row was just filed over. They are
  consumed where they are decoded and go no further
- [x] **The flatten applies linear blend skinning**, and the skinned node's own transform is ignored as
  the format states. The choice has **one spelling** — `VertexPlacement::At` — so no later reader can
  apply the node transform to a skinned vertex by reaching for the obvious variable. Positions, normals
  and tangents all route through it
- [ ] The baker poses bones, with the rest-relative conversion checked against the importer
- [x] **`RiggedSimple` is a render case, frame by frame, and all three arms are green.** 18 frames at
  8 fps, **worst geometric disagreement 0.00074717091 px against the 0.005 px instrument floor**, 458
  checks and no failures — with `BoxAnimated` re-scored beside it in the same run so the change to the
  shared instrument is not a claim about one case
- [x] **Shown to be able to fail**: posing the subject at frame 0 for every frame takes it red on the
  motion clause **by name** and on `worst_disagreement_px`, because a frozen subject disagrees with a
  moving oracle in both directions

## Comments

**The vertex-layout question was the expensive-looking part and it is already settled.** `board:0079`
records it as open — *a fifth layout per capability is the wrong answer, and so is one fat layout* — but
`Subject` answered it in the meantime by carrying each semantic as its own optional stream and packing
at the draw. **The open question was about the GPU layout and the flatten never had one**, which is why
the answer arrived without anybody deciding it.

## What the flatten decided that the format left open

**The winding rule has no input for a skinned primitive.** glTF states the triangle winding is reversed
when *the node's global transform* has a negative determinant — and a skinned primitive ignores that
transform, so the rule as written names nothing. The sign is therefore taken from the blended matrices
themselves, and **a primitive whose vertices do not agree on it is refused**: one primitive cannot carry
two windings, and picking either would flip half its triangles.

**The weights are used exactly as the file declares them.** glTF says a float weight set *SHOULD* sum to
one; it does not say *MUST*. Renormalising would repair somebody else's asset inside a comparison whose
subject IS that asset — the same argument `COLOR_0`'s range refusal turns the other way, because there
the format says MUST. **What is refused is a vertex whose weights sum to zero**, which names no position
rather than an unusual one.

**The matrices are blended and then applied, not applied and then blended.** They are the same number —
the transform is affine and the blend is linear — so the order is chosen for cost: four matrix adds per
vertex against four point transforms per attribute.

## The acceptance clause this case had to narrow, and what it was actually wrong about

**`board:1169` demanded that every frame's drawn subject differ from FRAME 0's**, and `RiggedSimple`
**loops** — its bend returns to the rest pose, so at frame 17 the motion from frame 0 is **exactly 0 m**
and the picture is correct. The old clause therefore forbade an animation from ending where it began.

**The verdict moved from the frame to the GRID**, and what the clause exists for survives intact: a case
whose subject never moves is a still rendered N times and agrees with the oracle by construction rather
than by being right. That is a statement about the sequence and it is now made where it is true, with
**the count of frames that moved published beside the maximum** — 16 of 18 here — so a grid where one
frame carries all the motion is visible rather than merely passing.

**A hold is a different case and was never caught by the old rule, which is worth recording because it
looks like it would be.** `BoxAnimated` carries the same translation at two keyframes and is stationary
between them, but the comparison was against frame 0 rather than the previous frame, and it never returns
there. *The rule that had to change is the one about RETURNING, not the one about STANDING STILL* — and
the first draft of this comment claimed otherwise.
