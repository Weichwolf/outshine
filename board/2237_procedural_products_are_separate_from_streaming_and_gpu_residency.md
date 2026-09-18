Type: refactor
State: open
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
CrownAtlas mixes a binary codec, native geometry construction and a Live/SceneRenderer
capture. WorldCrowns starts generation/capture jobs and installs renderer instances.
StructureBakes and StructureBakeTask mostly schedule, retain inputs and consume results:
these are integration responsibilities, not meshing algorithms merely because of their names.

## Concrete destinations and dependency direction

| Existing responsibility | Owner and destination |
|---|---|
| HeightSheets::Press, refinement, mesh production | generators/terrain; immutable sampled inputs -> native products |
| Laying terrain/water algorithms | generators/terrain and generators/water; no Engine::State |
| StructureBake algorithms | existing generators/building; retain existing native output |
| StructureBakes/Task revision, cancellation, scratch lease | engine/streaming/StructureBuildQueue and StructureBuildTask |
| StructureTilePublication / terrain publication | engine/world; sole coherent candidate commit |
| CrownAtlas codec and stored atlas data | content/impostor/ImpostorAtlas; no TreeSpecies, Live or GPU |
| CrownAtlas::Bake GPU capture | render/impostor/ImpostorBaker; native Geometry input, existing renderer |
| Tree prototype generation | existing generators/flora; no GPU/renderer dependency |
| CrownCache transport | world/data/ImpostorCache; content codec and existing bounded IO |
| CrownPieces view selection and instances | render/impostor/ImpostorInstances; scene-resource handles |
| WorldCrowns species resolution, admission, completion | engine/streaming/VegetationStreaming; narrow owners |

No renderer dependency in generators or content. No engine dependency in render/world.
The integration layer may call both generator and renderer, but owns neither algorithm.
Only native products cross that boundary; do not move Core::Live into generators to
make an include compile. Keep immutable producer inputs alive through task completion.
Stale results are rejected before the existing candidate commit; old world remains usable.

## First independent slice, then consumers

1. Extract HeightSheets::Press and its private computational helpers into terrain generation.
   Input borrows immutable constraints plus exclusively owned candidate height storage;
   return contact diagnostics and mutate only that private storage, not an extra world copy.
   Reuse PressPoints and Patchwork/Sheet contracts. Pass grid side/halo layout explicitly:
   the algorithm must not read Render::GroundLattice constants. Preserve halo coordinates,
   ECEF/ENU conversion, order, precision and refusal behavior before candidate publication.
2. Move refinement/mesh algorithms by the same rule; retain IO resolution and renderer
   resource application in separate integration adapters. Align continuation boundaries
   with WI 2234; do not duplicate the scheduler or change its atomic product contract.
3. Move Structure scheduling into engine/streaming with truthful names and mirrored tests;
   retain current range bounds, worker ownership and publication proof from WI 2231.
4. Split CrownAtlas data/codec from GPU capture, then move cache and instance consumers.
   Preserve atlas wire-format/version and update crown-provenance inputs with file moves.
   GPU capture runs only in explicit preparation, never a normal frame. Reuse WI 2236's
   resource API when migrating Live-bound consumers; codec/algorithm extraction is independent.

No vegetation feature expansion or new atlas algorithm here. Existing renderer-backed
preparation stays usable while its ownership is separated. Do not use a file move as proof.

## Acceptance and commands

- [ ] Terrain computation runs without Engine, SDL or renderer; analytical flat, slope,
      footprint and corridor tests match the existing completed native products/contacts.
- [ ] Injected generator failure leaves the published tile/world unchanged; retry succeeds.
- [ ] New reaches edges obey the table, no back-reference or umbrella engine include.
- [ ] Existing atlas codec/corruption, instance and worker-lifetime tests migrate with owners;
      generated and imported native geometry use the same resource installation boundary.
- [ ] make format; make suite SUITE=outshine/src/engine/StructureBakeTask;
      make suite SUITE=outshine/integration/places/ScoreAFootprintStandsOnALevelFloor;
      moved focused suites via make suite; make lint. Inspect unchanged terrain/crown PNGs
      through the client for their respective migration slices, without enabling new features.
