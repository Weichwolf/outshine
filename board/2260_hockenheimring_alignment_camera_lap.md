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

The pinned `src/assets/world/osm/HockenheimringGrandPrix.osm` resolves relation
284588 to 267 directed edges without the pitlane. VersaTiles MVT omits the
raceway, so the declared source supplies the semantic route. The normal
candidate publishes a DEM-based native alignment and road surface.

Current `b6bf3b0e1` full offline lap after typed admission: 13,200 frames,
4575.927 m, zero contact gaps/station regression, 538 cache hits, no misses or
provider starts. Independent TSV check: maximum position step 0.504150 m;
first/last-frame separation 0.000417 m. p50/p95/p99 2.720/8.922/12.780 ms;
39 frames over 16.667 ms; peak heap 885.9 MiB. All frames remain unrefined.
All twelve motion PNGs opened. Marks 2–4 show giant torn structure geometry;
final fast-forward/motion frames differ at 7348/921600 pixels despite equal
source counts. Functional/data evidence does not grant visual acceptance.
WI 2295 owns the corrupt publication/history case; WI 2298 owns product LOD.

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

## Remaining motion proof contract

SceneRenderer owns last-submitted camera basis/serial; update only after successful
GPU submission. Failure/minimized skip preserves the snapshot and serial.
Framing publishes this snapshot through existing Engine::measures. The client
requires a newly submitted serial and finite orthonormal pose per captured frame;
`CaptureCameraBasis.h` validates axes using public Camera::viewMatrix.
ScenarioCapture records submitted position/forward/up after rendering,
not a future simulation pose. Add finite/orthonormal checks and compare adjacent
basis rotation and lap closure independently of position/contact. Smooth analytic
turns pass; an injected orientation jump fails. Existing position-only TSVs do
not prove orientation continuity; separate contact/readiness from camera basis.
CPU oracle: `make suite SUITE=outshine/src/client/CaptureCameraBasis`.
Owners: SceneRenderer.{h,cpp}, engine/FrameMeasurements.cpp, engine/Framing.cpp, client/ScenarioCapture.cpp.
No additional public API or render-internal client dependency.
CPU basis/failure: 2 PASS; transpose control: FAIL. motion_trace.py checks full
clock/serial/contact, station/position/rotation steps and declared lap closure;
limits are explicit in metres/radians. `make test-motion-trace` has analytic
and jump/roll/stale/truncated/unfinished-lap controls; included in lint/fast gate.
Actual GPU submission/skip and captured-lap acceptance remain unverified.

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
