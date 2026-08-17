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
