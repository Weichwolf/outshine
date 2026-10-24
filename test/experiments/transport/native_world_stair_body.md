# Native World stair bodies · #1189

`BuildRoadStepBody` consumes a planned interval and its real `RoadPortPositions`.
The fixed input is `data/world-stair-ports.json`; its old ramp is an input record,
not the desired output. The adapter does not solve already owned junction heights again.

- Keep all four Float32 profile knots at each exterior boundary, including crossfall.
- Design two short continuous transition strips and a straight interior flight.
  The three local source masks are start transition, flight and end transition;
  all retain the containing interval's original lane/source identity.
- Landing run is at least max(0.6 m, half-width, exterior height spread / 0.125).
  These are recipe choices. Maximum rise is the shared body's 0.179 m;
  at least 0.24 m run per rise and at most64risers are required.
- A straight recipe may replace an axis within0.05m; no global plane/error
  tolerance changes. Exterior gates must match the shared section and align
  within dot>=0.99. Reverse source direction produces the same physical body.
- The existing `BuildSurfaceBody` constructs indexed treads, risers and underside.
  It validates the closed solid after Float32 conversion. Outer profiles remain fixed.
  The reference body's strict 1e-8 m plane check remains unchanged.

The small Python construction at the fixed input followed by the existing body
probe yields21risers,198indexed body vertices,392faces and positive Float32
volume5.3385633212m³; all8exterior profile knots remain word-identical.
This is one input, not a worldwide or frame-budget admission. Probe-only body
cost was3.5–5.5ms on the local host; it excludes preparation, terrain and upload.

## Actual consumer

After junction profiles are complete, Corridors prepares qualifying ground steps
once. The immutable body is shared with `RoadPageRecipe` and the direct meshing
path. Both render flat treads/risers with geometric face normals. Native page
encoding stores finished render geometry; cache hits need no stair reconstruction.
`BuildSurfaceEarthworks` takes these same body undersides, replacing the old
axis-ramp stamps for this interval. Existing junction geometry/profiles remain.
Ground recipe94 invalidates earlier region products; the page byte format is unchanged.

Source placement/access facts remain optional and unchanged. The generator
explicitly chooses a ground-contact recipe for unmarked ground steps. Known
nonzero layers, bridges and tunnels are outside this recipe. Invalid input,
unsupported geometry, body failures and terrain-contact failures retain the
previous interval product and have separate aggregate counters. Retained body
array capacity is reported separately; it is not an exhaustive allocation peak.

```sh
make suite SUITE=outshine/integration/transport/RoadStepBody JOBS=2
make suite SUITE=outshine/src/generators/road/Corridors JOBS=2
```

Remaining #1189 work: explicit native navigation/physical consumers, multi-flight
World groups, actual terrain-edge conformance and tunnels; general LOD/body
persistence and live-frame scheduling still need qualification. This local body
integration does not establish those contracts or 720p60 on either device.
