Type: bug
State: open
Parent: 1953
Depends: 1963
Area: client

# The gate runs the integration test it builds

`test/run.sh` compiles `apps/driver` and never runs it. `NAMED_ONLY="apps"` reserves it for a named
invocation, and naming it answers:

    run.sh: no declared suite under apps/driver/src

-- because a program is not a suite. So the one thing CLAUDE.md calls outshine's integration test
is only ever type-checked, and board:1963 stood for however long it stood without any gate noticing
that the client could not open its own subject.

**Unreal runs its templates in automation and RAGE ran its map on every build.** A build that
compiles the demo and never starts it is not testing the demo, it is testing the compiler.

The blocker is real and belongs here rather than in a hasty fix: a drive fetches terrain and OSM
tiles, so a gate that runs one either reaches the network or ships a pinned cache. A pinned cache
is the answer the corpora already use -- a URL and a hash -- and it is what makes the run
deterministic rather than merely possible.

- [x] the gate runs the drive headless for a bounded number of frames and reads what it measured
      proof: harness/outshine/door/ScoreWhatTheDriveMeasures
- [ ] the tiles it needs are pinned and hashed, so the run is offline and deterministic
- [x] the run fails the gate when the drive refuses -- moving `scene.gltf` aside turns it red
- [ ] the drive is its own suite rather than a case borrowing the door's, once a program can be
      one: `NAMED_ONLY="apps"` still answers "no declared suite under apps/driver/src"
