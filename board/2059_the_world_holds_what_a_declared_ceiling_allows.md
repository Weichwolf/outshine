Type: task
State: active
Area: world
Tags: memory, streaming, measured

# The world holds what a DECLARED CEILING allows, and yields the rest

**Benchmark** — Unreal: `FIoDispatcher` streams under a pool the project declares, and the renderer's
own resources sit under `r.Streaming.PoolSize`; when the pool is full the least recently needed
thing goes. RAGE: the streaming module holds a fixed arena and evicts against it, which is why a
RAGE title's footprint is a NUMBER rather than a consequence. **They agree**: a resident set is a
declared bound and eviction is what keeps it, so the matter is closed and this item is that this
tree's bound governs the wrong 64 MB.

## What was measured

Shibuya, one run, `/usr/bin/time -l` beside the tree's own heap tags:

    RSS                                    3.94 GB
    heap taken under world-ground          2.38 GB
    heap taken under tile-worker           1.54 GB
    heap taken under index-run           386.00 MB
    heap taken under tile-carrier         94.64 MB
    every subject tag together            under 30 KB

    tilepool byteBudgetMB                       64

**The one declared ceiling in the tree governs 64 MB of tile CACHE while 3.9 GB stands outside it.**
`world-ground` has no bound at all, and `tile-worker` -- the workers' own scratch -- is 24 times the
budget that is supposed to describe the streaming.

**AND THE SUBJECT IS NOT WHERE THE MEMORY IS.** The standing goal named subject geometry and
`grep Evict src/render/` finding nothing; both are true and neither matters at this scale. Every
render-side tag together is under 30 KB. Writing eviction for the subject would be writing it where
the bytes are not.

## THE FIRST THING A CEILING NEEDS IS A NUMBER, and this tree does not have one

`GroundStack::HeapBytes()` was added -- the five fields that know what they hold, summed -- and it
reads ZERO on Shibuya, two lines above `prints.Built()` returning 17 539 993 corners. So does
`BuildingField::HeapBytes()` on its own, and so does `buildings: footprints the field holds`, which
counted 10 459 earlier the same day.

The measure was REMOVED rather than left standing. A number that reads zero beside geometry that
plainly exists is the eighth measure this session that says something other than its name, and
shipping it would have been the same fault I spent the day naming in other people's code and my
own.

## THE CONTRADICTION IS RESOLVED, and it was the ledger rather than the field

`GroundStack::HeapBytes()` was never wrong. It was PUBLISHED behind `Grounds()`'s early return, so
the number a reader saw was the FIRST rebuild's -- taken when the fields were empty, before a
single footprint had landed. The same staleness cost board:2063 a night and made
`building triangles the world meshed` read 0 on a warm start; this is its third instance.

Published before the early return instead, it reads:

    Heidelberg   world: the bytes its fields hold      180 831 228      181 MB
    Shibuya      world: the bytes its fields hold    1 181 143 386     1.18 GB
    Shibuya      maximum resident set size                             2.85 GB
    Shibuya      heap taken under tile-worker                          1.50 GB
    Shibuya      heap taken under world-ground                         6.34 GB   churn, not residency

**So the world's resident set is 1.18 GB and there is now a number to bound.** The 6.34 GB under
`world-ground` remains allocation THROUGH a phase and cannot be a ceiling; the 1.18 GB is what the
fields actually hold and it is what a ceiling governs.

**That contradiction WAS this item's first step**, before any ceiling: a field reports nothing held
while its own geometry reports seventeen million corners, and until that is understood there is no
number to bound. The heap tags cannot stand in for it -- `world-ground` measures ALLOCATION through
a phase, so its 2.38 GB is churn rather than residency.

## WHERE THE BYTES ARE, and the ceiling that now stands over them

Shibuya's 1.18 GB, per field:

| field | bytes | share |
|---|---|---|
| **buildings** | 922 747 648 | **78.1 %** |
| OSM features | 189 559 526 | 16.0 % |
| land classes | 66 186 612 | 5.6 % |
| streets | 2 622 208 | 0.2 % |
| water | 27 392 | 0.002 % |

`GroundStack::kHoldsBytes` is **1.5 GB**, derived rather than chosen: 1.18 GB of fields stands at
2.85 GB resident, and the target carries 8 GB for everything including the operating system and
the driver. 1.5 GB of fields leaves the resident set near 3.2 GB.

