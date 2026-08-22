Type: bug
Area: clients
Tags: scope, hardening

**A failing resume leaves the park standing, and the full doorway's refusal names a discard
that exists**

The 1665 adjudication is right — a park is the only copy, the ninth park refuses. But the
code around that refusal contradicts its own premise three ways:

1. **`src/clients/Engine.cpp` `Resume` erases before it stands** (`Asleep.erase` precedes
   `Declare(taken)`): if `Declare` fails — the parked scenario's asset moved, the device
   refuses — the local copy `taken` dies on return and THE ONLY COPY IS DESTROYED by the
   very call that tried to revive it. Order demanded: declare first, erase from `Asleep`
   only on success.

2. **`Declare` silently discards the standing scenario** (`S_->Standing.reset()` before
   `Live::Open`): `Resume` over a standing scenario destroys it without a word —
   1485's own principle says "a scenario that vanished on somebody else's call is one
   nobody can reason about". At the full bound this is the ONLY path forward, so the
   state whose preservation motivated the 1665 flip is exactly what gets lost.

3. **The refusal names a verb that does not exist**: "resume or discard '<name>'" — the
   public handle (`include/outshine/Outshine.h:39-41`) has `Park/Resume/Parked` and no
   discard. To clear a door the client must `Resume` (destroying the standing scenario,
   see 2). Demand: `Discard(name)` (or `Forget`) that drops a parked scenario explicitly,
   so the refusal's remedy is spellable — and `Resume` while standing refuses ("park or
   discard what stands first") instead of silently replacing.

Proving test extends `test/render/outshine/client/AClientRunsAScenarioInFourLines.cpp`:
a resume whose declare fails leaves the name in `Parked()`; a resume over a standing
scenario refuses; the discard verb clears the full doorway and the ninth park then stands.

---

Closed: Resume declares FIRST and erases only on success -- a failed stand-up leaves the
only copy parked; Resume over a STANDING scenario refuses ("Resume stands nothing down");
and Discard(name) exists as the explicit verb the bound-refusal names, on the public door
and in CLAUDE.md. The four-lines proof walks all three: refusal over standing, explicit
discard clears a seat, the ninth room parks -- nothing lost by the engine's own hand.
