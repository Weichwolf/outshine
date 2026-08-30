Type: defect
State: active
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

## THREE SOURCES, TWO CLOSED, AND THE THIRD IS THIS TREE'S OWN CULL

**One: tiles were ingested in ARRIVAL order.** `OsmField` appends as tiles land and the consumers
walked that order, so thread scheduling decided the vertex order of a city. `TileWatermark::Ask`
now picks the consumable tile with the smallest KEY instead of the first to arrive. Closed.

**Two: a building's base was sampled from whatever ground was resident.** `GroundStream::Resident`
answers from the fine tile when it is there and otherwise from a COARSER ancestor, marking the
sample `Coarser(n)` -- and the guard only refused a `Pending` one. So a building meshed while the
coarse tile stood got a coarse base, and whether that happened was a race. It refuses a coarse
sample now, which costs nothing: the preload already waits for every tile. Closed, and it is the
owner's older report -- *ready must be ready at the correct LOD* -- found by measurement.

**Together they made the WORLD deterministic**, and a new measure proves it rather than asserting
it: `restand: the geometry handed over, digested` is an FNV-1a over the packed index run and every
channel of every part. Six consecutive runs of Central Park answer one digest.

**Three: THE CULL'S COMPACTION IS ORDERED BY THE GPU.** With the world identical the picture still
moved, and the control is decisive -- disabling the indirect draw gave SIX identical pictures.
`subjectCull.msl` hands each surviving cluster its slot with `atomic_fetch_add`, so the compacted
index run is in the order the atomics fired. Where two surfaces coincide the depth test resolves
the tie by arrival, and Manhattan has many.

## The fix, designed

A PREFIX SUM, which is what makes a survivor's destination a function of its INDEX rather than of
when its atomic fired. Three dispatches:

    1  cull      one thread per cluster: test, write `Kept[job] = kept ? count : 0`
    2  scan      one thread per BATCH, serial exclusive prefix over that batch's jobs -> `At[job]`,
                 and the batch's total into `num_indices`
    3  compact   one threadgroup per job, copy to `base + At[job]`

The scan is serial per batch and that is affordable: about 74 000 dependent adds is roughly 74 us
on one lane, and the batches run in parallel. A multi-level scan buys nothing at this size and
costs a kernel.

**Nanite does this and so does every GPU compaction that has to repeat.** An atomic gives the right
COUNT and never the right ORDER.

## What will be true

- [x] the Jura answers one digest over ten runs; the geometry digest repeats everywhere
- [ ] Central Park answers ONE picture over ten consecutive runs -- the scan above is what is left
- [ ] `test/outshine/places/pictures.txt` carries every place's digest with no exception beside it
- [ ] whatever the cause is, it is named in the commit that fixes it, because a non-deterministic
      picture is a non-deterministic SIMULATION and the picture is only where it became visible
