# Original Ill inputs in the canonical water frame (#760)

The original export f45193925 closes the geographic-input gap. `native_river_inputs.py`
reads the hash-pinned **historical WATER2** blob using its recorded codec layout,
including all 550 surfaces, 564 rings, 935 courses and every byte of its 257884-byte
payload. Arrays are immutable views, ranges/tile addresses are checked, and the
reader has an explicit 1 MiB input guard. WATER3/WATER4 are rejected rather than
misinterpreted. The later WATER3 blob is separately hash-checked, not decoded or
silently substituted for the original.

Original coordinates are latitude,longitude. The adapter explicitly changes order
when calling the WGS84 helper. The selected 13 surfaces/17 courses and 485 original
ring points retain their native indices, levels, order and holes. Source metadata
comes from the separately recorded later observation under the existing permutation
contract; WATER2 has no provider-input table or recorded source epoch. Both original
blobs and the old planar NPZ/JSON remain unchanged.

`compare_native_water_frame.py` applies the [physical facet reference](geodetic_facet.md)
at the fixture's declared origin. It compares the old chart with canonical WGS84
ENU, builds the common **zero-datum reference footprint**, maps its faces to all
positive original overlaps with the agreed CSR component, and separately checks
physical per-fragment triangles. The output footprint has XY/topology/provenance,
**no fitted water height**. CSR indices address the selected 13-entry table;
`source_original_indices` maps them to the original native surface array. Do not
pass selected-table indices directly as native global surface IDs.

## Real-input results

| Quantity | Measured finite case |
|---|---:|
| Maximum legacy XY versus WGS84 zero-height XY difference | 12.198757 m |
| ENU-Up minus same numerical geodetic model height | −2.841940…−0.0000986 m |
| Maximum height-induced XY shift | 0.451666 m |
| Canonical reference footprint | 1492 vertices/faces, one body and one island |
| Positive original memberships | 1607; all 13 originals referenced |
| Membership polygon tests | 1667 versus 19396 complete pairs |
| Physical fragment triangles / interior centroid queries | 461 / 461 |
| Height-query disagreement with independent PROJ inverse at those centres | ≤4.73e−9 m |
| Sampled fragment-centre height difference from its original constant level | ≤0.015624 m |
| Sampled geodetic versus ENU-Up grade difference | ≤8.271e−4 m/m |

These are coordinate/model distinctions, not evidence that the existing native
renderer uses the old chart or is displaced by 12 m. The native placement already
uses geographic points. The numerical source levels labelled ASL are interpreted
only as the existing declared height model; no observed geoid offset is invented.
The old chart's degree scales are measured against the actual 485 original points
(maximum reconstruction residual 2.73e−12 m), solely for comparing coordinate
definitions. Original geography is read from WATER2, never recovered from old XY.

The raw 17 courses contain **121** endpoint-segment records; every record is measured,
with original course index/local ordinal retained. The older fixture has 120 derived
segments and reconciled levels; these arrays are not relabelled original or overwritten.
Geodesic length versus the old chart differs by at most 0.576012 m for these segments.
Endpoint model-height/geodesic grades and ENU-Up/anchor-XY grades differ by up to
8.707e−4 m/m. Endpoint order is not an assured source flow direction.

Ten pairs of selected original footprints overlap with different constant prior
levels, with a maximum difference of **10.227783 m**. This is a concrete reason to
fit a common field instead of retaining separate flat fragments. It does not by
itself prove hydrological compatibility. Unselected originals are listed under the
explicit finite-fixture selection reason; full original tables remain in WATER2.

The reference union/triangulation retains the island and source footprint with
zero symmetric-difference area and 1.93e−9 m² overlap residual. CSR missing area
sums to 5.26e−10 m², within the existing coverage check; no tolerance is loosened.
The actual fragment triangulation is Python/GEOS and may choose different diagonals
from a particular native build. Centroid checks do not certify whole-facet geodetic
monotonicity or replace the native watertight/BVH query. The full shared cooker still
needs an explicit physical fit/contact/direction contract and native integration.

## Work, evidence and reproduction

Both source blobs total 608333 bytes. Decode/projection/footprint/CSR/461-facet checks
take about 1.80 s in one instrumented Python run with ~2.55 MB traced peak; GEOS/PROJ
native allocations are excluded. Output arrays occupy 178068 bytes. No provider,
source-cache or native-asset-cache writes occur. This is no native build, loader,
frame-budget, target-device or runtime-image admission. The local diagnostic was opened.

```sh
OPENBLAS_NUM_THREADS=1 PYTHONPATH=lib/src:test/experiments/water .venv/bin/python \
  -W error -m unittest test_native_river_inputs test_geodetic_facet \
  test_river_membership test_river_surface
OPENBLAS_NUM_THREADS=1 PYTHONPATH=lib/src:test/experiments/water .venv/bin/python \
  -W error test/experiments/water/compare_native_water_frame.py \
  --input test/experiments/water/fixtures/feldkirch-ill-native-inputs.json \
  --output /tmp/outshine-research-water-frame-<commit>/capture
```

Dependencies: existing `outshine_ref`, NumPy, Shapely/GEOS, pyproj/PROJ and Matplotlib.
The fixture manifest pins original files, sizes, hashes and matching historical
reader commits/files. The result pins source provenance, arrays/image and scope.
The [geodetic derivation/primary sources](geodetic_facet.md) and
[provenance method/primary sources](river_membership.md) remain their single owners.
