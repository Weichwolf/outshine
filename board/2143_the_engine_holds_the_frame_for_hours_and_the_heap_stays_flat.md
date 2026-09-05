Type: proof
State: open
Area: engine, test
Tags: performance, owner, determinism
Depends: 2092, 2132

# The engine holds the frame for HOURS with a walking camera, and the heap stays flat

**Benchmark** -- Unreal: `Gauntlet` soak runs a level for hours and fails on hitches over a
bound and on memory growth (`MemReport` deltas); RAGE: the same as a nightly over a drive
around the whole map. **Both agree** that a frame budget proven over 120 frames is a
photograph and the proof is a FILM: the leak that costs a launch is one that grows a megabyte
a minute. **The measurement is mine**: this tree measures p50/p95/p99 over 120 frames
(`make shots`), which cannot see a slope.

## Where it stands, measured 2026-09-05

```
  make shots            120 frames a place, still camera, p50/p95/p99, peak heap once
  board:2092            the walk instrument: frames with the camera moving, not hours
  a soak                none; the longest run this tree makes is the fast gate's places suite
```

## The solution

One command (`make soak PLACE=<name> HOURS=<n>`) that walks the camera along board:2133's
network for the declared hours at the frame budget and prints a slope: heap bytes against
time as a least-squares line with its residual, frames over 16.67 ms per hour, and the
streaming pool's evictions per hour. The pass is a SLOPE UNDER A BOUND, derived from the
budget (Shibuya under 512 MB after an hour is the number 2104 declares), never a mean.

## What will be true

- [ ] `make soak` exists and its trailer names what it does not cover (a still camera, a place
      with no vectors)
- [ ] One hour at OldTown with the walk: heap slope under the derived bound, frames over
      budget per hour published, both in the item with their origin
- [ ] Negative control: a deliberate leak of one tile page per crossing makes the slope read
      the page's bytes per crossing, to a page
