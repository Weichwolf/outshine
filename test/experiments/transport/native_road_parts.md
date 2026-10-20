# Native semantic road parts

Owner: [#915](https://github.com/Weichwolf/outshine/issues/915).
This fixed offline corpus supplies actual corridor intervals and whole junctions
for a package/contact comparison. It is not a runtime page format, prepared render
LOD, qualified occluder, complete physical network or device-budget admission.

## Capture and selection

The [fixed input](../../outshine/data/transport/native_road_parts/manifest.json)
contains 6,510 original corridor emissions and 1,848 whole junction emissions.
The core is one acquired source tile, 14/14549/6452, with 5,486 edges.
All incident edges of its touched junctions are included as one complete halo.
Another 208 edges and 103 junctions are exact metadata-only boundary references.
The 7,479 captured junction legs bind the same endpoint node, gate and plane as
their original corridor ports. There are 2,051 referenced source lanes.

The source is native C++ d408273e7, whose affected generator/cache units are
unchanged through 725530ead; shaders are the checked 4f4a1f94e combination.
A private observer records the deltas of the existing PaveEdge and incremental
junction emission. It does not split arbitrary triangle ranges or change recipes.
The full fine job has 53,096 edge and 11,854 junction emissions. Its final position,
normal, colour and index arrays are word-identical to the original warm region;
the original part material and placement also match. Shared asset publication is
disabled in the diagnostic, and the original asset index/package count/hash stays
unchanged. Source acquisition is offline.

Baseline and observed fresh native processes both reach Refined and save identical
1280x720 RGBA bytes, SHA-256 pinned in the manifest. The generated street arrays
are checked before the existing stored region is installed. This does not certify
all newly generated terrain or other world classes. Core/Sync controls report no
VUIDs; current images were opened. Diagnostic IO, JSON and forced generation are
not a performance comparison. Raw original inputs, actual full compiler commands,
archive/source hashes and terminal receipts remain in the manifest's evidence path.

This is a new Iris-side capture. It is not the geometry/device provenance of
Master's separate A18 scene-depth oracle in native_road_coverage.md.

## Read and verify

No installed Python package or private cache is needed to consume the fixed input:

    python3 -B test/experiments/transport/native_road_parts.py
    python3 -B -m unittest discover -s test/experiments/transport -p test_native_road_parts.py

native_road_parts.load() returns manifest, metadata, parts, immutable payload and
one shared native material-table asset. checked_payload(part, payload) returns
borrowed stream views without copying. Whole-member and per-stream SHA-256 hashes,
sizes, finite words, original index ownership and junction bindings are checked.
Tests retain negative controls for cross-part indices, missing incident payload
and changed cut values. Normal invocation never regenerates this fixture.

The tar archive has four regular members: metadata.json, parts.jsonl, payloads.bin
and materials.asset. No archive extraction is needed. Payloads contain original
little-endian Float32 position/normal/colour words and UInt32 indices, in that order.
Counts are scalar words; positions/normals have three and colours four per vertex.
Indices retain their original global values; subtract base_vertex for an
independently owned mesh. No vertex word or primitive slot is simplified.
Original root material index and placement are in the manifest; material tables
are stored once, with no mesh parts.

Part metadata stores original lane/edge/node/contact IDs and optional endpoint
ports. A gate is east/north/height/out-east/out-north/half-width/shoulder/thickness;
a plane is slope-east/slope-north. Junction legs are edge/end/angle/half-width/cut.
Planned stations are east/north/height/node, in the captured tangent frame.
Mesh positions are east/up/south. Source records preserve supplied provider ID,
tile, layer and ring, with provider/revision/input digest. ID zero is valid.
Keep node/provider IDs as integers: converting mixed station rows to floating point
would lose large UInt64 IDs. JSON retains their exact integer words.

## Remaining contract

Local plan IDs are qualified by this frozen source/plan, not globally stable across
source revisions or ingestion order. Boundary references have no hidden payload.
A node with no separate junction emission is not inferred to have missing topology.
The corpus does not supply continuous functional curves, canonical future page IDs,
independent coarse replacements or conservative opaque coverage.

Compare layouts using these same native atoms and contacts; do not reduce stations
or change the functional reference. Visibility and spatial bounds computed from
these fine payloads are offline oracles only. Production must obtain qualified
coarse metadata first, then load independently compressed needed purpose/LOD pages
through the existing AssetCache, with valid parents, separate functional demand and
charged IO/decode/copies/residency. AABB alone does not prove occlusion.
