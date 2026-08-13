Type: feature
Area: render
Tags: oracle, perf

**I.1 The machine and the process**

- [ ] One declared target whose feature set is fixed and uses no vendor extension — **SDL3 · SDL_GPU · modern C++ · an A18 Pro at 720p60** (`CLAUDE.md` § *The constraints*). *This line read "wasm32 + WebGPU as the fixed target" and was ticked until 2026-08-12; `b83285f` deleted the emcc half of the build, so the target it was ticked for no longer exists, and the one that replaces it is not built either — the frame oracle links native Dawn (`Makefile` `NATIVE_BUILD`, `-lwebgpu_dawn`), which is on neither list. The requirement is that there be **exactly one** and that something build it, not which one it is. Earned at § I.19's SDL_GPU port (`board/active/` step 11).*
- [ ] Native translation as frame oracle (`make walk`) and a **second** translation from one source list — **one translation today**. `b83285f` deleted `make wasm`, `AppWasm.cpp` and the emdawnwebgpu port, and nothing replaced them; the point of the line is that one source list serves more than one host, which is what stops a target-only defect (§ I.18, § I.21). Re-earned when the `host/` layer has a second implementation
- [x] One object owns world and renderer and is the only thing that builds a scene (`clients/Outshine`)
- [x] A client is `main()` plus an output medium and nothing else
- [x] Server target that links no `render/` and needs no device (`make world`, the deleted world entry point)
- [x] Layering enforced by targets that stop building, never by a checker (`verify-generators`, `verify-world`, `verify-clients`, `verify-types`)
- [x] `core/` is I/O-free by directory; `generators/` cannot spell renderer, world or log
- [x] Declared internal render resolution 1280×720; the canvas only scales it
- [ ] Aspect-preserving letterboxing on a canvas of another shape — declared in the deleted architecture document, not found in `PresentStage`
- [x] Bring-up phases as an enumeration rather than booleans
- [ ] Fallible asynchronous bring-up completed outside a constructor, everywhere (`C.41`) — partially held, not audited
- [ ] A gate that fails the build on an unreferenced non-static symbol — `core/Mat4.h` sat entirely dead behind a comment asserting it was not, and no target noticed
- [ ] **A gym: the simulation with no renderer attached, running as fast as the machine allows.** Minutes of world time in a second, so a scenario can be soaked rather than watched. `make world` already links no `render/`, which is half of it; what is missing is a client that steps a declared scenario to a verdict instead of serving tiles
- [ ] The gym holds **no graphics device at all** — DECIDABLE, and the old spec's own check is the right one: no device symbol in the binary, verified with `nm`
- [ ] **Wall-clock speed does not change the result.** Run the same scenario throttled and unthrottled and the fingerprints match — this is principle 7 ("if pace decides the result, the coupling is a bug") made checkable, and it is the reason a gym is worth having beyond speed
- [ ] **Thread count does not change the result** — identical fingerprint over 1…N threads across repetitions. The old gym parallelised exactly one phase and left the rest sequential, which is what made that testable
- [ ] A run's fingerprint as one comparable value, so "the same" and "different" are a string compare rather than a reading
- [ ] The gym runs with no network, from what is on disk
- [ ] **Stability soak as a declared run**: the circling aeroplane and the Payerne circuit flown for hours of world time in minutes of wall time, with drift, growth and blow-up as the verdict. This is what those two scenarios are *for* — a body that flies for ten seconds proves nothing about one that flies for ten hours
- [ ] Frame loop that survives a device loss and re-creates the swap chain
- [ ] Pause / resume without the world losing residency
