# Native road-curve method inputs

Owner: [#915](https://github.com/Weichwolf/outshine/issues/915).
Contract: full functional geometry in the asset DB, separately requested render LODs.
This fixture supplies actual analytical road sweeps and their original upper meshes;
it is not an asset format, final network, visibility product or runtime admission.

## Contents and provenance

[test input](../../outshine/data/transport/road_curve_requests/manifest.json) describes
`requests.jsonl.xz`, SHA-256, fields, capture source and attribution. The native source
is `e2ae8098599d142397c72e706499604fa3ffad60`, original Ribbon SHA-256
`3b1ca5810a6a5267521aabb804440b8f43b899a9c7823dfc43e8d647229f1545`.
A diagnostic observer records before external port stitching and world transforms;
private cache keys preserve original products. Both captures reproduce the original
fixed-pose image exactly. They are correctness diagnostics, not timing benchmarks.

The archive retains 1,462 of 62,653 successful sweeps from one acquired source region:
1,024 lowest original-record hashes, up to 64 largest records for each of curvature,
slope, slope rate, bank, bank rate, station count and half-width, plus twelve
surface-comparison witnesses. Union removes repeated selections, not repeated roads.
Original capture ordinals remain; one region does not establish worldwide behavior.
The entire second capture matches every original field in every first-capture record;
only interval data is added. Selected native shapes are not the rejected gap candidate.

## Fields

- `range_m`, `step_m`, `required_m`, `stations_m`: original sweep and sampling inputs.
- `section_m`: half-width, shoulder width and thickness, metres.
- `form`: surface or closed shell. Only the four upper cross-section vertices per
  station are included below; closed-shell walls/caps are not supplied.
- `seams`: station, relative east/north/height, heading, curvature, curvature rate,
  slope, slope rate, bank and bank rate. Units/order are explicit in the manifest.
- `intervals`: consecutive seam ranges, midpoint poses with the same columns, and
  native maximum absolute reference-line curvature for each interval. Midpoints
  avoid sidedness ambiguity in curvature rates exactly at plan seams.
- `upper_positions_m`, `upper_normals`: four XYZ values each per original station;
  position axes east/up/south, relative to the initial reference pose. Float32
  emitted values, before external network contact adjustments.

A reference-line curvature bound is **not** a swept-surface or projected-error bound.
Heights/bank use the native continuous profiles; do not infer safety from station
count alone. Junction contacts, source way IDs, materials and final world placement
are absent. A successful isolated sweep comparison cannot certify those consumers.

## Read without private build dependencies

```python
from pathlib import Path
import hashlib
import json
import lzma

root = Path("test/outshine/data/transport/road_curve_requests")
manifest = json.loads((root / "manifest.json").read_text())
archive = root / manifest["archive"]
assert hashlib.sha256(archive.read_bytes()).hexdigest() == manifest["archive_sha256"]
with lzma.open(archive, "rt") as stream:
    curves = [json.loads(line) for line in stream]
assert len(curves) == manifest["records"]
```

Retain the full functional reference. Compare independent display sampling/LOD
against these surfaces, then against actual native contacts, physical queries,
visibility and images. Neither loading every fine curve nor reducing the physical
network is the intended renderer. Broader geometry/visibility inputs are separate.
