Type: bug
Area: corpus
Tags: oracle, instrument, khronos

**A per-node colour is keyed by two different names, so a file that names nothing cannot be compared**

[MEASURED] `SimpleMeshes`, authored as a case and then withdrawn rather than committed half-built:

| side | what it keys a per-node colour by |
|---|---|
| the oracle | **Blender's synthesised object name.** Two nodes sharing one unnamed mesh become `Mesh_0` and `Mesh_0.001` -- the MESH index plus Blender's collision counter, never the node index |
| ours | **the file's own node name**, and the refusal is explicit: `the subject's part 0 carries no node name, so a per-node colour has nothing to key on`, verdict `NOTHING-TO-COMPARE` |

**Both are defensible in isolation and together they cannot compare a file that names nothing.** The
oracle answers to a name the file never contained; we answer only to one it does.

## Why the two escapes do not exist either

| material kind | why it is unavailable here |
|---|---|
| `emission-per-material` | keys by MATERIAL name. `SimpleMeshes` declares no materials at all, and `BoxInterleaved` and `SimpleMaterial` declare one that is unnamed |
| `diffuse` | needs no key and the preparer refuses it: `material.kind is diffuse over 2 meshes, and the closed form holds for a single unoccluded facet`. **That refusal is correct** -- `rho*L` is exact only where no surface can see another |

**So a multi-body subject whose file names nothing has no spelling in this vocabulary.** It is not a
gap in one model: unnamed nodes are ordinary in the index, and this will meet the remaining 114 models
repeatedly.

## What is NOT the repair

**Patching the asset to add node names.** The ladder allows it -- *fix the engine, reduce the oracle,
patch the asset, disqualify* -- but it is the third rung and this is a first-rung defect: **our side
could key the way the oracle does, or the manifest could key by node INDEX, which both files carry
whether or not they name anything.** A patch here would put a correction in every affected case's
directory to work around one missing rule.

## The candidates, and the second is cheaper than it looks

- [ ] **Key by node index.** `colourLinearPerNode` becomes a map whose key may be an index, and both
  sides resolve it from the file's own `nodes` array. *The oracle's importer preserves import order,
  which is what makes this resolvable there -- and that is a claim to MEASURE rather than assume*
- [ ] **Mirror the oracle's naming on our side.** Our subject's parts take `Mesh_<meshIndex>` with a
  collision counter when the file names nothing. **Cheaper to write and worse to own**: it makes this
  tree's identity for a part a copy of another program's naming convention, which drifts at their next
  release and fails silently -- the two sides would simply colour different bodies

**The recommendation is the index**, because it is a fact about the FILE and neither side has to agree
with the other about a string.

## What this blocks today

`SimpleMeshes` -- *two nodes, one mesh*, which is the smallest asset in the index that draws one mesh
twice. **Its case was written, refused, and withdrawn rather than committed as a red that cannot run**:
half-built is worse than not built, and a `NOTHING-TO-COMPARE` in the corpus is a case that reports on
nothing while counting as one.

## The design, and the one measurement it turns on

**Our side is easy and it was checked rather than assumed.** `Part` carries `NodeName` and no index;
adding `int Node` is safe, because `Emit` writes **one node per part in part order**
(`{"mesh": <part>}`) and `EmittingASubjectIsAFixedPointOfTheFlatten` compares the fields it NAMES rather
than the struct, so a new member neither breaks the round trip nor is silently ignored by it. The key
rule is then one sentence: **the file's node name where it has one, the decimal node index where it does
not** -- and both are facts about the file.

**The oracle side is the whole difficulty and it reduces to one question.** Blender's importer gives an
object no glTF node index; it gives it a name derived from the MESH. The preparer already parses the
glTF, so it can walk the scene and pair mesh-bearing nodes with imported objects -- **but pairing by
order is a claim, not a given**.

- [ ] **MEASURE: does Blender 5.2.0 create objects in glTF node traversal order?** It logs
  `Blender create Mesh node 0`, which suggests it, and *suggests* is what this tree does not build on.
  **The witness is free**: on every case whose file DOES name its nodes, the paired object's name must
  carry that name. If the pairing holds wherever it is checkable, it is used where it is not; if it
  fails anywhere, the whole approach goes and the index cannot be recovered on that side at all.

**That measurement is the next step and it is small.** Until it is made, the design above is a plan and
not a repair, which is why nothing was written into the tree for it this round.

## What it costs to leave open, stated so the choice is visible

**Only files that are BOTH multi-body AND name nothing.** [MEASURED] over the batch examined this
session: `Suzanne`, `Cube`, `TwoSidedPlane` are single-body; `MetalRoughSpheresNoTextures` has 102
bodies and names all 119 of its nodes; `VertexColorTest` names both of its. **`SimpleMeshes` was the
unlucky one**, and the class is narrower than it first looked -- which is why the corpus can go on
being populated with this open.

## Comments

**The design this item assumed was refuted by measuring, and the measurement made it small.** The plan
was to key colours by the glTF material INDEX and resolve index to Blender slot by walking the file's
node hierarchy, with the named materials as a witness against the ordering assumption. None of that is
needed. [MEASURED], Blender 5.2's glTF importer names an unnamed material **`Material_<glTF index>`**:
`MetalRoughSpheres` (one unnamed material) arrives as `Material_0`, `TextureEncodingTest` (fourteen) as
`Material_0` .. `Material_13`. **The index is already in the string**, so both sides can key on one name
with no hierarchy walk and no ordering assumption at all.

**The hazard I filed this against does not exist either, and the reason is the same fact.** Two distinct
unnamed materials cannot collide under the `.001` duplicate-suffix stripping -- which would have painted
one of them the other's colour silently -- because they never share a stem.

**One way the key can still lie, and it is refused rather than documented**: a file carrying both an
unnamed material and a named one spelled `Material_<n>`. Our side names that collision and refuses.

**A table stops being a declaration somewhere between 14 and 344 materials.** `IridescenceMetallicSpheres`
draws 344 and `NodePerformanceTest` 10 000, where a colour table is a transcription of the file rather
than a statement about the case -- so `emission-by-material-index` takes a RULE with no free parameter
instead, and a manifest cannot tune a colour to make a case pass.
