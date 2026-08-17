Type: issue
Area: corpus
Tags: scope, khronos

**The generated render corpus duplicates Khronos, except where it does not**

**The owner:** `test/outshine/` needs no render tests as far as they are concerned -- the Khronos cases
cover it. **Mostly true, and the exceptions are the point of writing it down.**

## What Khronos does cover, and where the generated case can go once its counterpart is green

| generated case | covered by |
|---|---|
| `triangle` | `Triangle` -- **green today** |
| `cube`, `matrix-node` | `Box` -- **green today**, and its root node IS a matrix |
| `orthographic-camera`, `perspective-camera` | `Cameras`, two cases -- **green today** |
| `quad`, `trs-hierarchy` | `SimpleMeshes` and `BoxAnimated`, partly |
| `primitive-modes` | `MeshPrimitiveModes` -- **no case yet** |
| `sphere` | `MetalRoughSpheres` -- **no case yet** |
| `index-widths` | **nothing.** No model in the index declares its index component width as its subject |

**The recommendation is ORDER and not refusal**: a duplicated case goes when its counterpart is green,
never before, or there is a window in which neither covers the ground.

## What Khronos cannot cover, by construction

| | |
|---|---|
| **`beech`** | a part **this engine's generator grew**. No model in the corpus comes from a generator, so this is the only picture that says the generator layer draws something correct. `CLAUDE.md` gives generators their own layer and their own instrument; deleting this leaves that layer with unit tests and no picture |
| **`shaded-sphere`, `shaded-sphere-black`, `shaded-sphere-smooth`** | the instruments that reduced nine red cases to one term (`board:1363`). **Every fetched asset that carries a material carries textures, several materials and a light**; not one model in the index isolates a single BRDF term. Without these the diffuse finding was not measurable and would not have been found |

## The decision is the owner's and this records what it costs

- [ ] **Take it in full** -- `test/outshine/render/` goes entire. Cost: the generator layer loses its
  only picture, and the shading arm loses the only subjects that can isolate a term. Both are then
  claims held by unit tests, which `CLAUDE.md` says is exactly what a render case may not be replaced by
- [ ] **Take it as ordered** *(recommended)* -- the seven duplicated cases go as their counterparts turn
  green; `beech`, the three spheres and `index-widths` stay
- [ ] **Take it later** -- nothing goes until the corpus is populated, on the grounds that a case which
  costs nothing to keep is cheaper than a coverage gap discovered at 148

**Filed and worked around, never waited on**: nothing here blocks, and the generated cases cost a
preparation each.
