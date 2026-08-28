Type: bug
State: active
Area: engine, door
Tags: measured

# a ground that does not compose is a REFUSAL, and today it is a cleared error

**Benchmark** — Unreal: `UWorld::InitializeActorsForPlay` fails loudly when a streaming level it
was told to load does not; a missing level is never a silently empty world. RAGE: a map node that
fails to stream is an assert in development builds. **Both agree** — a world asked for and not
delivered is an error where it happens, not a note somebody may read later.

`src/engine/Engine.cpp:157`:

    if (!Composes()) {
      Session.Carried.push_back("the ground did not compose: " + Error);
      Error.clear();
    }

**The refusal is swallowed by design.** `Error` is cleared, `Assemble()` returns true, and the
client is told everything stood. The reason survives only in `Session.Carried`, which nothing in
the door reports.

**MEASURED, by five places on Earth.** `test/outshine/places/` declares Shibuya, the Grand Canyon,
Venice, the Black Forest and an old town, advances each eight steps at 1280x720 and keeps a
screenshot. All five read:

    0 tile(s), 0 triangle(s), ~500 000 of 921 600 pixel(s) differ

Half a million differing pixels and **not one triangle of Earth**. The differing pixels are the sky
gradient, which is why the first version of that case -- which asserted only "more than one colour"
-- passed on all five. A false floor, caught by looking at the picture.

`Assemble()` succeeded. `Advance()` succeeded. `RenderTo()` succeeded. Nothing refused.

## Why the swallow exists, and the narrower thing it should do

`Composes()` returns false for TWO different situations: *nothing was declared to compose* -- a
scenario with neither a sphere nor a drive, which is fine -- and *what was declared could not be
composed*, which is not. The swallow was written for the first and takes the second with it.

- [ ] `Composes()` returns true when there is nothing to compose, so a false is always a refusal
- [ ] `Assemble()` refuses when a declared ground does not compose, and says why
- [ ] the five places report UNPREPARED with the reason rather than PASS with an empty sky

**The measurement that would show I am wrong:** if some scenario in the tree relies on assembling
past a failed compose, this change turns it red and that red is the argument. `test/run.sh outshine`
is the walk; it was 79 PASS before this.
