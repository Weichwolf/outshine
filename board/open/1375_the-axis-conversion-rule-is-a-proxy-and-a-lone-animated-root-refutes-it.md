Type: bug
Area: corpus
Tags: oracle, instrument, khronos

**The axis-conversion rule is a proxy, and a lone animated root refutes it**

The baker converts an animated channel from glTF's space into Blender's by the rule **roots are
converted, children are not**, spelled `rooted = obj.parent is None`. Twenty animated cases were authored
and all twenty refused on the witness check that guards it:

```
AnimatedCube's rotation at the importer's first key is (1.0, 0.0, 0.0, 0.0)
and the same key derived from the file is (0.7071067690849304, 0.7071067690849304, 0.0, 0.0),
apart by 0.7071067690849304
```

**The check did exactly what it exists for.** It refused rather than baking every key in the wrong space
and producing a plausible picture nobody could attribute. *This item is a defect report about the rule,
not about the guard.*

## What is measured

| | |
|---|---|
| `AnimatedCube`'s first keyframe, read from its own buffer | `(0, 0, 0, 1)` in glTF's `(x, y, z, w)` -- **identity** |
| what the importer put at that key | `(1, 0, 0, 0)` in Blender's `(w, x, y, z)` -- **identity** |
| what the rule derived | the same key with the +90 degree X conversion applied |

**So the importer left this root's CURVE in glTF's own space** and put the conversion somewhere else.

## And the rule is not simply wrong, which is what makes it a measurement rather than a guess

[MEASURED] which animated nodes are roots, read from each file's own `children` arrays:

| model | animated nodes | of which roots | its case |
|---|---|---|---|
| `BoxAnimated` | 0, 2 | **0** | **green** |
| `AnimatedMorphCube` | 0 | **0** | **green** |
| `RiggedSimple` | 4 | none | green |
| `AnimatedCube` | 0 | **0** | **refuses** |
| `AnimatedTriangle` | 0 | **0** | refuses |

**Three models animate a root; two of them pass and one does not.** So `obj.parent is None` is a PROXY
for something else, and the difference between the passing and the failing cases is what has to be
measured before the rule is rewritten.

- [ ] **The candidate, named and NOT established**: `AnimatedCube` is a lone root -- one node, a mesh, no
  children -- where `BoxAnimated`'s animated root has children and may therefore arrive under an empty
  the importer created to carry the conversion. *If that is it, `obj.parent is None` is true for both and
  the discriminator is elsewhere; the honest next step is to print what the importer builds for each and
  compare, rather than to reason about it further.*

**Twenty cases wait on this** -- every animated or skinned model without one -- and they are **withheld
rather than committed unprepared**: a case that cannot prepare reports nothing and would make the suite's
red set harder to read, which is the opposite of what a case is for.

## MEASURED: the conversion is per PATH, and the root branch had never been refutable

**The importer converts a root's TRANSLATION curve and does not convert its ROTATION curve.** Both halves
are measured on files already in the corpus:

| | file says | importer writes | |
|---|---|---|---|
| `BoxAnimated` node 0, a root, `translation` | keys along glTF's **+Y**: `(0, 2.52, 0)` | keys along Blender's **+Z**, axis 2 | **converted** |
| `AnimatedCube` node 0, a root, `rotation` | first key `(0, 0, 0, 1)` — identity | `(1, 0, 0, 0)` — identity in Blender's order | **not converted** |

**And here is why nothing caught it for so long.** `BoxAnimated` is the only case that ever exercised the
root branch, and it animates a root on `translation` **whose first key is the zero vector** — invariant
under any rotation. Its ROTATION channel is on node 2, a **child**, where no conversion is applied. *So
the root branch ran on every animated case and could not be refuted by any of them: the one value it saw
was the one value the conversion cannot move.* `AnimatedCube` is the first subject with a rotation on a
root, and it refuted it on the first attempt.

**`BoxAnimated` still passes after the change**, which is what says the translation branch was not broken
in passing.

## Two more measurements the same push forced

**An unnamed MESH gives `Mesh_<meshIndex>`, not the node's index.** [MEASURED] `AnimatedTriangle`: one
node, one mesh, neither named, and the importer builds `Mesh_0`. The baker looked for `Node_0`, found
nothing, and **refused rather than dropping the channel** — the guard working again. `BoxAnimated` keeps
the other branch honest: its `Node_0` and `Node_1` carry no mesh and are named by node index.

**The oracle now gives the format's default material a datablock**, because an empty Blender slot cannot
hold an emitter — **and the manifest decides whether there is one.** Filling every empty slot
unconditionally created a `<default>` on `RiggedSimple`, whose file names a material for every primitive,
and **turned a green case red**. The manifest states what the subject carries and our side derives the
same from the file's own primitives, so the two agree by construction rather than by Blender's slot
count.

## Eleven of the twenty animated cases came in; nine wait on two named classes

**After the three measurements above**: `criteria 98 met of 104 · 76 within` became
**`criteria 104 met of 110 · 81 within`**, and the animated refusals fell from twenty to nine.

**What the nine wait on, measured rather than guessed:**

