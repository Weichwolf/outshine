# Native foliage-plan assets — #1 / #10

PlantCrownAsset stores the [demanded spatial foliage plan](native_plant_crown.md)
through the existing native AssetCache/ResolveAssetRequest service. A request
binds generator/format version and complete persistent parameters before source
acquisition. Only a miss invokes growth and host selection. The generator owns
the payload format; the shared store remains generator agnostic.

The payload contains original Float64 node positions, ordered parent IDs,
unreached source IDs, growth diagnostics/stop reason, physical shoot recipe,
original blade-variant rows and the complete precomputed ascending host list.
Leaf IDs/counts remain implicit in the existing quotient/remainder mapping.
No whole leaf-position table or mesh is persisted in this foliage-plan product.
It is a component of the plant's reusable raw asset, not a complete rendered tree.

## Decode boundary

The versioned little-endian codec reuses BinaryValueArchive. Counts, exact input
length and decoded-array budgets are checked before allocation. Unknown versions,
truncated/trailing data, unsafe sizes, invalid recipes/stops, nonfinite/collapsed
source edges, cyclic/forward parents, unsupported variants and invalid/duplicate/
unordered host IDs return no plan. Counts and the stored host list restore directly;
decode does not call GrowPlant, Create, IndexHosts, Shoot poses or surface builders.

Canonical host selection belongs to the cooking producer. The cache's request,
format and verified payload binding preserve that prepared selection. The codec
checks storage/topology invariants; it does not independently rerun the foliage-
depth algorithm or authenticate arbitrary external bytes. Unknown source inputs
go through Create, not directly into the stored-product path. No admission
registry, certificate, second cache or automatic source refresh is introduced.

Decode requests five arrays: positions, parents, hosts, variants and unreached
IDs. No tip-distance workspace or leaf rows are allocated. IndexBuildBytes is0
on restored plans; retained host capacity is compacted to the stored count.
Growth diagnostics describe the original producer, not current hit-process work.

## Verification

```sh
JOBS=4 sh test/run.sh outshine/src/generators/flora/PlantCrownAsset \
  outshine/src/generators/flora/PlantCrownPlan \
  outshine/integration/content/PlantCrownCache
PYTHONPATH=lib/src:test/experiments/vegetation python \
  test/experiments/render/native_plant_crown_reference.py --roundtrip \
  --binary /tmp/<native-consumer> --growth-cohort /tmp/<growth-cohort> \
  --output /tmp/<fresh-roundtrip>
```

The pure codec module links no SQLite/renderer/world units. The separate cache
integration links only the existing cache/package/hash units, SQLite, Zstandard and the existing Zlib checksum dependency.
Twenty-one arms pass: ordinary, ASan/UBSan and container configurations. Tests
cover exact original data/diagnostic/pose retention, byte-stable re-encoding,
malformed fields, every truncated prefix and allocation-free size refusal.
The small complete decode requests exactly169B in five arrays. A real fresh
child process loads through the shared request key without source inputs or
producer; its index-build counter remains0 before selected near-detail queries.

The independent Python comparison passes20stored-roundtrip and20ordinary cases,
including the full90k source in reverse host order. Original owners, variants,
hosts, counts/refusals and all compared pose values remain unchanged within the
previously declared finite comparison tolerances. These are not new LOD bounds.
Product units and the independent consumer pass full-flag clang-tidy. Existing
shared Check.h/Shell.h/libc++ case findings remain separate (#603).

## Actual complete-source cold/hit observation

An independent private C++ probe reads the original seed682 growth input only
on a miss, performs native growth, creates the crown plan and publishes it.
A second process receives only database/request identity, no source path.

| Scope | Miss | Fresh hit |
|---|---:|---:|
| Producer calls |1|0|
| Growth ms |42.832|0|
| Plan/index stage ms |0.089|0|
| Encode ms |0.101|0|
| ResolveAssetRequest ms |46.558|0.326|
| Native decode ms |0.257|0.249|

The hit's measured resolve+decode is0.326+0.249=0.575ms. Database/process startup,
the separate exhaustive pose verification and renderer startup are outside these
timers. These are individual local component observations under concurrent nice10
work, not p99, cold-disk throughput, total scene load or an A18-Pro device gate.

The5472-node/4301-host/90000-variant-row payload is261060B (about0.249MiB).
Its decoded arrays request260908B; compact host storage is17204B instead of the
original21888B capacity. No90000-entry pose table is resident until explicitly
requested by a consumer. Cold/hit verification subsequently generates all poses
outside the load timers; their stable binary-row SHA256 is identical:
`e47e35e5156c78b77d98a978b19aa8c01759cc5b5fac96bc14ccd44101660c58`.

A fresh Metal probe loads only this cached plan, then requests the declared blade
detail. Its actual opened640×720foliage PNG is byte-identical to the preceding
filled-light native/Python controls:
`9ed49a9a50fc93195ea346bd193b88f059c19585148b153749c76151e7ec685e`.
This preserves the technical portrait; the same perforated crown, qualitative
lighting and3060000diagnostic triangle references remain. It is not an engine
world integration, completed tree, cheap far representation or visual upgrade.
Prepared branch/foliage geometry products, direct coarser demand and world
registration remain the next work. Input/commands/binaries/logs/receipts/images
remain under `/tmp/outshine-native-plant-recipe-39bf9e07d/crown-asset/`.
