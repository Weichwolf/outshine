Type: debt
State: active
Area: build, include, engine, generators, audio
Parent: 2188
Depends: 2209

# Runtime errors are explicit without exceptions

## Problem

Engine runtime and generators must compile without C++ exceptions. A recoverable error
must be local, owned and checked; a mutable diagnostic string or an invalid sentinel
as the only error channel couples unrelated calls and cannot describe the preserved
state. Fatal system-wide OOM is not a recoverable `expected` error.

`EntityRegistry` exposed this flaw publicly: `open`, relation/tag/seat mutations
returned `bool`; `addEntity` and `instantiate` overloaded `kNoEntity`; callers then
borrowed `error()`. `GltfImporter` likewise returned `expected` while retaining a
second, mutable public `error()` channel. Both make ignored failures easy and let a
later call replace a diagnostic. Query absence remains a value (`optional`, false or
zero count), not an error.

## Decision

Compile runtime, generators and tests with `-fno-exceptions`; the client process
adapter translates exceptions from explicitly isolated foreign code before entering
the runtime. Use `[[nodiscard]] std::expected<T, Error>` at public and subsystem
boundaries where an input, capacity, IO, GPU, provider or callback failure is
recoverable. Use compact typed errors where callers branch on cause; format diagnostic
text at the boundary. `noexcept` states a proved nonthrowing contract only.

Migrate `EntityRegistry` mutations to typed expected results, including creation and
instantiation. `GltfImporter` returns its owned `expected` diagnostic as its sole public
failure channel. Successful mutations publish complete state; errors preserve slots,
relations, tags, seats, valid handles and imported snapshots. Remove public `error()`
only after every consumer uses the returned error. Keep query APIs allocation-free and
distinguish absent results from rejected requests. The scenario assembler maps registry
errors to its owned engine result without global registry diagnostics.

Setup allocates bounded scratch before publication. Audio graph/mixer preparation,
streaming and geometry baking use candidates and retain the running state on rejection.
Audio quality and backend choice belong to 2212; native asset publication to 2216;
world candidate publication to 2191/2224.

## Evidence

- `make db` has 189 units and runtime/generator units use `-fno-exceptions`.
- `SceneRenderer::Init` already returns `[[nodiscard]] expected` and its GPU error
  path is tested.
- `TagCatalogue::under` is constexpr expected with static negative checks.
- `EntityRegistry` public mutations now return owned `RegistryError` expected results;
  creation and instantiation no longer overload `kNoEntity`, and public `error()` is gone.
  Header, entity-column and simulation contract suites cover success, rejection and state reuse.
- `GltfImporter` now exposes only its owned `expected` diagnostics; failed loading,
  variant and animation operations preserve the published asset and their returned errors
  survive subsequent successful operations. Its focused public suite passes.
- The direct glTF client render path propagates each Engine `Result` at the operation
  boundary; it no longer derives a capture failure from `Engine::error()`.
- Direct clang-tidy run after this migration: 189/189 units, zero findings. The full
  lint gate remains blocked separately by the external immutable reference cache (2226).

## Proof

- Compiler-negative cases reject ignored relevant expected results.
- Registry invalid capacity, role, handle, relation, tag, seat and exhaustion return
  the typed cause and leave the complete prior state usable; retry after a rejection
  succeeds where capacity permits.
- Foreign callbacks, filesystem and GPU failures cross the runtime boundary as owned
  errors; no exception enters an engine frame or audio block.
- Allocation/IO occurs before realtime publication; prepared frame and audio paths
  have bounded work and no routine allocation.
- `make format`, affected suites and `make lint` run after every migration step.
