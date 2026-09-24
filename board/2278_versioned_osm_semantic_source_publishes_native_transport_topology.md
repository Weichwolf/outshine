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
MVT z14 at Hockenheim omits `highway=raceway`; its feature IDs are not proven
OSM Way IDs. The authored scenario now declares a SHA-256-pinned semantic OSM
source, loaded and published as a native graph even without a renderer. The
remaining gap is bounded cancellation and route stability during focus movement.
Worldwide source-cell scheduling belongs to WI 2280. The snapped `Ground::VectorStreetGraph`
is only a road-corridor rendering input.

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
   A `sha256:` pin verifies local source bytes before parsing. The authored
   Hockenheim scenario must use the shipped asset, not a test-only path.
2. `world/data` owns byte acquisition, limits, parsing and chunk identity.
   Each regional chunk records dataset, revision and coverage. WI 2280 adds
   worldwide source-cell identity and residency. Merge
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
a render target; the authored Hockenheim scenario resolves its shipped OSM file
and verifies its SHA-256 pin. A snapshot resolves a circuit against its own
source revision; source and graph cannot be paired by the consumer. Read,
parse and graph times, source bytes, named-route counts and pending jobs reach
public diagnostics. Scenario routes resolve on the worker and publish with the
matching graph; authored Hockenheim resolves 267 directed edges.
Names describe those ownership boundaries; do not reintroduce parser or file IO
into `world/navigation`.

The remaining regional work is cancellation/backpressure and route consumption
by the camera. Keep at most one executing source build and one latest desired
request in the loader; a new valid request supersedes the desired state and
signals the executing worker to stop. Check the signal before IO, after read,
after parse and before graph/route construction. `Poll()` discards stale work
and schedules only the latest request, without waiting on the frame thread.
Report completed, canceled and pending work separately. Do not change the
general `Tasks` guarantee that an accepted job runs unless explicitly canceled.
Worldwide focus-based source-cell scheduling is WI 2280. Adjacent shuffled chunks, conflicting
source IDs, mixed revisions and corrected equal-sized replacement now pass the
worker-publication test. Prove route stability under mesh eviction and moving
focus before closing this WI; the camera lap stays in 2260.

## Route publication contract

- `Scenario::Document` declares named routes independently of views. Read/write
  `<routes><route id="grand-prix" source="osm" relationId="284588"/></routes>`.
  The OSM relation is an import selector; native consumers use the route name.
  Reject empty/duplicate names, zero or invalid relation IDs and unsupported
  source kinds before changing the running declaration.
- `world/navigation` owns immutable named routes. Resolve requests on the OSM
  worker against its complete source snapshot and publish graph, source revision
  and routes together. Bound route count and total edges before allocation;
  `advance()` never scans a relation. Failed replacement retains the prior
  graph and routes, with source-ID diagnostics for the rejected candidate.
- The headless authored Hockenheim scenario publishes `grand-prix` with 267
  directed edges. A corrected equal-sized revision replaces it; missing member,
  pitlane-only and reverse-direction controls fail. Route state stays valid
  when render tiles vanish. Camera traversal remains WI 2260.
