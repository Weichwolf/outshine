Type: proof
State: active
Architecture: ready
Parent: 2175
Depends: 2281
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

## Current input and output

`src/assets/world/osm/HockenheimringGrandPrix.osm` is the 33,557-byte pinned
source (SHA-256 `f50914eac077325eee1b3e88eae0eeffac58ff086e180cba4ec8734e94661d7f`).
Relation 284588 resolves 267 directed main-route edges; the pitlane is excluded.
VersaTiles MVT does not identify this circuit, so camera and road use the pinned
semantic route. The normal refined candidate now publishes its DEM-based native
alignment and surface. The opened `Hockenheimring-a9d234b1.png` shows a more
continuous circuit after one earthwork stamp per rendered segment. Segment
midpoints have contact; continuous clearance and moving-camera coverage remain
unproven. The overview is input diagnosis, not driving acceptance.

## Construction

1. Resolve circuit relation 284588 from a generic OSM semantic source; join
   its unroled member ways by directed node IDs into one closed route. Exclude
   `role=pitlane`, shortcuts and Rallycross. Verify 267 nodes each have one
   predecessor and successor in this pinned source. A missing member, reverse
   direction or ambiguous junction yields source-ID diagnostics, never an
   invisible camera teleport. DEM, transport and geometry still stream normally.
2. Route graph (2133) yields stable edge IDs independent of rendering. The
   bounded road candidate (2256) must publish complete matching geometry.
   The vector-tile corridor network in 2262 is a rendering intermediate, not
   the source-ID route; it cannot identify the raceway while its provider omits
   `highway=raceway`. Alignment (2175) maps each (edge ID, s, t) to double-precision position,
   tangent, bank and road width. Camera sampling uses this native alignment,
   not triangles, screen-space tracking or a separate authored spline.
   `Engine::routeInfo` and `Engine::sampleRoute` now expose the published
   immutable alignment in the camera's local frame, with length, closed state,
   segment ordinal, station, width and pose. An offline analytic-DEM test samples
   512 stations, the closing seam, invalid stations and a changed source revision.
   Camera logic must use this contract, not a mutable candidate or source tags.
3. Define a reproducible lap speed profile with bounded acceleration and
   curve speed. Interpolate eye pose between fixed simulation ticks. Support
   chase and driver's-eye rigs with controlled lookahead; the camera need not
   simulate vehicle dynamics. Capture time-stamped frames through outshine-client.
   Add a route-bound view only after the overview has made the route ready;
   moving focus may rebuild geometry but must retain route identity and pose.
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
