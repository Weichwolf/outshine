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

## Bounded assembly from a selected branch

BuildPreparedPlant takes the immutable plan, canonical blade recipes, source
identity, an owned native material palette and explicit detail/budgets. Palette
slot0 is wood, slot1 is leaf; it has no parts/lights and at most two surfaces.
Its images and prepared mip levels are copied with the native owner on a miss;
the caller palette remains unchanged. Bare products need only wood. This is a
per-product palette, not a resident global material catalogue.

MeasurePreparedPlant uses prepared descendant/variant counts and visits only
the requested axis-row range and used blade recipes. It counts before product
allocation, without walking leaf placements or building surfaces. Four budgets
are independent: unique vertices, unique triangles, leaf instances and logical
triangle references for one whole-plant instance. References count wood once
plus each blade's triangle count times its placements; they are not measured
GPU invocations. Integer/index limits are checked before buffer reservation.
Failure returns no partial product and does not silently choose another LOD.

Assembly emits wood once with source-axis ranges and one mesh per used blade
variant. Instances retain IDs/owners; source frames convert once into native
placements. Unused catalogue variants produce no mesh or placement work. The
flat prepared subtree order is deterministic and source plan rows stay intact.
Work is proportional to selected rows, emitted geometry and selected instances;
this is not a whole-world optimality or screen-error guarantee.

```sh
JOBS=6 make suite SUITE=outshine/src/generators/flora/PreparedPlantBuild
```

Six normal/ASan+UBSan/container arms pass. A sibling/grandchild counterexample
keeps exact source geometry when requested alone or after another branch;
unused invalid recipes are never evaluated. Material/image ownership and the
Z-up/row-major to Y-up/column-major boundary are checked. Allocation interception
observes zero allocations in successful measurement and in all four distinct
underbudget build refusals. The module links geometry/plant units only, with
no GPU, provider or asset-store dependency.

The product unit passes focused analysis. Case analysis stays red at shared
Check.h findings and three libc++ parameter-name diagnoses for legal global
new/delete replacements. The same three diagnoses reproduce in the existing
GeometryAsset allocation case; no reserved implementation names, suppression
or weakened rule was introduced (#603).

The full64-axis/571-leaf technical portrait was assembled through this product
builder, then encoded/cached/decoded. Preflight counts15600unique vertices,
27680unique triangles,571leaf instances and174060logical triangle references.
Python independently checks all64 axis IDs/ranges and all571 original leaf
IDs, owners, variants and Float64 matrices in cold and fresh-hit output. All
agree; cold/hit/previous direct640×720Metal images are pixel-identical and
opened. Commands, hashes, original row output and images remain privately under
`/tmp/outshine-plant-assembly-5a97d0851/`. This is the same thin technical young
plant, not a mature species, whole-Place image or performance acceptance.

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
near/far LOD selection, world registration, wind,
whole-Place images and device time/memory gates remain integration work.

## Complete compiler flags

The frozen native4fcef0e36 full analysis found three missing designated field
initializers in BuildPreparedPlant under Clang23's full project warning flags.
The earlier hand-written focused invocation omitted these warning flags and
was too narrow to prove compiler parity. Explicit empty fields preserve the
original values and remove the diagnostics. The repair is checked through a
focused compilation-database entry carrying the full frozen project arguments,
plus the six owned module arms; no warning was disabled. Full-run outcome and
scoped follow-up remain separate evidence in #1/#603/PR#531.
