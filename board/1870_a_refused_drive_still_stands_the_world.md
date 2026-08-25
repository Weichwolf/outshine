Type: bug
State: open
Area: clients
Tags: driver, door, product, measured

# A declaration that refuses does not take the picture with it

`test/run.sh --drive` at 1af2c00b leaves **zero stills**. It prints:

```
DRIVING 48.13700,11.57600 -> 48.18000,11.62000, 1280x720, headless
REFUSED REFUSED the network holds both ends but no chain of ways joins them -- 20576 nodes of
64334 were reachable from the start, so this is a network in pieces and not a search that gave up
run.sh: 0 still(s) in /var/.../outshine-drive.xTDvbz
```

`Engine::Assemble` returns false when `Sim::AssembleDrive` refuses (src/clients/Engine.cpp:165),
the entry point exits before `RenderTo` (apps/driver/src/main.cpp:152), and nothing is ever
drawn. CLAUDE.md's rule is *a failure is loud; something is always drawn*: loud and blank are
not the same answer. The car, the sky and whatever ground stands are unaffected by a route that
did not join — refusing the DRIVE is right, refusing the FRAME is not.

The refusal also names itself twice — `REFUSED REFUSED` — because `Engine::Assemble` stores a
line that already begins with the word (Engine.cpp:35) and the caller prefixes it again.

Separately and underneath: the corridor's tile ring returns a network in pieces at 20576/64334
nodes, which is a content defect of the ring and belongs to board:1862.

## What will be true

- [ ] A drive that cannot be laid leaves the scenario STANDING: the frame renders, the still is
      written, and the refusal is published beside the picture rather than instead of it.
- [ ] `Engine::Drove()` answers false, and a client can ask WHY without parsing prose.
- [ ] No message carries a prefix twice.
- [ ] Negative control: a route between two disconnected coordinates -> stills are written, they
      show the world, and the drive reports its refusal by name.
