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

`src/assets/places/Hockenheimring.scenario` declares an overview and a
route-bound lap view. The camera follows the native alignment by station;
the overview is a data gate, not lap acceptance.

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
`outshine-client run --view lap --at-seconds 300 --into places
src/assets/places/Hockenheimring.scenario HockenheimLapEnd` captures the
published-road view at a metric simulation time. Opened captures at 0, 10, 15,
100, 200 and 300 s show motion on the road; 300 s reaches station 4575.880 m.
Start/end PNGs differ in 602 of 921600 pixels (0.0653%). A candidate-world
readback mismatch exposed at 15 s is fixed and has an independent regression.
The road is still flat grey; side strips and crude buildings are visually
obvious. These captures do not prove continuous road contact or LOD stability.

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
   `Motion::RouteSpeedProfile` plans a finite, bounded time-to-station curve
   from native pose samples. `placement="route"` binds a first- or third-person
   rig only after publication; a pinned-OSM/analytic-DEM test checks every
   tick of the 267-edge lap against `sampleRoute`, bounded station/eye steps,
   startup refusal and view reselection.
   Remaining: presentation interpolation, per-frame road-coverage/contact
   proof, refined streaming and visual acceptance. A paced 220 s real-DEM
   capture traverses all 267 segments and 4575.927 m by 213.833 s. Station
   never regresses; maximum eye step is 0.5042 m/tick. Chord-length-weighted
   grade caps observed vertical eye acceleration at 4.755 m/s², zero frames
   over 1 g, versus 36.865 m/s² and five frames before. All eleven PNGs
   were opened: road is visible but uniformly grey; mark 4 retains a vast dark
   wall and road shading waves, with flat ground, abrupt distance edges and
   crude buildings. The updated profiled-earthwork warm offline 220 s run
   reaches station 4575.927 m; 7170/13200 frames are unrefined, 31 exceed
   16.667 ms, p50/p95/p99 is 2.033/8.651/12.721 ms and heap peaks at
   642.145 MiB. Setup takes 829 ms including 371 ms preload; the provider
   serves 538/538 source reads from disk, 28.6 MB, with no remote start.
   These measurements do not prove per-frame road contact or Kaltstart speed.
   Warm offline 10 s A/B moves Refined candidate start from 9.10 to 1.93 s.
   Full 220 s after bounded water admission: 5,962/13,200 unrefined;
   p50/p95/p99 2.016/9.102/13.314 ms, 40 late, 585.1 MiB peak,
   538/538 cache hits and zero remote starts. Render spikes reach 79.52 ms.
   All eleven PNGs opened: mark 3 now shows a Refined near-black foreground
   beneath a vast wall; mark 4 retains the wall. The final PNG differs in
   50/921,600 pixels. Road contact, frame tail and independent publication
   remain open; earlier Refined visibility is not visual acceptance.
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
