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
