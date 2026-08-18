Type: task
Parent: 0079
Area: gltf
Tags: khronos

**A node may place its mesh many times**

`EXT_mesh_gpu_instancing` puts a translation, a rotation and a scale accessor on a mesh node and says
each names one instance: *an object space transform that should be multiplied by the node's world
transform*. Unread, the node draws ONE body where the file says there are many -- **a wrong picture and
not a plainer one**, which is why an optional extension is read here.

## What it bought, and it is the largest single disagreement the corpus had

| `SimpleInstancing` | before | after |
|---|---|---|
| `worst_disagreement_px` | **494.56683** | **0** |
| criteria | 0 of 1 | **1 of 1** |
| picture bound | outside | **within** |

**Exactly zero, not merely small.** 125 boxes land on the oracle's own pixels.

## Three things it needed that were not the extension

**THE CAMERA HAD TO BE RE-DERIVED AND IT IS THE RULE'S OUTPUT, NOT A CHOICE.** The declared camera
framed a subject of radius 0.87 m because that is all this engine drew; the real subject is 12.47 m and
the old eye put 124 instances behind the near plane -- [MEASURED] *vertex 220 sits -0.964029 m along
the view axis, inside the engine's fixed near plane of 0.050000 m*. Every number of the replacement is
`Framing.h`'s over the new bounds, so `ADerivedCameraIsTheFramingRuleAndNotAQuotation` still holds.

**THE COUNT IS THE FILE'S AND A DISAGREEMENT IS A REFUSAL.** The extension states *all attribute
accessors in a given node must have the same count*, so a file that disagrees with itself has no
instance count and is refused rather than drawn at whichever run happened to be read first.

**A MISSING ATTRIBUTE IS ITS IDENTITY.** The three are independently optional, so a file giving
translations alone instances at the rest rotation and scale.

## What this is NOT, said because the names collide

**It is an EXPANSION and not GPU instancing.** What it produces is N parts sharing one mesh's vertices
through their own transforms; a draw list that batches them into one call is the compositor's business,
and the extension's own note agrees -- *GPU instancing and other optimizations are possible, and
encouraged, even without this extension*.

**The children of an instanced node are NOT instanced.** The extension applies to a node's mesh and is
silent about a subtree; a reading that multiplied the subtree would invent a behaviour the format does
not define.
