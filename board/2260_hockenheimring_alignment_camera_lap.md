Type: proof
State: active
Architecture: ready
Parent: 2175
Depends: 2173, 2133, 2256, 2262
Priority: P0
Area: scenario, navigation, generators, client
Tags: osm, driving, camera, visual-acceptance

# A camera completes a Hockenheimring lap on generated road alignment

## Purpose and boundary

Use a real OSM/DEM Hockenheimring input as a repeatable end-to-end acceptance
scene. The first stage is a kinematic camera on the engine's generated road
alignment, not a claim that a simulated car can already drive it. A closed
lap exposes missing links, wrong-level joins, gaps, grade jumps, road-edge
flips, terrain penetration, LOD pops and tile eviction. It is a development
driver because the entire scene uses the normal streaming pipeline; no
hand-authored track mesh or route spline may replace OSM-derived products.

`src/assets/places/Hockenheimring.scenario` is the data-gate overview, not a lap.
The existing `src/assets/drive/f31.scenario` contains a root `<drive>` that the current
reader rejects; follower camera placement exists, but no scenario contract
currently proves a route-bound camera. Do not revive the rejected element as
an opaque shortcut. Extend the declarative scenario/API contract for a named
route and a camera rig driven by (edge ID, station, lateral offset), or an
equivalent format-independent native route handle.

## First data gate

The normal VersaTiles provider (`versatiles.osm`, version 1, vector z14) was
inspected in the 5x5 tile window centred on 14/8581/5603. It exposes 348
`kind=track` and 226 `kind=service` street features, but zero `kind=raceway`
features and no `highway=raceway` property; the sports-centre POI alone cannot
identify the closed racing surface. All 1,162 street features carry MVT IDs,
98 repeated across tiles, but no checked raceway Way ID matches an MVT ID or
simple decimal scaling of one. `src/assets/world/vegetation.json` has no raceway
street rule either. The loop remains **unidentified** at this provider boundary:
do not silently route a service road or author a track spline.

Raw OSM `/api/0.6/map` extracts for bboxes `8.54,49.315,8.58,49.34` and
`8.58,49.315,8.61,49.34` contain 24 `highway=raceway` ways. Relation 284588,
`type=circuit`, names the Grand Prix layout: 16 main ways form one directed
267-node/267-edge cycle with no branch, approximately 4,565 m by spherical
segment sum. A seventeenth member is explicitly `role=pitlane`; shortcuts and
Rallycross ways are outside the main relation. Relation member order runs
opposite the ways' one-way direction, so solve the topology rather than using
the listed order. The 33,557-byte source pin is
`test/outshine/integration/places/HockenheimringGrandPrix.osm` (SHA-256
`f50914eac077325eee1b3e88eae0eeffac58ff086e180cba4ec8734e94661d7f`).
It contains route source objects, not a hand-authored track or whole-world
fixture. `Data::OsmXmlReader` retains all 319 nodes, 17 ways, roles and tags;
`World::TransportTopology` resolves their 267 directed main-route edges and
rejects reversed ways, missing members and pitlane promotion. No runtime route
or provider publication follows from those tests. WI 2173 must
deliver OSM way/relation IDs and tags through a general
provider or raw overlay. The MVT ID preserved by WI 2270 is only a provider ID
until a source mapping is proven. A source without relation, way IDs or
direction tags fails this gate explicitly.

The height-field footprint now covers building footprints and the vector
window plus a DEM halo. Before that fix, 49 structure tiles remained
uncertified after 6144 frames; extending only building fields exposed a
missing road DEM at east -6140 m. `make shots PLACE=Hockenheimring` now reaches
Refined and produces `build/shots/places/Hockenheimring-c46ddc43.png`:
in a repeated run p50/p95/p99 2.07/3.42/8.41 ms, 1/1349 frames above
16.67 ms, 393 MB peak heap, 953915 triangles, zero bare tiles. The opened
1280x720 PNG shows the
track area and buildings, but flat olive/brown surfaces and generic dark paths;
it neither identifies a raceway nor proves photorealism. The overview is input
diagnosis, not driving acceptance.

## Construction

1. Resolve circuit relation 284588 from a generic OSM semantic source; join
   its unroled member ways by directed node IDs into one closed route. Exclude
   `role=pitlane`, shortcuts and Rallycross. Verify 267 nodes each have one
   predecessor and successor in this pinned source. A missing member, reverse
   direction or ambiguous junction yields source-ID diagnostics, never an
   invisible camera teleport. DEM, transport and geometry still stream normally.
2. Route graph (2133) yields stable edge IDs independent of rendering. The
   bounded road candidate (2256) must publish complete matching geometry.
   Alignment (2175) maps each (edge ID, s, t) to double-precision position,
   tangent, bank and road width. Camera sampling uses this native alignment,
   not triangles, screen-space tracking or a separate authored spline.
3. Define a reproducible lap speed profile with bounded acceleration and
   curve speed. Interpolate eye pose between fixed simulation ticks. Support
   chase and driver's-eye rigs with controlled lookahead; the camera need not
   simulate vehicle dynamics. Capture time-stamped frames through outshine-client.
4. Stream ahead and evict behind under bounded memory. Keep graph/route IDs
   resident while render tiles and LOD change. A missing geometry tile is a
   visible/readiness defect, not a route change.

## Acceptance

- One complete lap returns to the start with no jump in world position,
  orientation or station; all route edges have legal connectivity and one
  consistent direction. Edge/tile seams meet within declared metre/radian
  tolerances derived from the alignment representation.
- Every frame records route edge/station, camera clearance, road coverage,
  missing tiles, p50/p95/p99 frame time and peak CPU/GPU memory. Compare the
  moving camera's projected road bounds with generated road/contact geometry;
  no gap, wrong-side lane or clipping at any sampled seam.
- Open PNGs at start, each distinct turn, pit split, any bridge/underpass and
  lap closure, plus a continuous capture. Judge optical plausibility of road,
  surroundings, light, scale and temporal stability. Report missing OSM data
  separately from generator defects. No single still frame closes this WI.
- Repeat cold/warm and at two road/render LODs. Identical route identity and
  camera alignment within numeric bounds; visually acceptable transitions.

WI 2261 uses this same route and scenario as the later dynamic vehicle test.
