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
