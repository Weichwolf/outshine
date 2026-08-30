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

Three runs, three pictures. Every other place is stable across the same treatment -- Shibuya
answered `f9bfcc34` three times, Heidelberg `ece0fc0d`, Venice `50731a35`, Jura `55b8f95f`, the old
town `d22cda63`. So it is not the instrument and it is not the device: it is Central Park.

**This was invisible until the digest existed**, which is the whole argument for it. The case is
GREEN in all three runs -- its oracle asks whether the frame holds the geometry that was built for
it, and it does, three different ways.

## Where it most likely comes from

Not diagnosed, and the item says so rather than guessing in a way that gets quoted later. The
candidates, in the order they are worth testing:

- **The stream-in race.** A place stands when `settled()` says the view is loaded, and the ORDER in
  which tiles and vector features arrive is thread order. If a building is meshed from a tile that
  arrived second rather than first, its triangles land at a different index and the depth test
  resolves a coincident pair the other way. Central Park is the place with the most vector features
  per area of the six.
- **A generator reading a shared counter.** `RegionPool` and the draw generators run on the compute
  pool; a seed taken from anything but the FEATURE's own identity is a seed that depends on
  scheduling.
- **The frame the picture is taken on.** The picture loop draws what the picture needs; if a place
  needs one more frame on a slow run, it is a different frame.

## What will be true

- [ ] Central Park answers ONE digest over ten consecutive runs
- [ ] `test/outshine/places/pictures.txt` carries every place's digest with no exception beside it
- [ ] whatever the cause is, it is named in the commit that fixes it, because a non-deterministic
      picture is a non-deterministic SIMULATION and the picture is only where it became visible
