Type: defect
State: open
Area: engine
Tags: determinism, places

# The same place renders the SAME picture twice

**Benchmark** — Unreal: a cooked build with a fixed tick and no streaming in flight renders identically; its automation screenshots compare bit for bit with a tolerance and a NON-deterministic frame is treated as a bug in the streaming order rather than as noise to be tolerated. RAGE: the replay system depends on it -- a recorded drive replays frame for frame. **Both are unambiguous and so is CLAUDE.md**: an engine is "an interactive physics simulation with a focus on graphics ... temporally DETERMINISTIC".

## What was measured

`test/outshine/places` now prints the SHA-256 of each picture it writes (board:2041). Run the same
Central Park binary three times, no rebuild between them:

    e023c192
    82b5803f
    5f2b84a9

Three runs, three pictures. **And a second place does it too**: the Jura answered `2aa8f40f` and
then `d5261898` on consecutive runs of one binary. Shibuya, Heidelberg, Venice and the old town
repeat exactly, so it is not the instrument and not the device -- it is the two places, and both
are ones where the eye looks a long way over varied terrain.

**This was invisible until the digest existed**, which is the whole argument for it. The case is
GREEN in all three runs -- its oracle asks whether the frame holds the geometry that was built for
it, and it does, three different ways.

## WHERE IT COMES FROM, measured rather than guessed

**It is the ORDER and not the CONTENT.** Six runs of the Jura, one binary:

    d5261898  x4        2aa8f40f  x2        and nothing else -- a BINARY race
    every geometry count identical: 405 592 triangles, 128 mesh jobs, 456 outstanding, 456 held

The counts agree to the unit, so the same tiles arrive and the same buildings are meshed. What
differs is the sequence they are meshed IN, and two mechanisms turn that into a different picture:
a depth-test tie between coincident triangles goes to whichever was drawn first, and any reduction
over the set -- a bound, a centre, an anchor -- ends in different last bits.

**Drawing longer does not heal it.** Forty frames before the screenshot instead of two gives the
same two digests, so the divergence is settled BEFORE the first frame and is not a job landing
late.

**The mechanism is `OsmField`.** `Tiles_`, `Features_`, `Rings_` and `Points_` are all
`std::vector`s appended as tiles ARRIVE, and `BuildingField::Build` walks them in that order. Thread
scheduling therefore decides the vertex order of an entire city.

## The two shapes a fix can take, and the choice is a DESIGN one

**Ingest in KEY order rather than arrival order.** Deterministic, and it costs a reorder buffer: a
tile that arrives early waits for its lower-keyed neighbours. During a PRELOAD that is free, because
the client is waiting anyway and the owner has already said the initial stand must be complete from
far to near. While streaming WARM it is not free -- it would hold back geometry that could already
be drawn.

**So the likely answer is BOTH, split by phase**: key order while preloading, arrival order once
warm, and the digest is only claimed to repeat for a preloaded stand. That has to be decided
rather than assumed, which is why this item stops here instead of guessing.

## What will be true

- [ ] Central Park AND the Jura each answer ONE digest over ten consecutive runs
- [ ] `test/outshine/places/pictures.txt` carries every place's digest with no exception beside it
- [ ] whatever the cause is, it is named in the commit that fixes it, because a non-deterministic
      picture is a non-deterministic SIMULATION and the picture is only where it became visible
