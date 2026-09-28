Type: debt
State: active
Architecture: ready
Area: include, engine, world, render, generators, base
Tags: architecture, owner
Parent: 2188
Depends:
Priority: P0

# Module boundaries expose real responsibilities and ownership

## Audit scope and limits

Inventory covers include/, src/, reaches and boundary callers; dependency checks alone do
not prove ownership or runtime correctness. File length locates mixed responsibilities,
not an automatic split threshold. User examples do not limit this audit.
Water triangulation belongs to generators/water (2145). Split Document.cpp only along a
complete codec boundary. Track API growth, owner-crossing edits and cohesive responsibilities.

## Module decisions and implementation owners

| Module | Finding and binding decision | WI |
|---|---|---|
| include/ | Scenario.h composes public declarations. Provider, audio and render configuration now use native owners and transitive public-header edges are checked. Remaining public-door work is API scope/documentation. | 2096, 2131 |
| base/ | Math, geometry primitives, parsing and task infrastructure are reusable. Wayfinding owns transport constraints as well as search; keep generic graph math here, move transport policy to world/navigation. | 2133, 2124 |
| content/ | Own native assets and derived CPU artefacts/codecs, no GPU/engine/import dependencies. No second authoritative mesh representation. | 2150 |
| import/ | Geometry, materials, cameras and framing are native; the adapter fills native assets and remains behind the format-neutral loading boundary. Complete native playback ownership. | 2150 |
| scenario/ | Reader/writer belongs to import composition; TriggerField executes entity-time state and must leave the serialization module. Native input/action/view state is not parser state. | 2151, 2130 |
| engine/ | RuntimeScene coordinates playback and rendering. Terrain computation, resource residency and streaming scheduling have distinct owners; candidate editing still needs a narrow renderer boundary. | 2223, 2191 |
| generators/ | Building/flora/road/water/terrain algorithms return native CPU products without Engine or renderer dependencies. Preserve independent library linkage. | 2150 |
| world/ | Provider and OSM-ID topology are separate from geometry. The vector-street corridor builder consumes snapped `StreetField` for meshing; it belongs to ground, while authoritative navigation consumes source IDs. | 2133, 2262 |
| render/ | SceneState/FrameResources/WorldContent separation is useful. SceneRenderer still exposes individual stage settings; narrow calls by coherent frame/world inputs, reuse existing owners. | 2222, 2223 |
| actor/ | Rigid/prismatic computation is a valid simulation kernel. Stateful bodies/triggers currently straddle scenario/engine; gather native simulation state before adding threads. | 2130 |
| audio/ | DSP graph/mixer configuration is native. Engine AudioOcclusion remains the wrong owner; acoustic BVH belongs to audio while CPU triangle BVH stays base. | 2212, 2130 |
| ui/ | Layout/style/paint/input are coherent responsibilities; large Layout.cpp alone proves no defect. Host input routing stays at engine boundary. | 2139 |
| host/ | Fetching implements transport; provider interpretation stays world/data. Bounded queue/cancellation and no blocking frame IO remain required. | 2124, 2210 |
| client/ | CLI, process setup, captures and file orchestration are valid; no duplicate renderer or engine algorithms. | 2195, 2207 |
| diagnostics/ | Process allocator replacement is valid only for explicitly opted-in executables, never linked into the engine library. | 2209 |
| assets/shaders | Declarative content and generated shader products remain outside runtime logic; packaging/provenance is an independent boundary. | 2207, 2152 |
| test/ | Mirror actual module ownership; public API/client proves integration, local tests prove algorithms. Layer checks now walk transitive public headers. | 2094 |

## Executable reserve and order

Current slice: `GroundBuildSchedule` names the current phase and its state transition;
`Engine::State` names ground candidate operations by what repeated calls actually advance.
Migrate declarations, callers and schedule tests together. A formerly `BeginsGround*` call
must continue to be safe while pending and must preserve candidate retry/publication order.
1. The listed `Engine::State` verbs are migrated. Continue public API naming
   under 2096. Runtime body-instance resizing is now named at both the scene
   and render proxy boundary; keep names tied to operations, not metaphors.
   Render-proxy setters must name reset, material copies, borrowed previous positions,
   emission, placement, environment assignment and light insertion explicitly. RuntimeScene
   owns subject preparation; the client owns platform/engine initialization. Migrate all
   declarations and callers without aliases; preserve reset, borrow and failure behavior.
   Implemented: ResetForShape, SetMaterials, SetEmission, SetPlacement, BindPreviousPositions,
   AddLight, SetEnvironment, PrepareSubject and InitializeClientEngine; no aliases.
   Verify SubjectProxy/Lens, format and full lint. RuntimeScene GPU tests remain unverified:
   SDL video initialization is denied by sandbox XPC; some fixtures are unprepared. Continue
   Implemented: DiagnosticLedger::ConfigureMetrics/BeginFrame/RecordMetric/Samples/
   ConflictingMetricNames, TilePieces::SetSurfaces and Shipping::EnsureCatalogue.
   All callers and forward declarations migrated; no compatibility aliases. Catalogue atomic
   replacement and public diagnostic isolation suites pass. Other owner names remain open.
