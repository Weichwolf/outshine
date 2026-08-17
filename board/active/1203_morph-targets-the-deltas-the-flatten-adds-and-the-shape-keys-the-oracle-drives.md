Type: task
Parent: 0079
Area: gltf
Tags: khronos, oracle, instrument

**Morph targets: the deltas the flatten adds, and the shape keys the oracle drives**

The next row of `board:0079`'s sequence after skinning. **Impact 4 in-scope models**; tier 1 — reader
plus flatten, no new resource and no new lobe. **It is skinning's sibling twice over**: the reader is
missing the same shape of table, and `Pose` already carries a **named refusal** for this exact path.

## What is there and what is not, measured rather than recalled

| | |
|---|---|
| `AnimationPath::Weights` | **parsed already.** The reader carries the channel path and `Track` divides the run by the target count, so the animation half of this row was built when animations were |
| `mesh.weights` and `primitive.targets` | **absent.** Nothing reads either, so there is nothing for a weight to weigh |
| `Pose::Build` | **refuses it by name**: *animation channel targets the morph weights of node N, and a pose writes node transforms only.* That refusal is correct today and is what this task exists to retire |

**`AnimatedMorphCube` at the pin**, read here: one node, one mesh, one primitive of 24 vertices, **two
morph targets each carrying POSITION, NORMAL and TANGENT deltas**, `mesh.weights [0, 0]` as the rest
pose, and one animation channel — node 0, path `weights`, **LINEAR, 127 keys, output SCALAR × 254**,
which is 127 × 2 and is the format's rule that a weights keyframe carries one value per target.

## The oracle's half, and it is a THIRD arm rather than a variant of the two that exist

[MEASURED] Blender 5.2.0 importing `AnimatedMorphCube`:

```
OBJ AnimatedMorphCube  type=MESH   shape_keys: ['Basis', 'thin', 'angle']
    shape-key animation_data action: Square
      fcurve key_blocks["thin"].value    keys=127 interp=LINEAR
      fcurve key_blocks["angle"].value   keys=127 interp=LINEAR
    object animation_data: None
```

**The curves live on the mesh's shape-key datablock**, not on the object and not on a pose bone — so
`board:1200`'s dispatch has a third case and `board:1198`'s baker reaches neither of the two it knows.

**And this arm can be the FULL one, which the bone arm could not be.** A morph weight is a **scalar with
no space conversion**: the importer stores the file's own number, so the preparer can evaluate the file's
accessor directly and cover `STEP`, `LINEAR` and `CUBICSPLINE` alike. *The bone arm was narrowed because
a pose bone is rest-relative in axes glTF never states; a weight has neither property.*

- [ ] **The target-to-shape-key mapping is POSITIONAL and is checked, not trusted.** `key_blocks[0]` is
  `Basis` and target *i* is `key_blocks[i + 1]`. **The file declares no `extras.targetNames`**, so the
  names Blender shows are its own invention and cannot be the join. The witness check `board:1198`
  already applies — our derived first key against the importer's — and a mapping that slipped by one
  shows there as an O(1) disagreement

## Done when

- [ ] `mesh.weights` and `primitive.targets[]` are read, with the semantics the format allows in a
  target — `POSITION`, `NORMAL`, `TANGENT` and nothing else — and a target whose accessor count differs
  from the base attribute's is refused by name
- [ ] **`Pose` carries per-node morph weights and stops refusing the `weights` path.** The refusal is
  deleted with its cause, not left beside what replaces it
- [ ] The flatten adds the deltas before the node transform and before the skin: `p + Σ w_i · d_i`, and
  the same for `NORMAL` and `TANGENT`, which this asset carries on **both** the base and every target
- [ ] The baker drives shape keys, evaluating the file for all three interpolation modes
- [ ] **`AnimatedMorphCube` is a render case, frame by frame**, and it is the proof
- [ ] A deliberately dropped target is shown red

## The risk this case carries into an open bug

**The asset morphs `TANGENT`, and `board:1126` is open on exactly that surface** — *our shading normal
disagrees with the file on the tangent assets*. So a red here may be the morph or may be the tangent
frame, and **the caveat is named before the measurement rather than after it**: if this case fails, the
first question is whether it fails on the same channels `NormalTangentTest` already does, and the
comparison is against that case's residual and not against zero.