**The control fired and it corrected the code first.** At a ceiling of 512 MB:

| where the test sits | held | overshoot |
|---|---|---|
| AFTER the ingest | 1.18 GB | **220 %** -- a brake, not a bound: it stops once already over and creeps a tile per round |
| BEFORE the ingest | **626 MB** | 17 %, which is one round's worth of tiles |

Buildings fall from 923 MB to 365 MB and 370 rounds stop at the ceiling. The bound holds.

## NEAREST FIRST, and the ceiling becomes a bound worth having

`TileWatermark::Ask` sorted its candidates by the z/x/y tile word -- an order with nothing to do
with where the camera stands. Now by squared distance from the field's centre tile, with the same
z/x/y word breaking ties so the order stays DECLARED rather than incidental; determinism does not
get an exception here.

`OsmField` remembers the tile its last `Build` centred on, which is the one number the watermark
was missing.

| Shibuya under a 512 MB ceiling | triangles | varies by |
|---|---|---|
| by tile key | -- | **0.096** -- a picture of almost nothing |
| **by distance** | 2 646 181 | **1.428** |
| no ceiling at all (1.5 GB) | 8 831 673 | 1.436 |

**A third of the geometry and a picture the case's own measure cannot tell apart.** Looked at: a
full dense city to the horizon, towers, streets, roofs.

## SO THE CEILING MOVES TO WHERE IT BINDS

At 1.5 GB Shibuya reaches 1.18 GB and never touches it. A bound that does not bind at the densest
place this tree stands is a number with no effect -- the same shape as the seven blind checks this
session found elsewhere. Measured either way:

| Shibuya | 1.5 GB | **512 MB** |
|---|---|---|
| fields held | 1.18 GB | 625 MB |
| **maximum resident set** | 2.85 GB | **1.97 GB** |
| triangles | 8 831 673 | 2 646 181 |
| varies by | 1.436 | 1.428 |
| rounds stopped | 0 | 374 |

`kHoldsBytes` is **512 MB**, derived from the measurement rather than from the device's 8 GB:
it is the smallest bound at which the densest place still draws a picture indistinguishable from
an unbounded one. The 22 per cent overshoot is one round's ingest -- the test sits BEFORE a tile
is taken, so the last one taken may cross. `outshine/places` holds 8 PASS either way and
CentralPark's variation is 0.7992 against 0.7991, its own refusal unmoved.

## THE CONTROL FOUND WHAT THE CEILING LACKED

At 512 MB, Shibuya renders `varies by 0.096 of 255` -- a picture of almost nothing. Ingest walks
tiles in z/x/y KEY order, not by distance, so a ceiling fills with whatever sorts first and
**starves the near world**. A ceiling without an ordering throws away the wrong thing.

That is this item's third box in its own words -- *what exceeds it YIELDS, least-needed first* --
and the control has now measured why it is not optional.

## What will be true

- [x] ONE ceiling is declared for the world's resident set, in bytes, and it is the number a reader
      finds when they ask what this engine costs -- `GroundStack::kHoldsBytes`, 1.5 GB, published
      beside what the fields actually hold
- [ ] `world-ground` and `tile-worker` stand under it, because they are 99 per cent of the bytes
- [x] what exceeds it YIELDS, least-needed first, and the count of what yielded is published --
      the ingest order is nearest-first and `world: times a round stopped at that ceiling` reads
      374 on Shibuya. What is REFUSED is the farthest; nothing already held is dropped yet, which
      is the difference between a bound and an eviction and is the box below.
- [ ] A tile already held is DROPPED when the camera walks away from it. Today the bound refuses
      what has not arrived; it cannot shrink a set that is already too large, so a long drive
      still ends at the ceiling with the wrong tiles in it.
- [x] Shibuya holds under a stated figure rather than at whatever it reaches -- 625 MB against a
      stated 512 MB, at 1.97 GB resident where it was 2.85 GB
- [ ] Shibuya holds under a stated figure rather than at whatever it reaches, and the figure is in
      the commit that sets it

## What this does NOT cover

Whether 3.9 GB is too much. The target holds 8 GB for everything including the operating system, so
it is; but this item is about the bound EXISTING, not about its value. A ceiling that is met by
being generous still turns a consequence into a decision, and the value can then fall on evidence.

The graphics side is separate and unmeasured here: `/usr/bin/time -l` reports the process, and what
the device holds is its own accounting.
