# Native near-tree admission — #1

On native6c9378b3b, compare the existing birch prototype directly as fine mesh,
coarse mesh(rank3) and the existing256²/eight-view whole-tree card. Same640×720
camera at35m, same tree/22m height and direct light, black background. All three
actual images were opened. Geometry removes magnified card pixels, but the old
crown remains sparse. This isolated direct-light stage does not measure world
ambient illumination, forest quality or runtime/frame acceptance.

The card uses the same current native prototype, not the Python research tree.
A suspected duplicate-placement explanation is rejected: birch has52766 unique
origins for52766 leaf placements; spruce140251 for140254. Three coincidences
cannot explain the widespread missing apparent coverage.

## Actual source counts, not a frame benchmark

| Tree/rank | Bark triangles | Blade triangles | Leaf placements | Submitted triangles |
|---|---:|---:|---:|---:|
| Birch0 |107560|112|52766|6017352|
| Birch3 |2158|20|52766|1057478|
| Spruce0 |161340|66|140254|9418104|
| Spruce3 |1964|66|140254|9258728|

Submitted triangles = bark + blade × placements. Instancing shares prototype
arrays; it does not eliminate those vertex/triangle references. Birch0 has
3016064B logical geometry arrays plus6754048B Float64 placement matrices;
spruce0 has4526312B plus17952512B. These are logical CPU payloads, not RSS or
physical VRAM. Rank changes preserve every leaf placement; the current needle
blade does not simplify between ranks. No blanket activation of these meshes
in the world follows. No cheaper same-quality winner or global optimum claimed.

The narrow replacement entry is the checked Python PlantPlan contract:
summary(path) before detail, leaves(path,max_leaves), stable leaf IDs/variants,
owner axes, local origins and right-handed frames; canonical blade prototypes
remain separate. Full/partial table semantics are in lib/README.md and
plant_tables.py. The compact fixed whole-plant handoff is documented in
../vegetation/tree_entry.md (#367/#374). It is a technical young-plant reference,
not an approved mature species catalogue. World placement/reservations and
instance identity remain native integration responsibilities.

Original native source, commands, logs and the three images remain in
`/tmp/outshine-foliage-overlap-6c9378b3b/`. The private probe reuses the existing
tree-representation-cost counts and the earlier isolated tree-render setup.
No product algorithm, cache or shader changed; no new Places image published.
