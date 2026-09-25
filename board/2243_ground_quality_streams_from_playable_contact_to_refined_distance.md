Type: feature
State: active
Architecture: ready
Parent: 2105
Depends:
Priority: P0
Area: engine, world, streaming
Tags: terrain, structures, lod, publication

# Ground quality streams from playable contact to refined distance

## Evidence

The 2026-09-21 cold floor-contact case has its requested 48 terrain meshes after
5.1-6.9 s, yet complete fine-DEM seating for every visible building still exceeds
15 s. With bounded four-candidate admission, 24 structure tiles complete while 13
source requests remain. Their workers start within 0.075 ms; compute scheduling is
not the blocker. Requiring one fine product for the contact area and the entire 8 km
view serializes playability behind distant refinement.

The first staged implementation publishes a `Playable` request from exact contact
coverage while retaining `Refined` for normal streaming. It bounds structure admission
to one candidate before first publication. A cold floor run fell from 15.45 s (timeout)
to 8.58 s preload after removing synchronous DEM acquisition from road draping and
requesting only the central vector tile before first publication; all six contact
assertions pass. Cold Lattice passes in 10.19 s. Ring 3 begins after `Playable`, and its
mutable producer counters no longer invalidate the atomically published snapshot.
Crossing sweep reuse costs 0.095 ms. Sampling heights
for 1,891 crossings had cost 2.35 s because a field miss called `StitchedFieldAwaited`.
`FieldUpM` now reads prepared candidate fields only and falls back to its candidate BVH.

2026-09-22 pacing failure: `Focuses` admitted a candidate while terrain meshes
were still pending. `LayPatchwork` returned zero Sheets; `PrepareFields` created
zero requests and corridor draping failed without a DEM field. The candidate now
waits for its contact mesh and rejects terminal zero-sheet terrain. The unchanged
pacing oracle passes normal and validated modes.

The vector-generation revision gate exposes the next timing case: Refined can
start with a ready contact mesh while other requested terrain meshes are still
pending. Roads from the new complete OSM snapshot then reach outside prepared
DEM fields. Require all requested terrain meshes to settle before admitting a
Refined patchwork; Playable still waits only for contact. Keep the unchanged
pacing oracle and inspect refined completion time.

Warm offline Hockenheim: 600/600 frames unrefined at 10 s; Refined starts
at 9.10 s. Water: 49 tiles, 1,795 deferrals, 489 disk hits, zero remote.
Irrelevant features spend the 128-step quota. Bound cheap scans separately
from 128 height queries within 2 ms; preserve source order and profiles.

## Decision

Ground residency has explicit quality, coverage and revision. The first publishable
candidate contains exact camera/contact terrain, collision and structures inside a
declared near radius plus a coherent coarser product outside it. It may omit unloaded
distant structures; it may not invent fine samples, expose holes as settled ground or
mark coarse contact as final. Navigation and logical OSM data remain independent.

Every structure records the terrain product and sampling resolution used for seating.
Fine DEM arrival creates a new native structure/ground candidate from immutable inputs.
It atomically replaces the prior candidate through GroundPublication; no vertex patching,
in-place height change or mixed contact revision is visible. Stable source identity and
priority are camera distance, required contact coverage, source zoom and tile identity.
Movement cancels obsolete refinement without cancelling the current playable world.

Readiness distinguishes `Playable` from `Refined`. `Playable` requires the complete near
contact contract and all resources needed for capture at that quality. `Refined` requires
the scenario's requested visual coverage. Callers choose the required readiness; existing
tests retain their declared geometric tolerances. A timeout reports which coverage and
quality remain missing. Quality reduction follows a measured frame/memory/IO budget and
is deterministic for the same request, never an implicit response to arrival order.

`Engine::settled(WorldQuality)` is the public snapshot for this choice. The default remains
`Playable`; tests and clients requiring final visual coverage must request `Refined`
explicitly. `preload()` still targets Playable and does not claim final refinement.
Normal advancement now publishes Playable first as well: before any publication, the
streaming request intentionally contains contact coverage only and therefore cannot be
labelled Refined. Subsequent advancement requests and publishes the complete ring.
Structure seating first uses the exact fine DEM field. Outside fine residency it
samples the candidate's prepared height hierarchy instead of deferring forever;
contact coverage still receives fine fields. The eventual fine arrival remains a
revision-triggered replacement.

The warm Hockenheim lap reaches 4575.880 m in 13200 frames. `NeedsBakes` had
serially preceded `NeedsCorridors`, although corridors consume classes, road
graph and DEM fields, not baked building pieces. Bakes now admit and land
during corridor work; `NeedsBakes` remains a completion gate before earthworks
and publication. Both retain candidate DEM fields until complete. In 20 s,
unrefined frames fall from about 1075 to 958 with an identical 921600-pixel
end image. Over the full lap, they fall from 8021 to 7169; p99 changes from
13.81 to 13.91 ms, over-budget frames stay at 59, and peak heap rises from
547.5 to 621.7 MiB. The full-lap end images differ in 101 pixels at the
distant horizon despite Refined readiness. Inspect distant structure residency
and publication determinism before claiming exact visual equivalence.

## Implementation

1. Extend the native ground revision/request with required near radius, target coverage
   and minimum/final source resolution. Derive defaults from camera, collision and sight;
   expose no format-specific type.
2. Split BuildingField completion into required coverage and full refinement. Reuse the
   distance-ordered TileWatermark and bounded dependency window from WI 2233. A completed
   far tile must not block a nearer required tile.
3. Make GroundWorldCandidate carry the exact terrain-quality revision used by terrain,
   footprints, collision and render pieces. Reject mixed revisions at publication.
4. Continue refinement after `Playable`; publish B only when its complete native products
   pass the same validation as A. Bound retained A+B bytes under WI 2228.

## Acceptance

- [x] Cold floor-contact becomes Playable within 15 s with its 0.01 m contact checks
      unchanged; no timeout increase, missing geometry or forced test-only source.
- [x] Cold Lattice becomes Playable within 15 s with its seam checks unchanged.
- [ ] A distant delayed DEM cannot block near contact publication. Its later arrival
      produces one atomic revision change and no intermediate mixed frame.
- [ ] Camera movement cancels obsolete B while A remains capturable; repeated inputs
      produce identical quality choices and native products.
- [ ] Full requested refinement still completes and matches the uninterrupted fine oracle.
- [ ] Record cold/warm p50/p95/p99, bytes for A and A+B, and quality/coverage at timeout.
- [ ] make format; focused ground/publication suites; unchanged floor-contact and Lattice
      integration cases; make lint.
