# Original indoor requests and complete provider snapshots

Fixed offline inputs for [#111](https://github.com/Weichwolf/outshine/issues/111),
not reconstructed buildings or edited fitting polygons. `manifest.json`
contains all31 original request dictionaries from
`test/experiments/transport/fixtures/bridge-continuations-vienna.json`, its SHA256,
the actual original coordinate convention and common origin.

Three complete OpenFreeMap MVTs are stored losslessly as `.pbf.xz`; receipts
bind decompressed bytes. The original Mapterhorn Terrarium WebP is verbatim.
Exact URLs, tile coordinates, byte lengths and SHA256 receipts for all four
inputs are in the manifest. Other source layers, unrelated buildings, original
quantization and attributes remain present. Tests verify receipts without
fetching, generating or changing pins.

Vector data: © OpenStreetMap contributors,
<https://www.openstreetmap.org/copyright>, distributed by
<https://openfreemap.org/> (ODbL source data).
Elevation source: <https://mapterhorn.com/>; its original receipt also remains
in the existing `shared-bridge-vienna.json` transport fixture.

Fixed place coordinates remain in test inputs/drivers. Diagnostic PNGs, traces
and private measurements go only to explicit system-temp output directories.
Procedure and reproduction:
[indoor_floors.md](../../../../experiments/buildings/indoor_floors.md).
