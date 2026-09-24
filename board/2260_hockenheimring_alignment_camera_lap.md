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
   rig only after publication; one pinned-OSM/analytic-DEM test checks 60 fixed
   ticks against `Engine::sampleRoute`, startup refusal, and view reselection.
   Remaining: presentation interpolation, per-frame road-coverage/contact
   proof, refined streaming and visual acceptance. A paced 300 s capture
   traversed all 267 segments and 4575.880 m by 214.233 s. Station never
   regressed, advanced at most 0.5 m/tick, and eye movement stayed below
   0.505 m/tick. Only 2378/12854 moving frames were refined; p99 CPU
   advance+render was 11.552 ms, 32 moving frames exceeded 16.667 ms, and
   heap peaked at 488.025 MiB. `outshine-client run --motion --samples --view
   lap --at-seconds 220 --into places src/assets/places/Hockenheimring.scenario
   HockenheimSamples` saves route-decile PNGs and a per-frame TSV. All eleven
   PNGs were opened: road remains visible, but uniformly grey asphalt, flat
   ground colors, abrupt distant edges and crude dark buildings fail visual
   acceptance. The sampled run had 10474/13200 unrefined frames, p99 12.281 ms,
   32 over-budget frames and 483.033 MiB peak heap. A fresh offline 20 s lap
   starts only two candidates (Playable, Refined), stays unrefined for all 1200
   frames, and ends in corridors at station 408 m. The stage ledger reports
   1716 network and 456 corridor advances, with 100/101 structure tiles landed.
   Candidate restart is not the first cause. A graph worker cuts unrefined
   frames to 1074/1200; p99 is 14.09 ms with seven over-budget frames, and
   the world is Refined at station 408 m. Corridor and later stages still delay
   most of the lap. Measure their source scope and wall path; publish bounded
   road/terrain products independently of distant MVT work where safe. Preserve
   revisions and frame budgets rather than raising the per-frame work quota.
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
