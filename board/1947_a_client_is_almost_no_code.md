Type: feature
State: open
Progress: client
Area: apps
Tags: benchmark, target

# A client is almost no code, and its line count measures the door

**Benchmark** — Unreal: a game project is content plus a launcher. RAGE: a build. **Neither ships a four-line client**, so the line count is this tree's own measure of its door.

Unreal's own sample games are thin over the engine module; the measure both benchmarks pass is
that a product needs no engine surgery. outshine states the rule in CLAUDE.md and fails it:
`apps/viewer` is 706 lines across two translation units and does not even LINK from the library
alone, which is declared in `EXPECT_UNLINKED`.

- [x] a program that does not link is a declared red with a reason, not a nameless one
      proof: --audit-link
- [ ] `apps/viewer/src/main.cpp` links from `liboutshine.a` and its entry point ALONE -- no
      `parts/` of its own (board:1898, board:1923's EXPECT_UNLINKED line goes with it)
- [ ] `apps/viewer` under 120 lines and the driver client (deleted) under 100 (board:1898)
- [ ] the door offers an asset-SWAP verb, so a client does not re-declare a scenario to change
      one subject (board:1898)
- [ ] shipped default lighting is a catalogue selection, so a client that wants a lit scene need
      not know how to light one (board:1898)
- [ ] the engine drives the fixed timestep when a client hands it the wall clock, so no client
      writes that loop (board:1898)
- [ ] the driver takes a VIEW by its declared name -- `Takes()` stands in the door and no client
      calls it (board:1865)
- [ ] a key press reaches a body: the driver offers a `Host` and binds it (board:1803)
