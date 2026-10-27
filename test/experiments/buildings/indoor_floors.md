# Explicit indoor floor contacts

Python reference for [#111](https://github.com/Weichwolf/outshine/issues/111):
choose actual floor surfaces for the31 original indoor requests previously
declined by Buildings. Thirty flat ways receive closed floor plates and explicit
XYZ contacts; one actual staircase remains pending. A contact qualifies the
chosen local floor, not public access, rooms, doors or a complete route.

## Input and ownership

[Fixed inputs](../../outshine/data/buildings/indoor_floor_requests/README.md)
contain three complete OpenFreeMap vectors and one Mapterhorn DEM.
`indoor_floor_inputs.py` owns provider decoding, original IDs/attributes,
height-interval validation and coordinate normalization. It accounts for all4174
building parts:4167 accepted and seven omitted with reasons. The219 original
indoor axis parts comprise85 flat footways,64 vertical connectors and70
underground ways. Only normalized flat-footway purposes participate in scoring.

The older capture labels its locally scaled Web Mercator coordinates as ENU.
Verify every original against the pinned raw transportation feature, then
construct canonical WGS84 ECEF→ENU axes from that feature. Original dictionaries
remain unchanged. Geometry has one explicit frame; `level` labels remain
semantic indices, never metre heights. Supplied min/max envelope heights do not
specify measured floors. Width1.5m and construction limits are explicit recipes.

`indoor_floor_plan.py` consumes generic framed volumes and typed requests.
Query seeds close intersecting components within the supplied source snapshot;
no original building-parent topology or global completeness is inferred.
Only23 parts in three components need DEM anchor samples. Their median gives a
chosen foundation; the2m sample-deviation guard does not prove whole-terrain
clearance or produce cut/fill. Fixed source constraints determine the programme
even when a caller asks for one floor; only requested plates are materialized.
Missing ground, unsupported kinds and capacity limits yield local reasons.

## Compared procedures

- `uniform`: distribute storeys within the envelope at nominal3.2m spacing.
- `structural`: prioritize supplied lower-volume planes when headroom fits.
- `constrained`: dynamic programming over the finite union of uniform bands,
  zero and supplied lower planes. Fix ground at zero; choose ascending bands
  with2.1m headroom and0.2m slabs, or leave semantic labels unsupported.
  Lexicographic objective: covered fixed-source corridors, nonempty floors,
  earlier semantic labels, then lower candidate indices. Optimality applies
  only to this enumerated family, not arbitrary building designs.
- `adapted`: before the same programme, explicitly extend generated ground
  envelopes toward fixed ground-footway demands. Each addition is bounded by
  `min(0.25m, half a tile-centre coordinate step)`. A1mm construction margin is
  actual added geometry with mitred corners. Full-corridor `covers` stays strict. Exterior-only
  fringes preserve courtyards; unknown coordinate precision disables additions.
  Collective coverage can use several source parts. Retain original/adjusted
  WKB, contributing source IDs, added area and observed vertex distances.

Uniform/structural/constrained/adapted bind14/16/26/30 of the same31 requests.
The actual staircase source
`openfreemap:14/8939/5681:transportation:4031088860:3` remains
`unplanned_indoor_contact_kind`; a flat plate is not a staircase solution.
Neither candidate bands nor envelope changes reconstruct original architecture.

## Geometry, contracts and limits

`indoor_floor_geometry.py` uses the existing closed-prism constructor. Emitted
triangles are checked for finite coordinates, degeneracy, oriented closure per
plate part, top-domain equality and signed volume. Point-touching separate
plates keep separate face ownership and supply no navigable connection.
`indoor_floor_checks.py` independently checks actual top triangles against
absolute source-height intervals, including obstructing lower planes and roofs.
Arithmetic area/volume bounds are explicit; they do not expand membership.
Failed emitted geometry cannot receive a ready contact.

Ready corridors emit shared `Reservation`/`HeightInterval` products for future
room planning and original `SourceFeature` products in the explicit batch frame.
These reserve space above the floor; they do not prove absence of future walls
or native World obstacles. Consumers must use adjusted envelopes, rather than
combine adapted plates with unchanged generated building bodies.
Explicit capacities bound sources, component vertices/adjacencies, queries,
static constraints and candidate levels. No runtime source closure, cache,
streaming, whole-terrain/statics or native/device-frame qualification is implied.

## Reproduce

Use `.venv` with installed `outshine_ref`, NumPy, Shapely, mapbox-vector-tile
and Pillow; plotting needs Matplotlib, optional allocation traces need Memray.
No new dependency is introduced. From the repository root:

```sh
PYTHONPATH=lib/src:test/experiments/transport OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1 nice -n 10 .venv/bin/python -B -m unittest discover -s test/experiments/buildings -p test_indoor_floors.py -q
PYTHONPATH=lib/src:test/experiments/transport OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1 .venv/bin/python -B test/experiments/buildings/compare_indoor_floors.py --output /tmp/outshine-research-indoor-floors-comparison --rounds 3 --trace --plot
```

The output directory must be new. It contains all31 responses, receipts, limits,
component programmes, mesh audits, actual NPZ buffers, rotated warm CPU/wall
runs, separate original allocation traces and local diagnostic images. Source
read/decode/frame normalization/DEM preload is measured separately. Warm
generation includes construction and independent checks; profiler/image work
is excluded. Allocation traffic, scoped high-watermark, process RSS and retained
payload sizes differ. Total copied bytes are not instrumented. Python timing
does not establish A18/Iris or p99. Tests never fetch or regenerate input pins.

Müller et al., SIGGRAPH2006,
[Procedural Modeling of Buildings](../../../doc/references/buildings/siggraph/2006-procedural-modeling-of-buildings.pdf),
page4/Figs5–8, informs volume-to-component decomposition. The floor-band
programme and transport-led envelope changes here are our own derivation,
not an algorithm attributed to that paper. Wall/room/door and vertical
circulation planning, terrain support and native integration remain open.
