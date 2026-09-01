Type: bug
State: open
Area: engine, world, render
Tags: measured, performance, owner

# Every place holds 16.7 ms at p99 and stands in under a second

**Benchmark** — Unreal: the frame budget is held by taking work OFF the frame path -- streaming runs
on `FIoDispatcher`, the landscape is cooked, and `stat unit` attributes a hitch to the system that
caused it. RAGE: streaming threads sit beside `sysTaskManager` and a replay plays a drive back frame
for frame, so a hitch is a defect rather than weather. **Both agree**: nothing that takes seconds may
run where a frame runs, and what a place needs is resident before the camera is asked for a picture.

## Measured 2026-09-01, `shots --all`, all nine places

| place | p50 ms | p99 ms | over 16.67 | waited |
|---|---|---|---|---|
| OldTown | 3.32 | 7 759 | 4/120 | 8.4 s |
| Heidelberg | 3.84 | 12 318 | 8/120 | 13.1 s |
| Shibuya | 4.18 | 11 294 | 8/120 | 23.4 s |
| CentralPark | 6.31 | **7.55** | **0/120** | 36.1 s |
| Venice | 3.07 | 5 486 | 4/120 | 7.0 s |
| Jura | 4.04 | 32 | 2/120 | 11.0 s |
| ZurichPlan | 10.14 | **14.08** | **0/120** | 22.2 s |
| Kaiserberg | 6.27 | 16 061 | 9/120 | 17.1 s |
| Koehlbrand | 4.25 | 13 134 | 6/120 | 14.3 s |

**p50 already holds everywhere.** The whole of the miss is p99: one frame in each place does seconds
of work. Two places hold the budget outright, so the target is not hypothetical.

**Where the seconds are, measured at Kaiserberg:** `rebuild: of that, the streets and the water`
reads 10 840 ms. It read 19 014 ms before roads stopped being swept as ribbons this round, so half
of it went with the ribbon and the other half is the road derivation itself -- fit, design, junction
levelling, corridor yield -- all of it inside the frame that happens to cross a tile boundary.

**Where the load is:** `waited` runs 7.0 s to 36.1 s, of which 1.0 s to 15.0 s is network streaming.
The rest is standing the world after the bytes arrive.

## How

- The road derivation is not frame work. It belongs where the TILE is cooked, once, with the result
  cached -- which is what both references do with their terrain and is what board 2091's three
  passes assume anyway.
- What still has to happen when the eye crosses a tile is a lookup, not a re-derivation.
- The load is a separate half: a second means the fetch is prefetched along the camera's path and
  the standing is not serialised behind it.

## What will be true

- [ ] Every one of the nine places reads `0 of 120 over 16.67` -- p99 under the budget, not p50.
- [ ] Every one stands in under 1.0 s of `waited`.
- [ ] `rebuild: of that, the streets and the water` is under 100 ms at Kaiserberg and Heidelberg,
      quoted from a run rather than estimated.
- [ ] Proving case: the nine places timed with the ceiling recorded, so it may only fall.
- [ ] Negative control: put the road derivation back on the frame path and require the ceiling to
      go RED.


## Measured 2026-09-01, and it is worse than a slow frame

`make shots PLACE=Venice`, four runs in a row:

| run | digest | p50 | p95 | p99 | worst frame |
|---|---|---|---|---|---|
| cold | `05b6863d` | 3.03 | 5.05 | **4239.79** | 55 |
| cold | `05b6863d` | 3.02 | 3.68 | **4360.00** | 55 |
| WARM | `9c9a4758` | 3.16 | 6.35 | **145.42** | 55 |
| cold | `05b6863d` | 2.90 | 3.74 | **4263.07** | 55 |

Two things are in that table and only one of them is the frame time.

**p99 is one frame, and it is always frame 55.** Not a distribution with a tail -- a single call that
blocks for four seconds while 119 other frames sit near 3 ms. `harness/claims/NoFramePathCallReachesABlock`
is RED (board:2094), and the two are almost certainly the same defect seen from two sides. A frame
that blocks is a frame waiting for IO on a compute path, which the fourth invariant forbids by name.

**The picture CHANGES when that block does not happen.** The warm run rendered `9c9a4758` and every
cold run renders `05b6863d`. So the shot is taken at a moment defined by the CLOCK rather than by
what is resident, and what has arrived by then depends on the network. That is a straight breach of
*"The same declaration renders the same bytes, twice, on this machine"*, and it means the digest --
the instrument this whole board leans on -- has a precondition nobody wrote down: it agrees only
while the streaming path takes about as long as it did last time.

Repairing the frame-55 block probably repairs both, because a shot that never waits cannot disagree
about what arrived during the wait. Until then, a digest comparison across a change is only evidence
when both runs streamed.
