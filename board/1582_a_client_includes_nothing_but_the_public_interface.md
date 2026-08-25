Type: feature
State: open
Area: clients
Tags: scope, layering
Supersedes: 1525, 1619, 1635

# A client includes nothing but `include/outshine/`, and every module's include set is minimal

The rule is the owner's and the enforcement is the build's: when the include set cannot express
the breach, the rule needs no reviewer. Today `apps/driver/src` is declared with sixteen
`-Isrc/...` entries (test/run.sh:194) — the internals wholesale.

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