| | |
|---|---|
| **skinned animation** — `BrainStem`, `CesiumMilkTruck`, `RiggedFigure` and their kind | `the glTF names an animated node 'Node_2' and no imported object carries that name`. Their animated nodes are **joints inside an armature**, not objects, so an object lookup by name cannot reach them. `_bone_curves` exists for exactly this and the dispatch does not take it here |
| **a mesh bound by several nodes** — `InterpolationTest` and its kind | `Cube.001 is animated by the file and carries no channelbag to write into`. Nine cubes share one mesh, so Blender builds `Cube`, `Cube.001`, … and a name lookup sends every channel to the first. **This is `board:1362` in its animated form** |

**The nine are withheld rather than committed unprepared**, for the reason the earlier twenty were: a
case that cannot prepare reports nothing and makes the red set harder to read.

## Comments

**The skinned half is fixed, and the rule that blocked it was MY OWN, measured on one shape.**
`board:1375` recorded skinned animation as blocked because *animated nodes are joints inside an
armature, not objects*. The preparer already had a bone arm for exactly that. What refused was a
redirect I had added beside it: *a skin's `skeleton` node is the armature OBJECT*.

**Both shapes occur and I had measured only one.**

| asset | `skin.skeleton` | is it also a joint? | where the importer put the curves |
|---|---|---|---|
| `BrainStem` | node 2 | **no** | the armature OBJECT -- the redirect is right |
| `CesiumMan` | node 3 | **yes, `joints[0]`** | 190 curves on POSE BONES, **0 on the object** |

So on `CesiumMan` the redirect sent the channel to an object with no curves and the case refused with
*Armature's location is 0 curves and the file's translation carries 3 components* -- a message that
reads as a defect in the asset and was a defect in the rule. **The joint wins**, because a joint is
where the importer put the curves; the object redirect applies only where the skeleton node is not
itself a joint.

**`CesiumMan` prepares.** The five other cases that refused with the same Blender exit status are
re-prepared in the same round to find out how many shared the cause rather than the symptom -- *the
caveat first*: identical exit statuses are not evidence of an identical cause.

**One case still refuses here and it is a THIRD shape, not the two above.** `MeshoptCubeTest`:

```
Cube_4_animated_rotation's rotation at the importer's first key is (1.0, 0.0, 0.0, 0.0),
and no convention this preparer knows derives it from the file
```

**The importer's first key is the identity quaternion**, which every candidate conversion should
reproduce -- an identity is fixed by all of them -- so *none matching* is the one outcome the
derivation should not be able to reach. Either the key being compared is not the key the file's first
sample describes (a rest pose, or a frame the importer inserted), or `_agrees` is comparing a
quaternion against something in a different order. **It is not the axis question this item was opened
about**, and it is written down here rather than folded into it.

**The identity quaternion was an undone division, and the shape of the finding is the point.**
`MeshoptCubeTest` stores its rotation channel as `short normalized`. `_accessor` returned the RAW
integers, so the importer's `(1, 0, 0, 0)` was held against a file value of `(32767, 0, 0, 0)` and no
axis convention could derive one from the other -- **the message said *a frame this preparer does not
know* and the cause was arithmetic that had not run.**

**It was silent everywhere else.** Any sampler whose output is a normalised integer accessor was baked
at 127, 255, 32767 or 65535 times its value; only a quaternion is absurd enough at that scale to
refuse rather than to render a wrong pose. [MEASURED] exactly one model at the pin is affected, so no
other oracle moves -- which is luck about the corpus and not a property of the code.

**`byteStride` was wrong in the same loop and is fixed in the same round**: a view interleaving several
attributes states the step between elements, and stepping by the element's own width reads the next
attribute's bytes as this one's. No case that reached this function was interleaved, so it cost
nothing -- again luck rather than a property.

**`MeshoptCubeTest` is green at 83 checks and within the bound at every frame**, carrying
`KHR_mesh_quantization`, `COLOR_0` and this animation together.

**A correction I owed within the hour: the animation binding was written as a precondition and it
refused twelve cases that had been preparing for months.** `Fox`, `BrainStem`, `CesiumMan`,
`RiggedFigure`, `BoxAnimated`, `SimpleMorph` and their kind name their animations NOTHING, and never
needed an action lookup at all -- Blender had already bound the one animation each of them declares.

**I tested `InterpolationTest` and shipped the class.** The full re-preparation is what caught it, 18
cases refusing where 4 had before. The binding is now a REPAIR: it runs only where an object the
animation drives carries no curves, and a name is required only there.

**Fourth correction in the same family, and each was narrower than the last.** The repair must not
fire for the two arms that never used an object's channelbag:

| a channel's route to its curves | who repairs it |
|---|---|
| translation/rotation/scale on an ordinary node -> the OBJECT's channelbag | this repair |
| the same on a skin's joint -> a POSE BONE's | nobody: `bpy.data.objects` holds no object of that name, so the lookup already excludes it |
| `weights` -> the mesh's SHAPE KEY datablock | nobody, and a slot there reports `target_id_type` `KEY` |

`SimpleMorph` is the third row: its object legitimately carries no channelbag, the repair read that as
a missing binding, and the file names its animation nothing -- **so a case that needed no repair at all
was refused for lacking the name a repair would have wanted.**

**[MEASURED] before the next full pass rather than during it: no file at the pin carries several
animations of which any is unnamed and drives a pose**, so the remaining refusal path is unreachable
by this corpus. *Three passes were spent discovering these one at a time because the preparer was
edited while a pass was running -- a measurement you reach into is not a measurement.*
