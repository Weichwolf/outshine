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

2026-09-18: inventory of every include/ and src/ module, reaches declarations, boundary
headers and selected implementations/callers. This is a structural/dependency audit,
not proof that every algorithm, race and failure path is correct. Line counts locate
mixed ownership; no maximum file length or automatic split is an architectural oracle.
Examples supplied by the user do not limit the audit to Live, Crown and Structure.

2026-09-23 lexical inventory: 261 `class` and 903 `struct` definitions (excluding
`enum class` and template parameters), 31 public headers/4,459 lines, 580 private
C++/GLSL files/94,275 lines, 22 declared dependency tiers and 384 public edges.
Layer check finds zero direction violations; it cannot prove runtime ownership.
Water triangulation now belongs to `generators/water` (2145). `Document.cpp`
(2,268 lines) contains several glTF sections but one import owner; split it only
when a complete codec boundary reduces change propagation. Track public API
growth, owner-crossing edits, dependency edges and largest cohesive files, not
a target count of types or an arbitrary line cap.
Ground telemetry belongs to `GroundDiagnostics`; earthwork contracts belong to
`EarthworkPress`. Buildings use `EastNorth` directly. Tests and lint pass; Wien
retains digest `e45d4da2`. Building footprint pieces now expose `Ring` and
`PartyWallEdges` instead of `Piece::P`/`Party`; shape, scratch and mesh agree.
Ten building tests, Wien digest `e45d4da2`, format and full lint pass.

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
| world/ | Geographic/provider/logical-world products fit here; provider configuration is native. Navigation is independent of visual geometry. | 2133 |
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

1. Candidate routing is implemented. Review its consumers rather than starting another facade.
2. The twelve-hour review through 21342822f found live derived-state invalidation in
   GroundStack::Restand (2224) and unbounded remaining terrain phases (2234).
   These are concrete correctness/cost tasks, not permission for generic module moves.
3. Material/terrain work 2171/2166 proceeds independently under the order in 2188.
WI 2188 maintains global priority against runtime defects. Vegetation features and a new
threading model are not part of this refactor.

## Re-audit after candidate routing

2026-09-21: `Live` is no longer an engine state facade. The remaining `Time.Live` occurrence
is scenario configuration. `StructureBuildQueue` and `StructureBuildTask` are correctly inside
`engine/streaming`: they schedule generator-owned `StructureBake` products and do not contain
building geometry algorithms. `CrownBuildIdentity` is a generated private provenance header
included through `OutshineGenerated/`, not a build-relative path. The layer contract finds no
parent-path, absolute or physical build include in product code.

`GroundBuildState` remains local to `Laying.cpp` because it coordinates one Engine-owned ground
candidate. Its phase order is now owned by `GroundBuildSchedule`; production cannot start before
candidate preparation and all sheet phases, and cannot pass Publication. That is a real state
contract, not a file split. Open ownership work is specific: WI 2225 must atomically replace
crown resources; WI 2228 must count renderer, candidate and worker residency; WI 2234 still
needs paced-versus-uninterrupted product equivalence and tail distributions. No generic engine
module move remains authorized by this parent.

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
Audit class, struct, function and file names in every owner slice: a name states the domain
object or operation, ownership and units when relevant. Reject generic wrappers, misleading
source identities and verbs that hide side effects. Migrate callers and tests with each rename.

For each slice migrate code, callers, build paths, tests and contracts together; remove the
old implementation and storage. No facade containing the old monolith, service locator,
parallel model or compatibility aliases. Do not invent an ECS/framework without a consumer.
References: local SDL fa2c02b for platform lifetime; existing native Geometry, candidate
owners and module contracts provide the project baseline. No proprietary architecture claim.

## Acceptance

- [ ] Concrete child slices remove the identified dependency and preserve completed products.
- [ ] External minimal client uses installed public headers/library; no checkout internals.
- [ ] Import/parser types do not govern runtime lifetime or subsystem execution.
- [ ] Direction checks reject a deliberate back edge, including through public headers.
- [ ] Candidate failure/retry, resource retirement, relevant analytical tests and client PNGs
      prove behavior; documentation alone is not completion of this parent.
- [ ] make format; affected make suite cases; make lint including clang-tidy.
