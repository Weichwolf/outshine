Type: bug
State: active
Area: clients
Tags: measured, driver

# `--from` and `--to` shift the declared drive, as the help has always said

**Benchmark** — Unreal: a command-line override on a map's PlayerStart is applied to what the map declares. RAGE: a launch parameter modifies the loaded mission, never replaces it. **Both agree** — an override composes with the declaration; a switch that silently discards it is the shape neither ships.

`apps/driver --help` says it plainly:

    --from and --to are DELTAS on what the scenario declares: omit them and the drive the
    scenario declares is what runs

`main.cpp` assigned instead of adding, so a delta REPLACED the declared coordinate. Measured:

    ./build/outshine-driver --to 0.02,0.0
    DRIVING 0.00000,0.00000 -> 0.02000,0.00000
    REFUSED the fetched tiles decode to no feature

The declared drive starts at 48.13720,11.57560 in Munich. The run above drove from the Gulf
of Guinea, and the refusal it produced -- tiles that decode to no feature -- is true and
tells you nothing about the cause.

Worse in one direction: `asked.Routed` writes both pairs, so naming `--from` alone silently
zeroed the DESTINATION as well.

**This blocked board:1955.** That item records that no route this tree can drive exceeds the
tile pool's 64 MB budget, so the eviction path has never run, and it attributes the limit to
the road graph refusing a destination beyond three kilometres. The only tool for testing that
claim is `--to`, and `--to` did not do what it said.

- [ ] a delta shifts the declared coordinate rather than replacing it
- [ ] `--from` alone leaves the declared destination standing
