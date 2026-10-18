# Stored spatial wood — #1 / #10

`PlantLimbAsset` stores the prepared [C1 wood plans](native_plant_limbs.md).
The existing AssetCache owns packages, slices, spatial metadata, lookup and publication;
the generator owns this versioned payload. No additional cache or manifest is added.

The little-endian, fieldwise format contains version/counts, original source-node IDs,
exact knot intervals, cubic spatial controls and prepared radius coefficients.
It stores no vertices, leaf poses or camera-dependent detail choice. For `L` limbs
and `S` intervals, encoded bytes are `16 + 8*L + 148*S`.

Before allocation the decoder bounds counts, encoded bytes and requested resident
arrays. It restores coefficients directly, without growth, path selection, Hermite
construction or PCHIP fitting. It validates finite fields, the prepared knot grid,
unique node IDs, forward derivatives, C0/C1 joins and positive radii at endpoints
and derivative critical points. Malformed/truncated/unsupported products fail whole.
These floating-point checks do not prove a globally welded/nonintersecting tree.
The generator binds the individually prepared limbs to the correct source graph.
Resident limits cover requested arrays and limb/vector headers; allocator metadata,
input package storage, temporary buffers and subsequent meshes are separate costs.

## Shared package and source-free process

The `PlantSpatialCache` integration case publishes PlantCrownAsset and a two-limb
wood set as two slices of one package, with original matching source anchors.
Wood names the crown product as parent; typed request keys bind the complete fixture
recipe. One miss publishes both; the second request hits. A fresh subprocess then
opens the database with no inputs and empty generator callbacks. It restores both,
checks package/offset/parent and source bindings, builds demanded wood and requests
only the selected original leaf group. Crown index-build bytes remain zero.
Normal, ASan/UBSan and hardened arms pass. This is the fixed fork, not an adult-world
cache-load benchmark or a newly registered production generator.

## Original adult source

Link NativePlantLimbReference with PlantLimbAsset.cpp as well as the existing limb
sources. The driver forwards `--roundtrip` to replace each prepared source limb
with its decoded coefficients before meshing:

```sh
PYTHONPATH=lib/src <python> test/experiments/render/native_plant_limb_reference.py \
  --binary /tmp/<limb-reference> --growth-cohort /tmp/<growth-cohort> \
  --output /tmp/<fresh-roundtrip> --roundtrip
```

All1301 original seed682 limbs restore before their195888 triangles are generated.
The entire station/vertex/normal/index output is byte-identical to the direct output
used for the opened Metal controls. The other three source cohorts retain their
existing65-anchor rejection. Nothing is shortened or counted as a successful tree.
One local run under concurrent nice10 checks measured1.49ms for all1301 individual
encode/decode operations and6.34ms surface generation, excluding input/output/cache IO.
This is not a combined-package hit time or p99. A single combined1301-limb/5471-interval
wire payload would occupy `16 + 8*1301 + 148*5471 = 820132B` by the format arithmetic.
World registration, projected demand, coherent foliage, bark, joints and LOD transitions
remain under #1/#23. The production Places are unchanged.
