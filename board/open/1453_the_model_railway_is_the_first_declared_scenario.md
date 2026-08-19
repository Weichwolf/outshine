Type: feature
Area: scenario
Tags: instrument

**The model railway is the first declared scenario**

A train that drives by itself, stops at the station now and then, and carries a panel somebody can
press. **It is the first integration test for scenarios**, and its job is to be the smallest thing that
needs every layer at once: assets from a provider, geometry from the glTF reader, actors that act, a
surface that draws, and a client that owns the window.

**EVERYTHING COMES OUT OF ONE DECLARED SCENARIO.** No code path exists to place the train, wind the
clock or wire the panel — the scenario says what is there, and the engine reads it. A scenario that
needed a line of C++ per object would be a level editor written in a compiler.

## What one declaration names

| | |
|---|---|
| **assets** | glTF, fetched by digest through the provider interface and cached like every other subject |
| **actors** | a script and the host it is bound to, with the `Ref` that says WHICH train it drives |
| **surfaces** | a document, and only where a thing needs a face — `board:1452` |
| **the clock** | the declared step, because a scenario that read a wall clock is two scenarios |

## A script is bound to a HOST and not to a surface

**The two are independent and that is the owner's distinction.** A train needs a script and no
interface; a passcode panel needs both; a sign on a wall needs a surface and no script. So the binding
is *script → host* and *surface → texture*, and a thing may declare either, both or neither. Making a
script live inside a document would have made every actor carry a page it does not draw.

## What must be true

- [ ] **One scenario file names the whole run**, and running it twice produces the same frames — which
  is what `CLAUDE.md` means by the mathematics being deterministic, applied to a world that moves
- [ ] **The train's script is ticked with the declared step**, never with elapsed time, so the second
  run is the first one
- [ ] **The panel's script reaches the door and nothing else**, through a `Ref` its own host gave it
- [ ] **The scenario suite decides it**: p50/p95/p99 over a moving camera, determinism across two runs,
  residency and memory across a long one. `CLAUDE.md` names that suite as the one still ahead, and this
  is the run that makes it exist

## Depends

`board:1448` for the actors, `board:1452` for the panel's surface. **Neither blocks writing the
declaration**, which is the part worth doing first: a scenario format nobody has written a scenario in
is a format nobody has tested.