2. The twelve-hour review through 21342822f found live derived-state invalidation in
   GroundStack::Restand (2224) and unbounded remaining terrain phases (2234).
3. Material/terrain work 2171/2166 proceeds independently under the order in 2188.
WI 2188 maintains global priority against runtime defects. Vegetation features and a new
threading model are not part of this refactor.

## Re-audit after candidate routing

`Live` is no longer an engine state facade. `StructureBuildQueue` and
`StructureBuildTask` schedule generator-owned products from `engine/streaming`.
The layer contract rejects parent-path, absolute and physical build includes.
`GroundBuildState` remains local to `Laying.cpp` because it coordinates one Engine-owned ground
candidate. Its phase order is now owned by `GroundBuildSchedule`; production cannot start before
candidate preparation and all sheet phases, and cannot pass Publication. That is a real state
contract, not a file split. Open ownership work is specific: WI 2225 must atomically replace
crown resources; WI 2228 must count renderer, candidate and worker residency; WI 2234 still
needs paced-versus-uninterrupted product equivalence and tail distributions. Move further engine
owners when a named responsibility and caller migration demonstrate a real boundary violation.

## Common implementation contract

Generated private headers live under `build/generated/OutshineGenerated/` and are exposed only
through the private `-Ibuild/generated` include root of their owning tier. Source includes use
logical names, never relative paths into build/. `CrownBuildIdentity.h` is generated by the
provenance producer and included as `OutshineGenerated/CrownBuildIdentity.h`; layer-contract
rejects physical build paths, parent-directory paths and absolute paths in source, headers and
tests. C++ and GLSL include logical names from declared include roots; parent-directory paths
are forbidden even when the dependency is allowed.
Engine composes providers, generators, simulation, rendering, audio and UI. Domain modules
own algorithms and data. Cross-module interfaces carry native values, owned immutable
products or scoped borrows, not Engine::State/Live references. `SubjectPlacementHistory` names
the last placement-buffer upload; rendered vertex history remains separately owned by
`RuntimeScene` and advances only after a successful frame submission. Published products have
one commit owner; subsystem extraction must preserve rollback, generation and GPU lifetime.
Use reaches plus resolved-header checks; do not add reverse edges to make a move compile.
Audit names in every owner slice: state the domain operation, ownership and units. Reject misleading
source identities and verbs that hide side effects. Terrain publication uses `PublishDelivery`;
cache reads and IO use `ReadCachedDelivery` and `FetchDelivery`. Migrate all callers and tests.
For each slice migrate code, callers, build paths, tests and contracts together; remove the
old implementation and storage. No facade containing the old monolith, service locator,
parallel model or compatibility aliases. Do not invent an ECS/framework without a consumer.
The route-height seam is a current naming/ownership example: `SourcedTerrainFields`
names the immutable worker input, `RoadTerrainPinJob` owns bounded DEM selection,
and `HeightSheets` only snapshots candidate fields. Keep future names similarly
literal: an API that copies, pins, publishes or retires must say so; migrate every
caller when correcting it. Do not preserve poetic verbs through aliases.
References: local SDL fa2c02b; native Geometry, candidate owners and module contracts.

## Acceptance

- [ ] Concrete child slices remove the identified dependency and preserve completed products.
- [ ] External minimal client uses installed public headers/library; no checkout internals.
- [ ] Import/parser types do not govern runtime lifetime or subsystem execution.
- [ ] Direction checks reject a deliberate back edge, including through public headers.
- [ ] Candidate failure/retry, resource retirement, relevant analytical tests and client PNGs
      prove behavior; documentation alone is not completion of this parent.
- [ ] make format; affected make suite cases; make lint including clang-tidy.
