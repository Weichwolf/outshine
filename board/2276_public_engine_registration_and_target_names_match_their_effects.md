Type: debt
State: active
Architecture: ready
Parent: 2096
Depends:
Priority: P1
Area: include, engine, client
Tags: api, naming, ownership

# Public Engine target and registration names describe their effects

## Evidence

`include/Outshine.h` exposes `Engine::drawsInto` for both window and offscreen
target configuration, and `Engine::offers` for both host binding and generator
registration. These overloads select unrelated operations. Their Doxygen
explains the true effects, but a call site does not. The target switch already
has rollback and thread contracts; the generator and host remain borrowed.

## Binding API migration

- Rename both `drawsInto` overloads to `setRenderTarget`. Keep the overloaded
  input flexibility, SDL-window thread rule, no-open-frame precondition,
  previous-target rollback and offscreen pixel-extent semantics.
- Rename `offers(Host*)` to `setInputHost` and
  `offers(const Generators::Generator&)` to `registerGenerator`. These are
  different lifetime contracts; preserve null-detach and duplicate-kind
  behavior. Rename `Generators::Registry::offers` to `registerGenerator` as
  the same registration contract; preserve borrowed lifetime. No generic
  `offer` alias, extra facade or new ownership.
- Update declarations, definitions, all internal/client/test consumers,
  installed-header example and Doxygen in one change. Remove obsolete names;
  search all tracked C++ and documentation for remaining calls. Keep the
  public `Result`/`Holds` error contract and existing callback/thread rules.
- Do not rename `settled`, `standing`, `ships` or `logsTo` in this slice. Audit
  their semantics under 2096/2139 before assigning replacements. No blind
  project-wide lexical rewrite.

## Acceptance

- An external client using only installed public headers can configure both
  target kinds, bind/unbind its input host and register a generator.
- Existing target failure-injection tests retain prior target, pixels and
  error propagation. Registration tests retain borrowed lifetime and reject
  duplicate kinds. `make format`, focused API/client tests,
  `make test-client-render` and `make lint` pass on the changed commit.
