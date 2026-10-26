# Selected canonical station cuts (#1189, #1206)

`PlanarStationAxis.InitialFaces` identifies sorted unique faces of the original
input mesh. Omitted selection retains the existing global cut. An explicitly
empty selection requests no cuts. Descendants inherit selection through their
original `FaceParents`; selection never changes into a test of current face IDs.
This geometric kernel knows neither source classes nor source-height ownership.

Each cut first clips selected faces. One edge-key cache creates each crossing
once. Unselected faces then split only their actual shared edges found in that
cache; the plane does not extend across their other edges. Neighbor subdivision
uses a four-triangle stack partition. Original positions/indices, station values,
face lineage and shared edge identities remain available. Output is published
only after every requested cut succeeds within its budgets.

Scope membership uses one byte per original face. At most one pending unselected
index per current face uses a uint32_t; vector capacity/allocator overhead is
additional. For each cut, face work is linear in
current faces plus actual pending neighbors; no proximity scan or grid is added.
Existing intersection storage remains bounded by vertex/face budgets. `FaceVisits`
includes both the selection pass and processed unselected faces; global execution
retains its previous single pass. This is bounded asset-construction work, not a
claim about whole-world complexity or frame time.

## Reproduction

```sh
make all db
make suite SUITE=outshine/src/base/spatial/PlanarStationMesh JOBS=2
PYTHONPATH=lib/src python3 test/experiments/transport/native_local_station_cuts.py \
  --output "${TMPDIR:-/tmp}/outshine-local-stations-<unique-run>"
```

`data/station-profile-boundaries.json` is a fixed upstream geometry input, not an
expected output. Native complete profiles/ribbons/layout produced its canonical
faces at 566ee6bb45. It includes the actual whole profile polylines and explicit
initial face selections. The experiment compares original, translated and rotated
inputs against the existing Python `_split`, checks exact input preservation,
canonical children/parents/stations, source areas, edge conformity and full profile
coverage. Global cuts remain a diagnostic comparison. It records versions, build
commands, returned capacity bytes and measured work separately from compilation.
Returned capacity is not peak scratch memory; fewer logical vertices need not
reduce a vector's allocated capacity.
No oracle is regenerated. The native module tests also cover unaffected remote
faces, shared neighbor nodes, repeated cuts, empty/invalid selection and work limits.

`PrepareSurfaceFlights` seeds these selections from the qualified original source
faces before height aliases are reconstructed. Independent multi-flight/height
correspondence and downstream body/navigation/terrain qualification remain
#1206/#1189 work. This operator alone proves neither a visible street repair nor
runtime/cache admission.
