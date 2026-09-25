Type: proof
State: active
Architecture: ready
Parent: 2175
Depends: 2281, 2291
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

The pinned `src/assets/world/osm/HockenheimringGrandPrix.osm` resolves relation
284588 to 267 directed edges without the pitlane. VersaTiles MVT omits the
raceway, so the declared source supplies the semantic route. The normal
candidate publishes a DEM-based native alignment and road surface.

Warm/offline 220-s capture: 13,200 frames, 4575.927 m, no station regression,
0 centre/near-edge contact gaps, eye clearance 1.513–1.555 m. p50/p95/p99:
2.085/9.111/13.319 ms; 47 frames over 16.667 ms; 5,979 unrefined frames;
629.7 MiB peak heap; 538/538 cache hits, no remote starts. The trace records
all contacts and frame times. All eleven route-mark PNGs and the final PNG were
opened. Mark 3 has a near-black foreground despite valid road contact; mark 4
has an implausibly vast building wall. Grey asphalt, flat ground, schematic
buildings and abrupt distance transitions still fail visual acceptance.
The Refined pixel probe now confirms the dark mark-3 foreground in the paced
run but a bright static render at the same pose/time; a rare static mark-4
capture drew enormous overhead polygons while the paced run did not. WI 2295
owns this path-dependent geometry/shadow defect. Contact and frame-time proofs
remain valid, but Refined alone does not yet imply a stable image.
Ein neuer warm/offline 220-s-Lauf nach der semantischen Footprint-Trennung
(`f1a305e22`) und Quellnormalisierung (`5b2e5b633`) hat erneut 13 200 Frames,
0 fehlende Fahrbahnkontakte und 538/538 Cache-Treffer. p50/p95/p99 sind
2.277/9.810/14.466 ms, 69 Frames über 16.667 ms, 548.1 MiB Peak-Heap.
Aber 11 962 Frames sind nicht Refined, gegenüber 5 979 zuvor. Alle elf
Markierungen wurden geöffnet: Mark 3 hat keine schwarze Fahrbahn mehr; lange
repetitive Gebäude bei Mark 3/4/7 und monotone Boden-/Straßenflächen bleiben.
Die höhere Unrefined-Zahl ist eine Regression, keine Abnahme (WI 2298).

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
   `Motion::RouteSpeedProfile` plans a bounded time-to-station curve.
   `placement="route"` binds the camera after publication. A pinned-OSM/
   analytic-DEM test covers all 267 edges, startup refusal and view reselection;
   the public road-contact query reads published native triangles. Presentation
   interpolation, projected-road coverage, refined streaming and visual
   acceptance remain open.
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
WI 2296 extends the proven route/vehicle into a 24-hour day/night/weather
integration race; neither replaces this camera-lap acceptance.
