Type: feature
Area: corpus
Tags: khronos, core

**SimpleMaterial is green on both counts**

*Simple Material* -- tagged `core`, `testing`, `written` at the pin, published as `glTF`, `glTF-Embedded`.

**Criteria met and the picture within the bound**, on the run this item was written from.

| case | criteria | picture bound | failing metrics |
|---|---|---|---|
| `test/khronos/glTF/SimpleMaterial` | met | within | none |

The smallest asset in the index that declares a material at all: POSITION and nothing else, one unnamed material carrying a `baseColorFactor`.

**It is DIFFUSE under the factory world rather than an emission, and that was not a preference.** The
per-node emission arm refused it -- our side keys a per-node colour by the FILE's node name and this
file names none, while the oracle answers to Blender's synthesised `Mesh_0`. `board:1362` carries that
defect; here it is only the reason this case declares one colour and takes its light from the world,
which is the arrangement `Triangle` already runs green.
