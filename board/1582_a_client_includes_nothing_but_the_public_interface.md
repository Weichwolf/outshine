Type: feature
State: open
Area: clients
Tags: scope, layering
Supersedes: 1525, 1619, 1635

# A client includes nothing but `include/outshine/`, and every module's include set is minimal

The rule is the owner's and the enforcement is the build's: when the include set cannot express
the breach, the rule needs no reviewer. Today `apps/driver/src` is declared with sixteen
`-Isrc/...` entries (test/run.sh:195) — the internals wholesale.

**Measured 2026-08-25 at 1af2c00b**: the SOURCE now obeys the rule and the BUILD does not.
`apps/driver/src/main.cpp` includes `<outshine/Outshine.h>` and `<outshine/Fetching.h>` and
nothing else, but test/run.sh:195 still opens `-Itools/host` beside the sixteen `-Isrc/...`,
so the door the driver walked through by hand is one `#include` away from being left open again.

The PROOF suite's own set is machine-kept since 2026-08-25: `outshine/client) printf '%s'
"-Iinclude" ;;` (test/run.sh:190) is asserted by
`harness/claims/TheLayeringIsDeclaredOnce`, so the case that shows a client needs nothing but
the door cannot be retired by widening the set it stands on. `apps/` and `tools/` are still
kept by a person, which is what this item is for.

**And the door is a warehouse, measured by the gate going red.** At b7ffe736 the fast gate says
`277 tests: 269 PASS 0 FAIL ... 8 BUILD`, and seven of the eight are the whole
`test/outshine/client` suite — the cases whose entire purpose is to prove a client needs
nothing but the door. They no longer LINK:

```
Undefined symbols for architecture arm64:
  "outshine::Sim::AssembleDrive(...)", referenced from: outshine::Engine::Assemble()
  "outshine::Sim::DriveTick(...)",     referenced from: outshine::Engine::Advance()
  "outshine::Data::ShippedProviders()",referenced from: outshine::Engine::Assemble()
  "outshine::Ground::TilePool::~TilePool()", referenced from: GroundStack::~GroundStack()
```

4d4981ec gave `Engine.cpp` hard dependencies on `src/sim`, `src/ground` and `src/data`, so every
client that links the door now links the sim, the ground stack and the provider table with it.
The proof of this item is the first thing that broke.

Three things stand in the way, and they are one motion:

- **the door is a warehouse.** `Engine::State` (src/clients/Engine.cpp) holds the render device,
  a `unique_ptr<Clients::Live>`, the frame extent and scenario bookkeeping beside the one part
  that belongs — the graph. Device, Live and Frame dissolve; the door keeps declaration in,
  graph owned, systems advanced, frame out.
- **SDL has no declared seam.** The core door stays SDL-free (the engine owns SDL init;
  `SDL_InitSubSystem` is refcounted so a host that initialised SDL coexists), and the GUEST path
  gets exactly ONE opt-in adapter, `include/outshine/HostSdl.h`: window or surface in, frame
  out, wrapping the present seam. It is the only public header that may spell an SDL type.
- **a module carries include sets it does not use.** Four modules carried `-Isrc/core` and none
  of them used it; removing it broke nothing, which is the proof it was a habit.

## What will be true

- [ ] The include sets for `apps/` and `tools/` shrink to `-Iinclude` plus the host seam, so a
      breach cannot compile.
- [ ] Every capability a client uses today reaches it through `include/outshine/`.
- [ ] Every module's declared set is MINIMAL, proven by removing each entry in turn and
      requiring the compile to fail.
