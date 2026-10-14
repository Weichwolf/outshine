# Transport reference fixtures

`surface-sampling-vienna-4ea502363.npz` is the explicitly generated derived mesh
and 22,000 contact queries/results from [#69](https://github.com/Weichwolf/outshine/pull/69).
Native integration belongs to [#48](https://github.com/Weichwolf/outshine/issues/48).
No raw provider tile, decoder, source refresh or native result is included.

The adjacent JSON pins the fixture bytes, query/result buffers, reference commit,
source hashes/receipts/usage, local ENU frame and numeric comparison tolerances.
Arrays use local metres: `vertices`/`triangles`, `queries_xy_m`, `points_enu_m`,
unit upward `normals_enu`, `face_indices`, `reasons`, and the original road mapping.
Unresolved height/normal are NaN and Face -1; the reason remains explicit.
Face indices/reasons compare exactly, heights at 1e-7 m and normals at 1e-12.
Facet normals do not imply smooth C1 terrain or a complete functional Place.

From the repository root, verification only reads existing pins:

```sh
.venv/bin/python -W error test/experiments/transport/surface_contact_fixture.py
```

Reference creation is a separate deliberate command requiring the retained,
hash-checked `4ea502363` Ground/benchmark products under `build/experiments/transport/ground/`:

```sh
.venv/bin/python -W error test/experiments/transport/surface_contact_fixture.py --generate
```

Generation verifies the original query and full-result digests before writing.
Normal tests and the default verifier never create reference data or update pins.

`vertical-transition-cases.json` contains 256 saved Vienna straight ground axes,
original source IDs/tags/XY, DEM endpoint targets, class grade recipes and original
OpenFreeMap/Mapterhorn receipts. It supplies portable inputs for #20's flat-ended
vertical profile comparison, without requiring the original decoder/cache. Flat
end grades and fallback width are explicit procedural assumptions. Normal runs of
`vertical_transition_experiment.py` only read it; `--capture-inputs` deliberately
replaces it from the retained original source and DEM cache. These cases do not
prove a complete junction/terrain/grade-separated network or native integration.

`overpass-vienna-cases.json` pins 53 independent bridge/ground crossing pairs from
the original Vienna tile and one additional tile from the same OpenFreeMap snapshot.
Original source geometry/IDs/tags, DEM endpoint targets, class/fallback width origins,
source hashes/receipts and considered/unselected/deferred source identities survive.
These are alternative local experiments, not a combined certified transport network.
`overpass_checks.py` and `overpass_experiment.py` only read the fixture. Explicit
`capture_overpass_cases.py` replaces it from prepared raw inputs; `--fetch` acquires
only missing DEM tiles. Captured raw full features stay in build/source cache.

`shared-bridge-vienna.json` captures the complete positively overlapping bridge
corridor component and every area conflict in the retained Vienna-Donau source tile,
seeded by the most-crossed bridge of the pair fixture. Six deck sources and17 outdoor
ground sources are planned together;10 indoor paths retain original geometry/tags
with an explicit missing Buildings height/contact reason. All793 axis IDs and44
polygon-source IDs remain available for the usage ledger. Original endpoint DEM
heights are soft procedural priors, not measured bridge-port elevations. Normal
checks never update this pin; `capture_shared_bridge.py` explicitly recreates it from
prepared input and existing DEM cache, without network access. Shared exterior
surfaces do not certify indoor floors, outside ramps, traffic routes or full Places.
