Type: feature
Area: corpus
Tags: khronos, core

**BoxInterleaved is green on both counts**

*Box with interleaved position and normal attributes* -- tagged `core`, `testing` at the pin, published as `glTF`, `glTF-Binary`, `glTF-Embedded`.

**Criteria met and the picture within the bound**, on the run this item was written from.

| case | criteria | picture bound | failing metrics |
|---|---|---|---|
| `test/khronos/glTF/BoxInterleaved` | met | within | none |

Box's twin, and the case that separates a reader which handles `byteStride` from one that happens to work when the stride equals the element size. Its world bounds and therefore its framing camera are IDENTICAL to Box's -- which is the instrument: a mishandled stride moves a vertex, and a moved vertex moves this picture while Box's stays where it is.

**It is DIFFUSE under the factory world rather than an emission, and that was not a preference.** The
per-node emission arm refused it -- our side keys a per-node colour by the FILE's node name and this
file names none, while the oracle answers to Blender's synthesised `Mesh_0`. `board:1362` carries that
defect; here it is only the reason this case declares one colour and takes its light from the world,
which is the arrangement `Triangle` already runs green.
