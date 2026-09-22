Type: proof
State: ready
Architecture: ready
Parent: 2175
Depends: 2133, 2256, 2262
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

The repository does not yet contain this scenario. The existing
`src/assets/drive/f31.scenario` contains a root `<drive>` that the current
reader rejects; follower camera placement exists, but no scenario contract
currently proves a route-bound camera. Do not revive the rejected element as
an opaque shortcut. Extend the declarative scenario/API contract for a named
route and a camera rig driven by (edge ID, station, lateral offset), or an
equivalent format-independent native route handle.

## Construction

1. Pin a local OSM/DEM input region and source identities. Identify the main
   `highway=raceway` loop from OSM topology; pit lane, service roads, paths and
   public roads remain distinct. Use explicit start/waypoint/direction only
   where OSM admits more than one legal loop. Missing/ambiguous connectivity
   yields a diagnostic with source IDs, never an invisible camera teleport.
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
