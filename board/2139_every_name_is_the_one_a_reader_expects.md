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

## Module decisions and implementation owners

| Module | Finding and binding decision | WI |
|---|---|---|
| include/ | Scenario.h composes public declarations. Provider, audio and render configuration now use native owners and transitive public-header edges are checked. Remaining public-door work is API scope/documentation. | 2096, 2131 |
| base/ | Math, geometry primitives, parsing and task infrastructure are reusable. Wayfinding owns transport constraints as well as search; keep generic graph math here, move transport policy to world/navigation. | 2133, 2124 |
| content/ | Own native assets and derived CPU artefacts/codecs, no GPU/engine/import dependencies. No second authoritative mesh representation. | 2150, 2237 |
| import/ | Geometry, materials, cameras and framing are native; the adapter fills native assets and remains behind the format-neutral loading boundary. Complete native playback ownership. | 2150 |
| scenario/ | Reader/writer belongs to import composition; TriggerField executes entity-time state and must leave the serialization module. Native input/action/view state is not parser state. | 2151, 2130 |
| engine/ | Live has lost resident resources, camera history, capture, sky integration, UI and material resolution; its final playback-policy boundary and RuntimeScene name remain. Laying/HeightSheets still contain integration beside extracted terrain algorithms. | 2236, 2237 |
| generators/ | Existing building/flora/road/water algorithms are appropriate; extract remaining terrain computation from engine. Preserve native outputs and independent library linkage. | 2237, 2150 |
| world/ | Geographic/provider/logical-world products fit here; provider configuration is native. Navigation is independent of visual geometry. | 2133 |
| render/ | SceneState/FrameResources/WorldContent separation is useful. SceneRenderer still exposes individual stage settings; narrow calls by coherent frame/world inputs, reuse existing owners. | 2222, 2223, 2236 |
| actor/ | Rigid/prismatic computation is a valid simulation kernel. Stateful bodies/triggers currently straddle scenario/engine; gather native simulation state before adding threads. | 2130 |
| audio/ | DSP graph/mixer configuration is native. Engine AudioOcclusion remains the wrong owner; acoustic BVH belongs to audio while CPU triangle BVH stays base. | 2212, 2130 |
| ui/ | Layout/style/paint/input are coherent responsibilities; large Layout.cpp alone proves no defect. Host input routing stays at engine boundary. | 2139 |
| host/ | Fetching implements transport; provider interpretation stays world/data. Bounded queue/cancellation and no blocking frame IO remain required. | 2124, 2210 |
| client/ | CLI, process setup, captures and file orchestration are valid; no duplicate renderer or engine algorithms. | 2195, 2207 |
| diagnostics/ | Process allocator replacement is valid only for explicitly opted-in executables, never linked into the engine library. | 2209 |
| assets/shaders | Declarative content and generated shader products remain outside runtime logic; packaging/provenance is an independent boundary. | 2207, 2152 |
| test/ | Mirror actual module ownership; public API/client proves integration, local tests prove algorithms. Layer checks now walk transitive public headers. | 2094 |

## Executable reserve and order

1. WI 2236: translate the last playback policy, verify remaining owners and rename Live to
   RuntimeScene without aliases.
2. WI 2237: finish Crown capture/streaming migration over the existing scene-resource boundary.
3. Re-audit this parent against the resulting code and close it or name a concrete remaining
   dependency; do not keep a generic refactor open as permanent permission to rearrange files.
WI 2188 maintains global priority against runtime defects. Vegetation features and a new
threading model are not part of this refactor.

## Common implementation contract

Generated private headers live under build/generated/<owner>/ and are exposed by a private
compiler include directory. Source includes use logical names, never relative paths into build/.
Migrate CrownBuild.h through the provenance generator and GroupIncludes together; preserve its
identity calculation and regenerate compile_commands. Shared GLSL/C++ atmospheric constants may
retain relative source includes where they follow render -> world and introduce no build edge.

Engine composes providers, generators, simulation, rendering, audio and UI. Domain modules
own algorithms and data. Cross-module interfaces carry native values, owned immutable
products or scoped borrows, not Engine::State/Live references. Published products have one
commit owner; subsystem extraction must preserve rollback, generation and GPU lifetime.
Use reaches plus resolved-header checks; do not add reverse edges to make a move compile.
Names follow the actual operation: Prepare/Commit/Submit/Read/Cancel, not Opens/Hands/Into
where they obscure ownership. Rename only alongside the owning consumer migration.

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
