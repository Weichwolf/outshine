Type: bug
State: open
Parent: 1953
Area: client

# The gate runs the integration test it builds

**Benchmark** — Unreal: automation runs the binary it built, in CI. RAGE: the same. **Both agree** — a gate that compiles the client and never runs it proves the compiler.

`test/run.sh` compiles `apps/driver` and never runs it. `NAMED_ONLY="apps"` reserves it for a named
invocation, and naming it answers:

    run.sh: no declared suite under apps/driver/src

-- because a program is not a suite. So the one thing CLAUDE.md calls outshine's integration test
is only ever type-checked, and board:1963 stood for however long it stood without any gate noticing
that the client could not open its own subject.

**Unreal runs its templates in AUTOMATION and RAGE ran its map on every BUILD.** A build that
compiles the demo and never starts it is not testing the demo, it is testing the compiler.

**AND THE FAST GATE IS NOT AUTOMATION, which this item conflated.** Both benchmarks run the
binary in CI, not in the developer's inner loop -- Unreal's automation suite is not what a
programmer waits a minute for. `test/gate.sh` is that minute; `test/run.sh` is the full verdict,
and `NAMED_ONLY="apps"` already keeps clients out of the fast one. So the client is RUN, by the
full run, and the gate holds no `apps/` at all.

Measured, and it is why: the driver stood in the gate and hung. It held the gate for ten minutes
against its own thirty-four seconds, and the processes it left behind were not reaped by an
ordinary kill -- three of them, two hours old, holding the nest and the device, turning every
later measurement in that session into noise. A client is a PRODUCT, not a check; when it hangs
it must cost its own run and nothing else.

The blocker is real and belongs here rather than in a hasty fix: a drive fetches terrain and OSM
tiles, so a gate that runs one either reaches the network or ships a pinned cache. A pinned cache
is the answer the corpora already use -- a URL and a hash -- and it is what makes the run
deterministic rather than merely possible.

- [x] the client is RUN and not merely compiled, and what runs it is `test/run.sh apps` rather
      than the fast gate. `apps/bench` is where a client is MEASURED -- Khronos's own scenes
      through the door, with the engine's counters read rather than a stopwatch held outside it.
      proof: outshine/door/ScoreWhatTheDriveMeasures, and `sh test/gate.sh` naming apps/ among
      what it does NOT cover
- [ ] the tiles it needs are pinned and hashed, so the run is offline and deterministic. **The
      SUBJECT asset belongs to this predicate too** (from board:1509): the F31 is a CC-BY-4.0
      model fetched like any other corpus subject, and its attribution has to travel beside it.
      One pinning mechanism, not two -- the corpora already have the shape, a URL and a hash.
      **And board:1963's second predicate is this one** (inherited on its closure): a clean
      checkout DRIVES and writes its stills, but the terrain and OSM tiles still arrive from
      somewhere. What is declared in the tree is the scenario and the subject; what is fetched
      is the world under it.
- [x] the run fails the gate when the drive refuses -- moving `scene.gltf` aside turns it red
- [ ] the drive is its own suite rather than a case borrowing the door's, once a program can be
      one: `NAMED_ONLY="apps"` still answers "no declared suite under apps/driver/src"
