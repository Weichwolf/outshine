Type: bug
State: open
Parent: 1862
Area: clients
Tags: door, driver, measured

# Read -> Assemble -> Advance stands a picture, or it refuses at Read

**A COMMIT ANNOUNCED THIS CLOSED AND IT IS NOT.** `acede045` reads "board:1881 closed -- the
canvas comes first, and the present stage is in the plan", and four predicates below are still
unticked. The commit ran ahead of the work. A reader following `git log --grep 'board:1881'`
would conclude the opposite of what this file says, so the file says it here: the history is
wrong about this item and the directory is right. board:1988 is the guard that stops the next
one.

**Benchmark** — Unreal: `LoadMap` fails loudly and the engine stays in a known state. RAGE: a resource that does not map is a refusal. **Both agree** — a declaration that cannot stand refuses at READ, before anything half-built exists.

The front door says: **outshine loads a scenario and runs it.** Run exactly that way, it renders
nothing and says nothing until the third call.

Measured 2026-08-25 at a3ebe3e0 against `outshine-driver` (since deleted): `Assemble()` returned
TRUE and `Advance()` then refused "no scenario is standing" -- two arrival routes, one of which
stood no picture.

**Measured again 2026-08-31, and the route is worse than refusing.** Through the door as it stands
today -- `outshine-client run <scenario>` -- EVERY scenario file segfaulted:

```
build/outshine-client measures probe.xml
EXC_BAD_ACCESS (address=0x44)  libSDL3`METAL_DrawIndexedPrimitivesIndirect + 56
```

Bisected by deleting one section at a time: the killer is a scenario with **no `<lighting>`**.
`shots` never sees it because `ScenarioFor` sets `Lit.Declared = true` for every place -- a gate
blind to a path, and the corpus runner is built on the path it cannot see.

Cause: `Declaring.cpp` put the sun-from-clock computation INSIDE `if (scenario.Lit.Declared)`, so an
undeclared `<lighting>` meant no sun, no irradiance stage, a null `SkyIrradiance_`, and
`SubjectDraw` then skipped a fragment storage binding its own pipeline demands -- silently. The
driver dereferenced the hole. Repaired: the flag now gates only what the scenario STATES, and the
sun over a declared place stands from the clock either way, which is what the invariant already
said. The silent skip at src/render/stages/SubjectDraw.cpp:938 is still silent and is the second
half.

## What will be true

- [ ] `Read` stands what it read, or `Assemble` does — one arrival route, as the save path
      already argues in its own refusal at src/engine/Engine.cpp:450.
- [ ] `Assemble()` returning true and `Advance()` refusing "nothing is standing" is unspellable:
      the two agree by construction, not by call order.
- [ ] Proving case: a scenario that declares NO drive is read, assembled, advanced and captured,
      and the still carries the subject.
- [ ] Negative control: remove the stand from the arrival route -> `Assemble` refuses by name at
      the call that failed, never silently.
- [x] A scenario declaring no `<lighting>` renders instead of crashing -- the engine's own sun
      stands in the section's place. Measured: `measures probe_bare.xml` exit 139 -> exit 0, the
      nine places unmoved.
- [ ] `SubjectDraw` REFUSES by name when a pipeline wants a storage buffer nothing handed it,
      instead of binding nothing and letting the driver find the hole.
