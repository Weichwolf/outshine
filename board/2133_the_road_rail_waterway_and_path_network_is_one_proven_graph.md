Type: feature
State: open
Area: world, generators, engine
Tags: architecture, owner
Depends: 2162
Lab rank: 3

# The road, rail, WATERWAY and path network is ONE PROVEN graph a simulation can route on

**Benchmark** -- RAGE: `paths.ipl` -- vehicle nodes and links with lane counts, direction, speed
and junction flags, a separate pedestrian graph, both queried by the AI every frame and both
DERIVED from the road geometry at build time. Unreal: `ZoneGraph` is a lane-based graph for
vehicles and crowds and `NavMesh` for walkers, both built from the level's geometry by a
builder. **Both agree**: the network is a GRAPH with lanes and direction, built once from the
same source the geometry is built from, and the simulation routes on the graph and never on
the mesh.

## Where it stands, measured 2026-09-04

```
  src/base/spatial/Wayfinding.h   Path::Network: ways laid as segments, crossings found -- for
                                  BRIDGES, not for routing
  Engine::State::Routes()         exists; the driver that routed on it is gone (board:2117)
  OSM layers read                 buildings, water polygons, water lines, streets, street polygons
  railways                        NOT READ -- no layer, no way, no station
  paths                           read as streets of a class; no separate walker graph
  lanes, direction, one-way       StreetField::Way carries a class and a width; direction not kept
```

The road geometry is derived per rebuild from `StreetField` (board:2101); what a simulation
needs beside it is the GRAPH: nodes where ways meet, edges with lanes and direction, the
junction's turning relations, and a rail graph with the same shape and its own rules.

## The solution

One `Network` type in the engine's world tier, built by the road generator from the same
`StreetField` it meshes, in the same per-tile bake (board:2122) and stitched across tile edges
by the shared OSM node ids `WayEndKey` already quantises:

| | |
|---|---|
| **node** | an OSM node where two or more ways meet, or a way's end; a place and a height from the lattice (board:2121) |
| **edge** | one way between two nodes: class, lane count, direction (`oneway`), width, the corridor's centreline |
| **lane** | an offset along the edge's centreline; a vehicle drives a lane, a walker a footway edge |
| **junction** | the node's turning relations: which lane continues into which, derived from the geometry |
| **rail** | the same three types from the transportation layer's `rail` class, with track, gauge and no lanes |
| **query** | `Route(from, to, mode)`; A* with the class's speed as the cost inside a ring, and CONTRACTION HIERARCHIES (OSRM's answer, Geisberger 2008) for a route across a country -- a worldwide sandbox routes worldwide; `Nearest(place, mode)` to snap a body onto the network |

The graph is part of the SNAPSHOT (board:2130): the simulation reads it, the renderer never
does, and a tile that leaves takes its nodes with it while the edges to a resident tile stay
stitched.

The vector source is assumed to be the tile server already declared (VersaTiles' transportation
layer carries `class=rail`); if a place's source has no rail, the graph has no rail there and
says so.

## What will be true

- [ ] Every way OSM supplies stands in the graph with its class, lanes and direction; rails are
      read and stand beside them
- [ ] `Route(from, to, Vehicle)` and `Route(from, to, Walker)` answer over a whole ring and
      across tile seams; a case routes across a tile boundary and the route is continuous
- [ ] A body snapped to the network follows a lane's centreline at the lattice's height, so a
      driven body sits on the road it was routed along; the case that proves it is the one
      board:2127's contact needs
- [ ] The graph is built in the tile's bake and costs the tile, never the ring
- [ ] Negative control: drop the stitch across tile edges and the cross-boundary route case
      goes RED

## What will show I was wrong

A junction whose turning relations cannot be derived from the geometry alone -- a grade-
separated interchange whose ramps OSM tags but does not draw as connected. Then the relation
comes from the tags (`junction`, `turn:lanes`) and the graph carries a second source, named.

## Cited 2026-09-05: SUMO's netconvert is the readable network builder

CARLA's Digital Twin Tool builds its network with SUMO's `netconvert` (EPL-2.0, readable):
`NIImporter_OpenStreetMap` reads ways through `osmNetconvert.typ.xml` (per `highway=*` and
`railway=*`: lanes, speed, priority, one-way, permissions -- road, rail, tram, path in ONE
table), `NBNodeCont` joins nodes, `NBEdgeCont` resolves lanes and connections, and the same
graph carries rail. That table and that join are this item's baseline; board:2101 carries the
per-tile form.

## Decided 2026-09-05: the driving surface is the MAP's profile, analytic and C1, never a mesh

Decided with the owner: a car's errors must stand well under a centimetre, so nothing a car
drives on may be a polygon. The map holds, per lane, a continuous elevation profile
(OpenDRIVE's cubic elevation polynomials, C1 -- the vertical curve returns HERE as a
polynomial and not as a relaxation of a mesh), queried analytically as (s, t) -> height, and
a junction's plane meets its legs' profiles without a kink. The structure's mesh pictures
that profile; its deviation from the profile is a measured number a case reads, and a car
never sees it. CARLA's Traffic Manager plans on the OpenDRIVE map while its vehicles roll on
the mesh at 0.5 m vertex distance; Jolt takes an analytic height function as a contact
surface (board:2127), so this tree can do better than CARLA by one step.


## The water is in it, and so is the proof (2026-09-07)

**FOUR MODES, ONE GRAPH.** OSM states all four and a simulation needs all four: `highway=*` for
the road and the footway, `railway=*` for the rail, `waterway=*` for the navigable water. They
share nodes where they meet -- a level crossing, a bridge, a lock, a ford, a quay -- and a graph
that holds only the road cannot answer where a boat goes or where a train crosses a street.

**THE GRAPH IS 2D AND FLAT, AND THAT IS THE POINT.** Out of sight there is no vehicle anybody can
watch, so a height nobody sees and nothing touches is a number with no consumer (board:2162). The
graph carries position, class, lanes, direction, speed and connectivity; the third dimension
belongs to the geometry, which exists only where the eye is.

**A NETWORK IS PROVEN OR IT IS A DRAWING.** These are the claims, and each has a control that
must go red:

| | claim | negative control |
|---|---|---|
| **N1** | the graph is PLANAR where it is planar: two edges cross only at a shared node, or at a node whose `layer`/`bridge`/`tunnel` says they do not meet | inject a crossing without a node -> red |
| **N2** | every edge is SIMPLE -- no self-intersection, no zero-length segment, no repeated node | collapse two nodes -> red |
| **N3** | CONNECTED as the data allows: every component is reachable, and a component that is not is REPORTED with its size and its cause, never silently kept | drop one join -> the count rises -> red |
| **N4** | a junction is WELL-FORMED: every leg has a distinct bearing, the legs are ordered by bearing, and the turn set is complete and free of duplicates | duplicate a leg -> red |
| **N5** | every edge crossing a tile border arrives in the neighbour, and a border node has exactly ONE owner (Valhalla's rule) | let two tiles write a border node -> red |
| **N6** | routing is CONSISTENT: a route's length equals the sum of its edges, and reversing it costs the same on a two-way edge | perturb one edge's length -> red |

**And the readable references decide the SHAPE, not us.** Valhalla (MIT) for the tiled addressing
and the hierarchy levels; OSRM (BSD-2) for the border-node overlay; SUMO's `netconvert` (EPL-2.0)
for the type table that turns `highway=*` into lanes, speed and permissions, and for
`NBNodeShapeComputer`'s junction polygon. Nothing about the graph's shape is invented here.
