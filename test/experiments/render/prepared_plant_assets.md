# Prepared native plant products — #1 / #10 / #48

`src/generators/flora/PreparedPlant` owns a finished, device-independent plant
product. It uses the existing GeometryAsset codec and AssetCache/ResolveAsset;
there is no second store, index, compression layer or source-data format.

## Product boundary

- PlanIdentity holds the32-byte canonical plan identity; Root names the selected
  axis. The plan itself is separate and is not required to display this product.
- Prototypes part0 holds combined wood; Axes partitions its index stream into
  nonoverlapping complete triangle ranges, retaining each source-axis ID.
- Remaining parts hold exactly one shared blade mesh per nonempty leaf group.
  Groups have strictly increasing original variant numbers; sparse numbers stay
  sparse. Group n uses prototype part n+1, not part variant+1.
- Each group stores separate stable leaf IDs, owner-axis IDs and affine Mat4
  placements. IDs are unique in their own axis/leaf domains. Every leaf owner
  belongs to the stored branch. Matrices are Float64 column-major, finite and
  orientation-preserving; zero/reflected/projective transforms are invalid.
- Geometry uses native local Y-up metres. Part placement applies before instance
  placement. The producer converts canonical Z-up frames once. Normals are
  prepared for every part, UVs for blades. Existing native material/image/mip
  data are carried by GeometryAsset. Runtime light/wind are not baked poses.

The generator-owned versioned envelope encodes fields explicitly little-endian,
without struct padding, pointers or GPU handles. Geometry is encoded once,
followed by axis ranges and compact grouped instance records. Counts are checked
against remaining input before allocation. Unknown versions, incomplete ranges,
missing prototypes/owners, duplicate identities, invalid geometry/placements,
trailing bytes and exceeded encoded-byte limits fail without a partial product.
The encoded limit is not an exact peak-memory limit. Cold encoding temporarily
holds geometry bytes and the final envelope; decode owns mesh/instance arrays
alongside its borrowed input lease. It is not mapped or zero-copy loading.

## Verification and limits

```sh
JOBS=6 make suite SUITE=outshine/src/generators/flora/PreparedPlant
```

Two module cases run normally, with ASan/UBSan and container hardening:6/6PASS.
A cold ResolveAsset builds once; a reopened cache loads with an empty factory.
Decoded prototype streams, source identities and matrices remain exact. The
negative case rejects every truncated prefix, excessive counts/byte budgets,
unknown versions, invalid metadata/placements and incomplete lighting normals.
Only native geometry/asset/base units are compiled; no GPU, world or provider
is linked. Product units pass focused clang-tidy. Case analysis still reports
pre-existing shared Check.h findings tracked by #603/PR#666; full lint is open.

The actual64-axis/571-leaf reference was also displayed through Metal640×720
using this codec and ResolveAssetRequest. Cold process: one producer call;
fresh hit process: zero calls and empty stdin, with no canonical plan or
surface construction. Eight shared blade meshes and571 instances remain.
The806214-byte uncompressed product contains719744logical prototype-array
bytes,73088matrix bytes,571×16=9136leaf/owner-ID bytes,64×16=1024axis-range
bytes and3222bytes of native/envelope metadata. These are logical content
sizes, not vector capacities, SQLite size, RSS or GPU residency.
Cold/hit images and the previous direct portrait are pixel-identical and were
opened. Original commands, private probe, source/binary hashes and images stay
under `/tmp/outshine-prepared-plant-c4fb02068/`; no gallery publication.

This is the same thin technical young plant described in native_plant_plan.md,
not an accepted mature species or forest. Native recipe/plan persistence,
budgeted product assembly, near/far LOD selection, world registration, wind,
whole-Place images and device time/memory gates remain integration work.
