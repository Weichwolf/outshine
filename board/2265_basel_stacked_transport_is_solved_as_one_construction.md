Type: proof
State: open
Parent: 2175
Depends: 2133, 2173, 2121, 2259
Priority: P1
Area: generators, terrain, navigation, simulation
Tags: osm, rail, bridge, tunnel, constraints, basel

# Basel Badischer Bahnhof constrains one connected 3D construction

## Independent input

Use raw OSM data, not Outshine's current mesh, for the Basel stress case.
The OSM API extract `bbox=7.598,47.560,7.616,47.574` fetched 2026-09-23
contains 27,522 nodes and 4,428 ways; SHA-256 of its 12,701,280-byte XML is
`4df25ec8ee07ea9f2f9b17260ae9122c06095a868848eb877384e1bcf12af3c8`.
Preserve a compact, attributed OSM fixture with source IDs, all referenced
nodes/tags and checksum before implementation. Runtime generation must not
special-case Basel or these IDs. Include connected halo ways beyond the crop.

The A2/A3 uses `highway=motorway`, `oneway=yes`, `lanes=3` in the
Schwarzwaldtunnel (`24537658`, `711566565`, `tunnel=yes`, `layer=-2`), then
ordinary segments (`52098661`, `24537659`) and southward bridge carriageways
(`32089898`, `32089899`, `bridge=yes`, `layer=1`, two lanes). Ramp `31585982`
is a one-lane bridge. This is a connected change of structure type, not three
independent elevation offsets. At about 47.562325, 7.610695, A2/A3 bridge
`32089899` crosses Bäumlihofstrasse `137063183` in XY without a shared node.
Around 47.5693, 7.6079, Maulbeerstrasse `1219622017` is `tunnel=yes`,
`layer=-1` beneath multiple `railway=rail`, `bridge=yes`, `gauge=1435` yard
tracks including `1219622011`; they share no route node. Station platforms
carry `ele=256`, but that does not fix every deck, tunnel or road height.
OSM does not specify the exact real piers, section dimensions or all datums.

## Construction decision

Parse OSM into directed, mode-specific topology and a separate spatial
crossing graph. Shared legal nodes define turns/switches; segment intersection
in XY creates a clearance constraint, never a route edge. `layer` defines
relative local order, not metres. Preserve `railway=disused/abandoned` as
distinct non-operational semantics, not live tracks.

Choose from a finite parametric grammar: ground alignment, graded approach,
bridge deck/support/abutment, cut, tunnel tube/portal, underpass, rail bed,
switch and station passage. OSM tags fix declared roles; missing structure
details admit a small ranked candidate set. Fit continuous horizontal and
vertical alignments for each connected corridor, coupling shared node pose,
tangent and lane/rail width. Solve heights jointly with terrain anchors,
explicit `ele`, grade/curvature limits, tunnel cover, deck thickness, clearance
and platform/rail geometry. Infer missing values with declared priors and
uncertainty. A local bounded constraint solve chooses a feasible candidate;
an infeasible system names feature IDs and violated bounds. Never use a fixed
height per `layer` or a mesh-intersection heuristic to create connectivity.

The accepted solution produces version-matched logical graph, 3D alignment,
terrain cut/fill/void requests, native render mesh, material and contact.
Only approach earthworks touch the ground under a free span; a tunnel opens a
volumetric passage. Road and rail contact follow the same alignment while
remaining different navigation modes. Tile cells bound work and residency;
structure constraints and shared seams span cell boundaries.

## Falsifiable acceptance

- Graph: A2/A3 and its legal ramps remain directed and connected through
  tunnel/open/bridge transitions. No car turns onto Maulbeerstrasse at an XY
  crossing or onto a rail. A train traverses live rail only. All survive
  render-LOD changes and tile eviction.
- Geometry: inspect portal, open transition, bridge deck/underside, underpass,
  rail bridge and street tunnel in section and above/below PNGs. Verify
  clearance, cover, grade, tangent and contact continuity over entire station
  intervals, including interior extrema and tile seams. Report inferred
  dimensions; do not claim to reproduce the unique real structure.
- Mutations: shared-node deletion breaks the route; a fake XY connection is
  rejected; a filled underpass, insufficient tunnel cover, too-steep ramp and
  rail/road mode leak fail their specific certificates. Vary tile order,
  candidate budget, seed and render LOD; hard constraints and route IDs remain.
- Measure cold/warm movement p50/p95/p99, frame tails, CPU/GPU memory and job
  limits. Use the Hockenheim camera/vehicle proof first; Basel then stresses
  the same generic pipeline with several overlapping structures.
