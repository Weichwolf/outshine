Type: feature
Area: render
Tags: perf, instrument

**A frame is submitted without waiting, and the queue states its bound**

A consumer builds frame N+1 while the device is still drawing N, and the device is never allowed to owe
more than a declared number of them. **The wall-clock a consumer measures around one frame is therefore
the PACE it can hold** -- neither a throughput, which hides a hitch, nor a sum of terms a shipping frame
runs at once.

## What it was

`RenderFrame` submitted and returned, and **nothing ever waited**. That is not pipelining, it is an
unbounded queue: a consumer faster than the device runs arbitrarily far ahead, every average still looks
well, and **latency -- the one number a player feels -- has no bound at all**. `CLAUDE.md` names both
halves: *two frames in flight is the normal case*, and *everything that grows states its bound*.

A consumer that wanted a frame time had one instrument, `WaitForGpu`, which waits for the device to go
IDLE -- so every measurement this repository could take of a frame was a serialised one, and a serialised
frame is a sum of two things that overlap.

## What it is

`kFramesInFlight` [SET] 2, and `RenderFrame` waits on the fence of the frame that many submissions back
before adding another. The bound is stated in one place, the wait is inside the library, and a consumer
measures a frame by timing its own call.

## What it measures, over a declared population

`ABeautifulGame`, 1280x720, 80 frames after a declared warmup, camera orbiting at 4 deg per frame.

| arrangement | p50 | p95 | p99 | max |
|---|---|---|---|---|
| **pipelined, 2 in flight** | **4.9599 ms** | 5.9075 | **6.7319** | 7.6574 |
| serialised | 6.1768 ms | 6.9412 | 7.2230 | 8.2700 |

**The declared run holds 720p60 at p99 with 59.6 % of the frame unspent**, and it holds it in BOTH
arrangements -- which is the stronger claim, because a cost on the engine's own side cannot hide behind
the device's when the two do not overlap. **What the overlap buys at p99 is 0.4911 ms**, and that number
is small for a good reason: `board:1460` had just removed the term it would otherwise have hidden.

**The two runs produce the same picture at the same frame, 0 of 102 480 samples differing** -- so the
picture is a function of the declaration and not of the pace. *That is a claim no previous instrument in
this tree could make, because no previous instrument ran the same declaration twice at two speeds.*

## Comments

**This is the fourth constraint answering with a distribution instead of being quoted.** `CLAUDE.md` has
carried *720p60 on this device* as the falsifiable target since the first page; until this item there was
no arrangement in which a run could fail it.
