Type: feature
State: active
Architecture: ready
Parent: 2173
Depends:
Priority: P0
Area: world, data, navigation, engine, scenario
Tags: osm, streaming, source-identity, hockenheim

# Versioned OSM semantic source publishes native transport topology

## Problem and evidence

The runtime asks `Data::SourceSet` for Terrarium elevation and VersaTiles MVT.
`SourceProvider` chooses kind/revision/rank, but cannot name a semantic OSM
source. MVT z14 at Hockenheim omits `highway=raceway`; its feature IDs are not
proven OSM Way IDs. `OsmXmlReader` and `OsmElements::Merge` retain source
node/way/relation IDs, roles, tags and revision. `TransportTopology::Build`
and `ResolveCircuit` already prove the pinned 267-edge Grand Prix loop.
No running world requests, validates or publishes that graph. The snapped
`Ground::VectorStreetGraph` is only a road-corridor rendering input.

## Ownership and data flow

1. Extend `SourceProvider` for `kind="osm"` with `Dataset`, `Location`
   and `Coverage` (west/south/east/north degrees). XML uses `dataset`,
   `location`, `westDeg`, `southDeg`, `eastDeg`, `northDeg`, plus
   existing `pin`, `rank`, `whenAbsent="fail"`. Location is a local path,
   absolute or relative to `Roots.Shipped`; reject URI schemes. Split
   antimeridian coverage into two rows. Scenario reader/writer round-trip;
   reject missing revision, invalid
   coverage, duplicate rank/identity and unsupported location scheme before
   mutation. Existing terrain/vector/star declarations retain their behavior.
   A regional OSM XML file is the first adapter; source acquisition uses
   bounded IO/compute work off the frame path. Do not make a Place-name switch,
   an opaque `file://` curl trick or an unversioned whole-world XML document.
   First code slice: declaration validation, layer merge by kind/rank and
   round-trip, with a negative control for every required field. It does not
   claim a graph is published until the subsequent runtime slice passes.
2. `world/data` owns byte acquisition, limits, parsing and chunk identity.
   Each chunk records dataset, revision, spatial cell and coverage. Merge
   only identical dataset/revision; reject conflicting objects. Missing Way
   nodes or Relation members remain explicit until an enclosing source
   coverage is complete; they cannot become silent disconnected edges.
3. `world/navigation` builds an immutable `TransportTopology` from the
   complete semantic source snapshot. A world-owned candidate publishes
   source identity and graph atomically after validation. Earlier revision,
   canceled work or late chunks cannot overwrite the current publication.
   Keep graph/route identity resident independently of render tile LOD and
   `Ground::VectorStreetGraph`. Expose readiness and failure with source IDs.
4. Runtime consumers receive the native graph, never XML or MVT identities.
   The loader may retain original OSM IDs as stable provenance. Do not
   infer identity by XY snapping or by MVT feature-ID equality.

## Falsifiable acceptance

- A generic scenario selects the pinned Hockenheim XML source by declaration;
  the running world publishes a graph with the source dataset/revision and
  resolves relation 284588 to the same directed 267-edge loop, excluding
  the pitlane. No circuit name or relation ID is compiled into the provider.
- Two adjacent shuffled chunks with shared source nodes merge to the same
  edge IDs. Changed revision with equal element counts replaces the graph;
  stale response, cancellation and failed replacement retain the prior
  publication. Missing members, contradictory IDs and wrong revision fail
  with source-ID diagnostics.
- Renderer disabled, street-mesh eviction and vector-tile absence do not
  change the semantic graph or selected route. A synthetic XY crossing
  without a common OSM node stays disconnected.
- File IO, parse and graph construction have measured per-step time, bytes,
  queue depth and cancellation. No blocking file read or full parse on the
  simulation/render thread. Run focused source/topology/runtime tests,
  `make format`, `make lint`; Hockenheim camera acceptance remains WI 2260.

## Current implementation boundary

`SourceProvider` and scenario IO now validate and round-trip bounded, pinned
OSM chunks. `Data::OsmChunkSetLoader` owns byte limits, XML parse and merge;
`World::OsmTransportLoader` builds an immutable native graph on a worker and
atomically publishes a complete candidate. Runtime readiness exposes pending
and failed source states. The public groundless path works both with and without
a render target; the pinned Hockenheim relation has a focused graph test. Read,
parse and graph times, source bytes and pending jobs reach public diagnostics.
Names describe those ownership boundaries; do not reintroduce parser or file IO
into `world/navigation`.

The remaining work is spatial scheduling of source cells around moving focus,
real cancellation/backpressure when revisions overtake the two-job queue,
and query access for route consumers. Test adjacent shuffled chunks, revision
conflict, absent members and synthetic XY crossing independently of the pinned
track. Only then mark this WI complete; the camera lap stays in 2260.
