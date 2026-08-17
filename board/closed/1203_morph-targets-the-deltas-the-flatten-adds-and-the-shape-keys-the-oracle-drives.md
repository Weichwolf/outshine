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

- [x] **The target-to-shape-key mapping is POSITIONAL and is checked, not trusted.** `key_blocks[0]` is
  `Basis` and target *i* is `key_blocks[i + 1]`. **The file declares no `extras.targetNames`**, so the
  names Blender shows are its own invention and cannot be the join. The witness check `board:1198`
  already applies — our derived first key against the importer's — and a mapping that slipped by one
  shows there as an O(1) disagreement

## Done when

- [x] `mesh.weights` and `primitive.targets[]` are read, with the semantics the format allows in a
  target — `POSITION`, `NORMAL`, `TANGENT` and nothing else — and a target whose accessor count differs
  from the base attribute's is refused by name
- [x] **`Pose` carries per-node morph weights and stops refusing the `weights` path.** The refusal is
  deleted with its cause, not left beside what replaces it
- [x] The flatten adds the deltas before the node transform and before the skin: `p + Σ w_i · d_i`, and
  the same for `NORMAL` and `TANGENT`, which this asset carries on **both** the base and every target
- [x] The baker drives shape keys, evaluating the file for all three interpolation modes
- [x] **`AnimatedMorphCube` is a render case, frame by frame** — three arms green over 18 frames, **worst geometric disagreement 0.00099526122 px against the 0.005 px instrument floor**, the morph moving the drawn subject **53.6790192 px** over the grid, 458 checks and no failures
- [x] A deliberately dropped target is shown red — decoding the deltas and discarding them takes the case red, and takes the unit test red too

## The risk this case carries into an open bug

**The asset morphs `TANGENT`, and `board:1126` is open on exactly that surface** — *our shading normal
disagrees with the file on the tangent assets*. So a red here may be the morph or may be the tangent
frame, and **the caveat is named before the measurement rather than after it**: if this case fails, the
first question is whether it fails on the same channels `NormalTangentTest` already does, and the
comparison is against that case's residual and not against zero.

## Three defects this case found that were not about morphing

**The sampler width rule was stated too narrowly and refused every morph animation.** `ReadAnimations`
held `values.Count == times.Count * perKeyframe`, which is right for a TRS path where one element is a
whole VEC3 — and wrong for `weights`, whose output is SCALAR carrying one value **per target** per
keyframe. 127 keyframes and two targets is 254, and the reader called that a malformed file. **A sampler
does not know its path**, so what it can decide is that the run divides; which multiple is right is a
question about the mesh the CHANNEL names, and `Pose::Build` answers it where the mesh is in scope.

**Blender renamed the asset's material and the manifest stopped matching it.** The factory startup carries
a datablock called `Material`, `clear_objects` removed only the OBJECTS, and the importer brought the
file's own material in as `Material.001`. It surfaced as a refusal about a declared colour rather than
about a collision, and it would hit **any** corpus asset whose material is called `Material`. The scene
reset now purges the datablock collections, not just the objects.

**And the camera was hand-derived when the engine owns the rule.** The first declaration came from a
Python reimplementation of `Framing.h` and disagreed with the engine in the seventh digit, which
`ADerivedCameraIsTheFramingRuleAndNotAQuotation` caught at the libm floor — *which is exactly what that
test is for*. The manifest now quotes the engine's own bounds and eye.

## The hazard I wrote a comment against and then walked into

`Subject::Build` had a two-argument posed overload and a three-argument one that added the weights, with
a comment saying the pair *is not an invitation to pass one without the other*. **The runner then passed
one without the other**, every frame reached the flatten with the mesh's rest weights, our subject stood
still against an oracle that morphed, and nothing failed to compile — the case failed on
`worst_disagreement_px` at 3.267 px with a motion of 0 m.

**The two-argument overload is deleted.** An empty weight run is still legal and still means *the file's
own weights*, but it has to be **said**. *A comment forbidding a call is worth less than not having the
call to make* — which is `CLAUDE.md`'s own preference for the shape that makes a mistake unspellable, and
this is the second time this round that the shape was available and the comment was chosen instead.
