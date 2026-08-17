Type: feature
Area: corpus
Tags: khronos, core

**SimpleSparseAccessor is green on both counts**

*Simple Sparse Accessor* -- tagged `core`, `testing` at the pin, published as `glTF`, `glTF-Embedded`.

**Criteria met and the picture within the bound**, on the run this item was written from.

| case | criteria | picture bound | failing metrics |
|---|---|---|---|
| `test/khronos/glTF/SimpleSparseAccessor` | met | within | none |

`board:0079` records sparse accessors as BUILT and outside the extension count; this is the case that turns that record into a picture. A reader ignoring the sparse block would draw the flat base mesh, whose silhouette differs over most of the boundary rather than at a sub-pixel offset.

**It is DIFFUSE under the factory world rather than an emission, and that was not a preference.** The
per-node emission arm refused it -- our side keys a per-node colour by the FILE's node name and this
file names none, while the oracle answers to Blender's synthesised `Mesh_0`. `board:1362` carries that
defect; here it is only the reason this case declares one colour and takes its light from the world,
which is the arrangement `Triangle` already runs green.
