Type: refactor
State: active
Architecture: ready
Parent: 2139
Depends:
Priority: P0
Area: engine, generators, world, render, content
Tags: ownership, modules, streaming

# Procedural products are separate from streaming and GPU residency

## Evidence and correct distinction

HeightSheets combines refinement/earthworks/mesh assembly with Live-bound uploads.
Laying.cpp mixes palette/classification, water/terrain generation and candidate scheduling.
ImpostorPreparation creates a SceneRenderer plus Core::RuntimeScene for every atlas view.
Render::ImpostorInstances now owns render handles, card-view selection and atomic instance batches.
WorldCrowns starts generation/capture jobs and installs those renderer instances.
StructureBuildQueue and StructureBuildTask schedule, retain inputs and consume results:
these are integration responsibilities, not meshing algorithms merely because of their names.

## Concrete destinations and dependency direction

| Existing responsibility | Owner and destination |
|---|---|
| HeightSheets::Press, refinement, mesh production | generators/terrain; immutable sampled inputs -> native products |
| Laying terrain/water algorithms | generators/terrain and generators/water; no Engine::State |
| StructureBake algorithms | existing generators/building; retain existing native output |
| Structure build revision, cancellation, scratch lease | engine/streaming/StructureBuildQueue and StructureBuildTask |
| StructureTilePublication / terrain publication | engine/world; sole coherent candidate commit |
| Stored atlas data/codec | existing content/impostor/ImpostorAtlas |
| ImpostorPreparation GPU capture | render/impostor/ImpostorBaker; native geometry/instances, one preparation renderer |
| Tree prototype generation | existing generators/flora; no GPU/renderer dependency |
| CrownCache transport | world/data/ImpostorCache; content codec and existing bounded IO |
| Impostor view selection and instances | render/impostor/ImpostorInstances; existing scene-resource handles |
| WorldCrowns species resolution, admission, completion | engine/streaming/VegetationStreaming; narrow owners |

No renderer dependency in generators or content. No engine dependency in render/world.
The integration layer may call both generator and renderer, but owns neither algorithm.
Only native products cross that boundary; do not move Core::RuntimeScene into generators to
make an include compile. Keep immutable producer inputs alive through task completion.
Stale results are rejected before the existing candidate commit; old world remains usable.

## First independent slice, then consumers

1. [x] Extract HeightSheets::Press and its private computational helpers into terrain generation.
   Input borrows immutable constraints plus exclusively owned candidate height storage;
   return contact diagnostics and mutate only that private storage, not an extra world copy.
   Reuse PressPoints and Patchwork/Sheet contracts. Pass grid side/halo layout explicitly:
   the algorithm must not read Render::GroundLattice constants. Preserve halo coordinates,
   ECEF/ENU conversion, order, precision and refusal behavior before candidate publication.
2. [x] Move refinement/mesh algorithms by the same rule; retain IO resolution and renderer
   resource application in separate integration adapters. Align continuation boundaries
   with WI 2234; do not duplicate the scheduler or change its atomic product contract.
3. [x] Move Structure scheduling into engine/streaming with truthful names and mirrored tests;
   retain current range bounds, worker ownership and publication proof from WI 2231.
4. [x] Rename/move CrownPieces to Render::ImpostorInstances under render/impostor. Keep its
   typed PieceHandle ownership, all-view atomic SetPieceInstances batch and release semantics;
   WorldCrowns stores that render owner and contains no card construction or raw handle logic.
5. [ ] Implement Render::ImpostorBaker over native bark geometry plus optional leaf geometry and
   instance transforms. Create/configure one preparation renderer and resource set per atlas,
   then vary only the camera across views. It owns plan, GPU completion and readback conversion;
   it does not accept TreePrototype or Core::RuntimeScene. Engine integration grows TreePrototype once,
   translates the native capture input and publishes the unchanged Content::ImpostorAtlas.
6. [ ] After those consumers move, rename/move WorldCrowns to engine/streaming/
   VegetationStreaming. Preserve its bounded IO/preparation tasks, cache state machine, failure
   retention and destructor join. No vegetation feature or atlas algorithm change in this WI.

No vegetation feature expansion or new atlas algorithm here. Existing renderer-backed
preparation stays usable while its ownership is separated. Do not use a file move as proof.

`Generators::PressTerrain` now owns the complete pad/corridor earthwork operation over
private `Patchwork` storage. Its explicit side/halo layout removes GroundLattice from the
algorithm. Flat-pad, sloped-corridor and invalid-layout controls run without Engine, SDL or
renderer; the unchanged public floor-contact Place and standalone generator-link claim pass.
HeightSheets retains sampling, refinement and resource application; those
remaining responsibilities are the next slices, so this WI is not complete.

`Generators::BuildTerrainMesh` now converts candidate pages to the native indexed mesh and
relief range without Engine or renderer ownership. `TerrainPageLayout` owns the shared explicit
side/halo addressing used by pressing and meshing. HeightSheets no longer owns mesh assembly;
sampling, halo resolution and GPU application remain. The analytical quad fixes winding,
visible-node range and zoom filtering; the public floor-contact Place remains green.

`Generators::RefineTerrain` now selects complete native patches from borrowed immutable
height fields and returns a private candidate. Engine integration only resolves cached fields
and commits a successful result. Flat, high-error subdivision, deterministic child identity,
budget rejection and provider-free virtual passthrough are analytical controls. HeightSheets
still resolves/halos streamed samples and applies pages; no provider or GPU owner moved.

`StructureBuildQueue` now owns admission, revisions, scratch leases and landing preparation
under `engine/streaming`; `StructureBuildTask` owns each bounded worker continuation. Building
meshing remains in `generators/building`, and atomic candidate publication remains separate.
The moved lifetime, stale-revision and atomic-publication tests preserve those boundaries.

`Content::ImpostorAtlas` is the sole stored crown artifact and owns validation plus the
versioned binary codec under `content/impostor`. Cache, streaming and preparation exchange that
neutral type directly; the hand-authored fixture proves byte compatibility and corruption bounds
without renderer setup. GPU capture remains Engine integration and imports Core::RuntimeScene; card
derivation and GPU instance ownership are now both Render-owned. WorldCrowns retains only the
integration owner and stores `std::unique_ptr<Render::ImpostorInstances>`.
`Data::ImpostorCache` now owns bounded asynchronous artifact transport under `world/data`; it
depends only on the content artifact, ContentStore and Tasks, not Engine preparation or rendering.
`Render::BuildImpostorCard` now derives native card geometry and material maps under
`render/impostor`; its analytical test fixes coverage padding, tangent-space normals and separate
metallic/roughness channels without opening a renderer or Engine runtime.

## Acceptance and commands

- [ ] Terrain computation runs without Engine, SDL or renderer; analytical flat, slope,
      footprint and corridor tests match the existing completed native products/contacts.
- [ ] Injected generator failure leaves the published tile/world unchanged; retry succeeds.
- [ ] New reaches edges obey the table, no back-reference or umbrella engine include.
- [ ] Existing atlas codec/corruption, instance and worker-lifetime tests migrate with owners;
      generated and imported native geometry use the same resource installation boundary.
- [ ] make format; make suite SUITE=outshine/src/engine/streaming/StructureBuildTask;
      make suite SUITE=outshine/integration/places/ScoreAFootprintStandsOnALevelFloor;
      moved focused suites via make suite; make lint. Inspect unchanged terrain/crown PNGs
      through the client for their respective migration slices, without enabling new features.
