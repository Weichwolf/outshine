# Native observed source metadata before the water fit (#875 / #760)

`native_water_observation.py` decodes the already-versioned **later WATER3** payload
using the historical codec at `6bb149e9f`. `native_river_inputs.py` still requires
WATER2 for the original geometry; both share the geometry/range reader. WATER4
is not accepted as either historical format. No source or oracle is regenerated.

The original manifest pins both files and their provenance:
`fixtures/feldkirch-ill-native-inputs.json`. WATER2 has no input metadata table or
recorded source epoch. The observed table therefore supplies **later observations**,
never historical metadata or an assured OSM flow direction.

## Exact bindings and retained ambiguity

The complete 350,449-byte WATER3 input contains 550 surfaces, 564 rings, 935
courses, 49 sources and 1,482 input records. The two payloads total 608,333 bytes.
All numeric views remain backed by immutable original bytes. Metadata tuples
contain decoded dictionaries; they are not immutable game assets.

The audit compares complete point/level pools and every geometry record by exact
bytes. Surface keys include ordered rings/holes, level and owning tile addresses.
Course keys include ordered coordinates, per-point levels and width. Tile range
offsets may change under the actual permutation; their membership/address/count
is checked instead. Record multiplicities are preserved, without tolerance or
provider-ID matching. All matching observed indices are returned. In the full
input eight course keys occur twice (16 ambiguous original records); none is
silently assigned the first available source identity.

The old fixture's 13 selected surfaces and 17 courses have unique metadata bindings
to the actual later tables. All 13 surfaces are `river`; the courses contain 13
`river`, three `stream` and one `canal`. Thus this laboratory selection is not a
uniform river-class or hydraulic-connectivity admission. Its solver constraints
and generated DEM-height priors remain laboratory hypotheses.

The complete later input table contains two bridge and 300 tunnel flags. Those
and all classes/identities are preserved. Provider feature IDs remain decimal
strings from full uint64 values, including absence and zero. Unknown intermittent
flags stay `None`; false flags are not promoted to observed ground-level evidence.
This capture has no unknown intermittent records. Empty crossing strings, defaults,
class equality or footprint overlap do not prove hydraulic connection.

## Strict codec controls

Geometry records use the original little-endian scalar widths; metadata uses
uint64 record counts, uint32 text lengths, canonical one-byte booleans and optional
presence flags. Ranges, enum/tile/cell limits, source/feature references, geometry
input indices and EOF are checked. `NoInput` remains unknown. Input reads are
bounded to the explicit 1 MiB diagnostic budget plus one overflow byte before
hashing/decoding. Arbitrary native string bytes round-trip through UTF-8 with
surrogateescape; no text normalization or fabricated field occurs.

Tests include an independent field-by-field synthetic writer, absent/present
optionals, uint64 values above 2^53/max, opaque text bytes, invalid booleans/counts/
lengths/identities/references, truncated/trailing/foreign-version/digest/byte-budget
failures, complete original bindings and deliberate ambiguous geometry. Changed
metadata cannot pass merely by retaining a recorded digest next to it.

## Reproduce and costs

```sh
PYTHONPATH=lib/src:test/experiments/water .venv/bin/python -W error \
  -m unittest test_water_observation test_native_river_inputs
PYTHONPATH=lib/src:test/experiments/water .venv/bin/python -W error \
  test/experiments/water/compare_water_observation.py \
  --output "${TMPDIR:-/tmp}/outshine-research-water-source-observation"
```

NumPy is required for the reader/audit; Matplotlib supplies the local 1280×720
geographic metadata diagnostic. The wider original-frame tests retain existing
pyproj/GEOS/reference-library dependencies. Outputs remain in system temp.
The report contains all geometry mappings, candidate selected metadata, source
identities, class/crossing census and diagnostic cost scope. It reports explicit
key payload copies, point/level/record work, traced peak, CPU/wall time and process
peak RSS; total allocations, native bytes/CPU/GPU/device costs are unavailable.
No solver or native speedup is claimed.

## Primary implementation source and remaining contract

Historical `WaterAssetCodec.cpp`, `WaterAssetMetadata.cpp`, `WaterAssetFormat.h`,
`BinaryValueArchive.h`, `TileSourceIdentity.h` and `DataKind/Address.h` define the
bytes and bounds. These are source-parsing checks, not a new geometric algorithm.
The local provider water/waterway schemas are semantic context only; raw native
fields are preserved rather than expanded into unavailable original OSM tags.

No fit, library/native API, cache or world product changes. #760 still owns
hydrological source admission, physical connections/direction provenance, datum/
regional/terrain/bridge contacts, native solver/cooker integration and device/
Place gates. This reader does not join two water bodies or establish a hard peg.
