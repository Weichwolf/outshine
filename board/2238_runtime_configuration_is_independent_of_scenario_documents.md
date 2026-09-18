Type: refactor
State: active
Architecture: ready
Parent: 2139
Depends:
Priority: P0
Area: include, scenario, audio, render, world, test
Tags: api, modules, validation

# Runtime configuration is independent of scenario documents

## Evidence

src/audio/{Mixer,BusGraph,SignalGraph}.h, src/render/SceneRenderer.h and
src/world/data/DeclaredSources.h include scenario/Scenario.h. Their reaches declarations
omit scenario because these public-header dependencies are not an internal source edge.
Thus a green tier check does not prove scenario-independent runtime contracts.
Scenario.h also aggregates provider, world, audio, entity, physics, UI and camera settings.
This is not a reason to delete its public documentation or reject legitimate SDL adapters.

## Decision

Subsystems own small native configuration/value types. Scenario import owns syntax,
source diagnostics and declarative composition. It translates to those native types and
uses the same validation/creation operations as direct API clients; no parallel runtime
configuration model or two implementations of validation. Where semantics already match,
move the type to its owning module and use it directly in the scenario declaration.
Where serialization semantics differ, use an explicit boundary conversion.

Public headers follow ownership (audio, render, world, simulation); keep only types needed
by callers public. Scenario::Document composes these types. No subsystem receives an
entire document to extract a handful of fields. No serialization spellings/default lookup
or file IO inside the native audio/render/provider execution path.

Extend test/scripts/layer-contract.py to resolved public headers and transitive includes.
Public math/scene asset contracts are legitimate lower-tier inputs; classify them explicitly,
not by declaring every include/ path dependency-free. Check content of imports as well as
path names. Existing violations must become named migration findings, not broad exemptions.

## First complete slice

1. Reuse existing world/data/SourceDecl.h native types; add only missing provider fields.
   Move validation there; keep DeclaredSources Scenario-to-native conversion in scenario or engine
   composition boundary. Runtime providers consume only copied/owned validated declarations.
   Direct API and imported declaration use one validation operation with identical errors.
2. Add a focused dependency negative control: including a scenario-owned type from a provider
   fails the module check; legitimate math/scene types pass. Establish the full inventory
   before enforcing other unconverted consumers. Never weaken an already enforced edge.
3. Migrate audio graph configuration using the same rule, then renderer plan/settings.
   Preserve checked DSP/plan construction and candidate failure semantics; no new graph or
   public feature. Coordinate header names with WI 2096 and schema conversion with WI 2151.

`Data::SourceProvider` now owns revision, priority and typed missing-data policy in the public
world boundary. Scenario XML converts `pin`, `rank` and `whenAbsent` there; DeclaredSources,
GroundStack and source registration contain no Scenario provider type or schema-policy string.
SourceSet batch registration validates every source and duplicate `(kind, priority)` before one
publication, so a late failure leaves the prior set unchanged and a valid retry succeeds.

Commands: make format; run existing declared-provider tests with make suite and the
repository dependency checks through make lint. Later audio/renderer slices run their
focused suites, direct API/import equivalence and relevant frame/PCM regression controls.
Before implementation activate this WI. Do not make audio depend on scenario to appease
reaches, or use a private duplicate of Scenario::Document as the supposed native boundary.

## Acceptance

- [ ] Native provider construction compiles/runs without scenario headers or renderer.
- [ ] Invalid imported/direct inputs reject identically before mutation; retry succeeds.
- [ ] All source-to-public-header edges are accounted for; deliberate reversed dependency
      fails. Header relocation alone does not count if subsystem types remain importer-owned.
- [ ] Completed module slices have no Scenario:: type in runtime headers/operations;
      public client, serialization roundtrip, ABI/source changes and lifetime docs migrate.
- [ ] make lint including clang-tidy passes; no renderer/audio behavior changes are hidden
      as naming work. WI 2151 retains whole-scenario schema and roundtrip responsibility.
