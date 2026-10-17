# Demanded native spatial foliage — #1

PlantCrownPlan transfers the [mature scaffold](../vegetation/oak_scaffold.md)'s
existing CrownShoots procedure. It owns the moved growth skeleton, original
variant rows and immutable recipe. It indexes leaf-bearing nodes before any
poses or blade meshes. No fixed-depth recursive path limits the spatial tree.

An O(nodes) reverse pass computes distance to the nearest descendant tip.
Nodes within the authored foliage-depth band become hosts, in original node-ID
order; the root is excluded. For L leaves and H hosts, host i receives
`L/H + (i < L%H)` leaves, starting at `i*(L/H) + min(i,L%H)`.
No full leaf-position table or per-host offsets/count arrays are required.
Queries and summaries are const; a query's random phase depends only on
recipe seed XOR original host ID. Request order changes no source identity.

MeasureLeaves checks host/count without allocation. Leaves refuses insufficient
demand before pose allocation, then emits exactly one requested host's rows.
PlantCrownLeaf.OwnerNode is an original growth-node ID, explicitly distinct from
the recursive PlantLeaf.Axis convention. Shared PlantLeafPose owns only pose
values; consumers must bind these to actual shoot products, not reinterpret IDs.

## Shared mathematics and memory

PlantFrame and PlantShoot share the existing normal-frame fallback, circular
shoot frame, azimuth/inclination, normal alignment and petiole calculation with
the old recipe planner. PlantSkeleton shares rooted ordered-parent, coordinate
and noncollapsed-edge validation with native growth. Old recipe behavior remains
checked independently, including all five phyllotaxis arrangements.

Construction requires a declared additional index budget before allocating.
It requests two node arrays:8N bytes of temporary tip distances and4N bytes of
host capacity. Only the host array survives construction. Original growth data
and variant rows are moved, not copied. These figures exclude existing source
capacity, allocator overhead, object headers, stack and process RSS.

| Actual18m authored study, seed682 | Measured inventory |
|---|---:|
| Skeleton nodes / tips / foliage hosts |5472/1301/4301|
| Original leaf rows / variants |90000/8|
| Additional requested index-build bytes |65664|
| Retained host-index array bytes |21888|
| Largest requested host group |21rows/3024B|
| Full selected90k pose expansion, separate output |12960000B|

Thus a summary or one host does not materialize the full12.36MiB pose table.
On this standard library, allocation tracking verifies two index allocations,
zero summary/measurement/refusal allocations and one exactly sized array for a
successful selected group. Output arrays belong to the caller; no new mutable
cache, database, serialized admission registry or world integration is added.

## Independent checks

```sh
JOBS=4 sh test/run.sh outshine/src/generators/flora/PlantCrownPlan \
  outshine/src/generators/flora/PlantGrowth \
  outshine/src/generators/flora/PlantRecipe \
  outshine/src/generators/flora/PlantRecipeJson
PYTHONPATH=lib/src:test/experiments/vegetation python \
  test/experiments/render/native_plant_crown_reference.py \
  --binary /tmp/<native-consumer> --growth-cohort /tmp/<growth-cohort> \
  --output /tmp/<fresh-crown-cohort>
```

The growth cohort is produced by [native_plant_growth_reference.py](native_plant_growth.md).
Twenty actual crown cases cover forks at three depth bands, empty/small/large
inventories, all64 variant IDs, a70-node chain/high64-bit seed, and the four
full mature-study skeletons. One complete90k source is requested in reverse host
order, then repeated and underbudget/unknown requests are added. IDs, original
owners, variants, hosts, counts and refusal behavior match Python exactly.
Maximum measured position difference3.552713678800501e-15m and frame-component
difference2.0212997942081756e-14. Declared finite comparison tolerances are1e-9m
and1e-12 respectively; neither is a surface, screen-space, radiance or wind bound.
Shoot.detail is forbidden during the Python summary, then enabled for independent
pose comparison. Existing35typed+35JSON recipe comparisons still pass unchanged.

Thirty module arms pass normally, with ASan/UBSan and container hardening.
Six changed/dependent product units and the independent consumer pass clang-tidy
with full build flags. Case analysis retains only shared Check.h/libc++ findings
(#603), without suppression. No oracle, existing tolerance or budget is increased.
One local component run measures index construction0.084ms and summed pose
expansion7.781ms for90020requested rows, including a repeated host; stream output
is outside those component timers. These are individual observations, not p99.

## Actual Metal control and remaining gaps

Two640×720 Metal foliage-only images were actually rendered and opened. One uses
native demanded groups, the other independently supplied original Python poses;
the same eight14×7cm maximum physical Quercus blades use8×1 mesh detail, shared
qualitative diffuse reflection/transmission, fixed lights/exposure and camera.
Both PNGs have SHA256
`6100f6017341e2084e983e28b842150fce7b9c2978dd10141828946a3b858845`.

The matched picture proves this finite native placement path preserves the
rendered foliage. It looks dark and perforated at this scale, with a generic
ellipsoid envelope. It has no wood, terrain or world context and is not a new
plant/art/lighting acceptance. Its90000 instances imply3060000 triangle references;
that literal whole-leaf path is a diagnostic, not an acceptable blanket runtime
LOD. Next integrate spatially selected shared products, complete branch surfaces,
near/far foliage representation and coherent material/light response. Existing
world trees/Places and their cache products remain unchanged.

Original inputs, paired native/Python rows, commands, source/binary receipts,
terminal logs and images remain under
`/tmp/outshine-native-plant-recipe-39bf9e07d/crown/`.
