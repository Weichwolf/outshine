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

### Two suspects, neither measured

`harness/claims/NoFramePathCallReachesABlock` is red and names `Render::Readback::FromBuffer`,
`::Land` and `::Release` as REACHABLE from `Engine::render`. Those are the `Read*` instruments --
ReadDepth, ReadSceneLinear, ReadPyramid and the rest -- so it is reachability in the link graph and
not proof of a per-frame call; a readback every frame would show in p50, and p50 is 3 ms.

And the stall sits at frame 55 of 120, which is NOT the capture at the end. Something happens once,
mid-run. `meshMs=545` appears in the log around there, which is half a second and not four.

So: a named suspect and a plausible mechanism, and no measurement joining them. Whoever takes this
item states what they will measure BEFORE changing anything -- per-frame timing with the phase that
owns each millisecond, so the four seconds have a name rather than a theory.

## The nine places, CONFIRMED BY THE OWNER 2026-09-02

Rendered together and looked at. These digests are one STREAMING STATE and not a reference --
the control below shows one unchanged binary rendering two of them differently.

| place | digest | triangles | p50 | p95 | p99 | over 16.67 |
|---|---|---|---|---|---|---|
| OldTown | `a23e97f2` | 349 766 | 3.29 | 6.46 | 5951.67 | 3 |
| Heidelberg | `8892d581` | 1 040 292 | 4.01 | 16.73 | 9014.88 | 7 |
| Shibuya | `70e2c675` | 2 732 059 | 4.20 | 24.97 | 7667.48 | 7 |
| CentralPark | `8f5e8e3f` | 3 673 071 | 6.37 | 8.52 | 9.39 | 0 |
| Venice | `05b6863d` | 1 379 233 | 2.86 | 4.07 | 4196.72 | 4 |
| Jura | `6fca5698` | 405 592 | 4.02 | 5.37 | 16.02 | 1 |
| ZurichPlan | `02336e15` | 1 637 850 | 7.92 | 8.43 | 8.66 | 0 |
| Kaiserberg | `97c911b5` | 1 529 841 | 5.10 | 19.68 | 11243.28 | 8 |
| Koehlbrand | `2f6acab3` | 722 223 | 4.18 | 9.80 | 9429.33 | 5 |

**Two of the nine already hold the frame** -- CentralPark and ZurichPlan are 0 of 120 over budget
and their p99 is 9.39 and 8.66 ms. Both are the places that finished streaming before the camera
moved. That is the shape of this item's answer: the seven that do not hold it have a p99 of four to
eleven SECONDS, which is not a frame that is slightly late, it is a frame waiting for IO.

Venice's digest is the one that wanders between 05b6863d and 9c9a4758; the others were seen once
each here and their stability is not yet measured.

## The control, 2026-09-02: the table above is a SNAPSHOT and not a reference

The row of digests was about to be used as an oracle for a refactor. It cannot be, and the control
that says so took four runs:

| run | binary | place | digest |
|---|---|---|---|
| 1 | HEAD | Kaiserberg | `97c911b5` |
| 2 | HEAD, THE SAME BUILD | Kaiserberg | `2c14838f` |
| 3 | HEAD | Shibuya | `f7b85587` |
| 4 | HEAD + a medium refactor | Shibuya | `f7b85587` |

**Two consecutive runs of one unchanged binary render two different Kaiserberg pictures.** They
differ in 400 of 921 600 pixels by at most 4 of 255, clustered -- a distant thing that arrived in
one run and not the other, never a shifted sky, which would move every sky pixel by at least one.
Shibuya's `70e2c675` and `f7b85587` differ in TWO pixels by 3.

So the wandering is not Venice's alone: it is every place whose p99 is a multi-second stall, which
is seven of the nine. The two that hold the frame -- CentralPark and ZurichPlan -- are the two that
were stable across every run here, and that is the same fact from the other side.

**What this costs.** `make shots` writes each picture under its digest so a frame that moved says
so; while seven of nine wander, that instrument reports a change the tree did not make and stays
silent about none it did. Until the stall is repaired the honest oracle is the PIXEL DELTA against
a kept picture -- zero, or a handful of pixels at a handful of levels, or a shifted field -- and
only the third is a change to the drawing. That is the reading this item has to make unnecessary.

## The wander has a MECHANISM, and the picture is not taken where this item assumed

Two measurements, 2026-09-02, and the first one moves this item's ground.

**THE SCREENSHOT IS TAKEN BEFORE THE 120 TIMED FRAMES.** `Shots::Draw` preloads, renders
`settleFrames()` frames, writes the PNG, and only THEN walks the camera for the timings. So the
four-second stall at frame 55 cannot be what moves the digest -- it happens after the shutter. What
the picture depends on is `preload` and the settle, and nothing else.

**WHERE THE TWO PICTURES DIFFER SAYS WHAT MOVED.** Comparing the kept pairs pixel by pixel:

| place | differing | worst | where |
|---|---|---|---|
| Venice `05b6863d` vs `9c9a4758` | 18 of 921 600 | 3 of 255 | y 293..329, x 89..1229 |
| Shibuya `70e2c675` vs `f7b85587` | 2 | 3 | y 295 and 483 |
| CentralPark `8f5e8e3f` vs `fd049bf7` | 138 | 1 | y 299..639, full width |

All three start at **y ≈ 295** and spread across the whole width: the horizon and the ground below
it, a handful of levels. Not a shifted sky, not a moved building -- the ground's LIGHT.

**AND A MECHANISM THAT FITS.** `IrradianceStage::Declare` decided whether to recompute the sky's
irradiance with

    Standing wanted;                                   // default-initialised
    if (Settled_ && std::memcmp(&Standing_, &wanted, sizeof wanted) == 0) { return; }

`Standing` holds an `alignas(16)` 80-byte `Medium` and one `float`: 84 bytes rounded to 96, so
**twelve bytes of padding nobody writes** sit inside the comparison. Reading them is undefined and
their value is whatever the stack held. The stage next door, `MediumRadianceStage`, had already
patched exactly this by hand -- an explicit `Vec2f Pad` and a static_assert saying "every byte of
the settled comparison is a member, none is padding" -- which is evidence that somebody hit it
before and fixed the symptom in one place.

All four medium stages compare by value now (`operator== = default`), the hand-rolled `Pad` and its
assert are gone, and CLAUDE.md carries the rule: `alignas` belongs at the device boundary, equality
is `operator==`, never `memcmp`.

**AND THE PADDING WAS NOT IT.** Before the repair: 24 runs of Venice, 23 `05b6863d` and one
`9c9a4758` -- about one in twenty-four. After: 84 runs, all `05b6863d`, which at the old rate had a
2.8 % chance of happening by luck. That looked like a repair. It was not: the very next run after
the next build rendered `9c9a4758` again, byte for byte the same second picture. Eighty-four clean
runs bought a 2.8 % coincidence, and the coincidence is what happened.

**THE LESSON IS ABOUT THE MEASUREMENT AND IT IS THE EXPENSIVE ONE.** A rate of one in
twenty-four needs on the order of seventy runs to see a single event, so a clean batch of eighty is
barely one expected occurrence -- it can never distinguish "repaired" from "unlucky". Chasing a rare
binary with a pass/fail count is the wrong instrument. What is needed is the STATE that differs:
the shot must publish what it stood on -- how many tiles were coarse, which frame the settle ended
on, what the irradiance buffer held -- so one run of each picture says what is different, instead of
a hundred runs saying how often.

What the round did settle: reading indeterminate padding to decide whether to recompute a lighting
table is a defect whatever it does to a digest, all four medium stages compare by value now, and
CLAUDE.md carries the rule. The wander itself is still open and this item still owns it.

## The state IS published, and it says the content is identical

The step this item asked for needed no code: `Shots::Draw` already writes `Triangles`, `BareTiles`,
`VariationAlongRows`, `Preloaded`, `SettledOver` and `PosedAtS`, and `shots --rows` prints them. The
earlier runs were measured with the human-readable line, which prints none of them.

One `9c9a4758` beside five `05b6863d`, same binary, 2026-09-02:

| field | `9c9a4758` | `05b6863d` (×5) |
|---|---|---|
| triangles | 1 379 233 | 1 379 233 |
| bare tiles | 0 | 0 |
| variation along rows | 1.4150 | 1.4150 |
| preloaded | 1 | 1 |
| settled over | 2 | 2 |
| posed at | 0.0000 | 0.0000 |

**Every measure the shot publishes agrees.** The world that was drawn is the same world by every
count the instrument can take, and the picture still differs in 18 pixels by one level. So the
difference is not "a tile arrived in one run and not the other" -- that hypothesis is now measured
and dead. It is in the DRAWING of an identical scene, or in a state the shot does not yet publish.

**And the settle is TWO frames.** `Plan_->SettleFrames()` reads 2, so the picture is a two-frame
temporal accumulation: frame 1 has no history, frame 2 blends against it with the Halton jitter at
index 2. Anything that is one frame late is half of what the shutter sees.

## THE ENGINE CUTS ITS WORLD AT A WALL CLOCK, in three places

`grep steady_clock` over the ground path, and the fourth invariant is breached by construction:

| where | what the clock decides |
|---|---|
| `GroundStack::Restand`, the ingest loop | how many tiles of ways, water and footprints enter this frame |
| `OsmField::Build` | whether the next vector tile is DECODED this frame (`mayDecode`) |
| `ClassField::Update` | passes the same budget down to both its fields |

All three take `kStreamBudgetMs = 2.0` and stop when `steady_clock::now()` has passed it. A machine
that is 2 % busier ingests one tile fewer, and the frame that renders next stands on a different
world. **"The same declaration renders the same bytes, twice, on this machine" cannot hold while a
wall clock decides what is in the world.** Whether the remaining work always lands before the
shutter is exactly the question the 18 pixels are asking.

Both references budget streaming by WORK rather than by time for this reason -- RAGE's replay plays
a drive back frame for frame, and Unreal's automation compares screenshots bit for bit. A time
budget is what you use when a dropped frame is worse than a wrong one; here the invariant says the
opposite.

**The repair is a DECLARED budget**: a count of tiles per frame, taken from the same declaration
every run, with the millisecond figure kept only as an instrument that reports what the count cost.
That changes streaming behaviour and possibly every picture, so it is its own round with its own
before-and-after, and it belongs beside board 2091's passes rather than inside a lint commit.

**One correlation, three for three, unexplained.** The flip has now appeared on the first run after
a build three times running, and on no other run: 84 clean, build, flip; 30 clean, build, flip. A
build does not touch the tile cache -- it lives in the system temp directory -- so the mechanism is
not "cold cache" and is not yet named. Recorded because it is a cheap trigger for whoever measures
this next: rebuild, then run once.

## THE WANDER HAS ITS MECHANISM, MEASURED: a WALL CLOCK decided what was in the world

`grep steady_clock` over the ground path found three cuts, all taking `kStreamBudgetMs = 2.0`:
`GroundStack::Restand`'s ingest loop, `OsmField::Build`'s `mayDecode` gate, and `ClassField::Update`
passing the same budget to both its tiers. A machine 2 % busier ingests one tile fewer and the next
frame stands on a different world.

**The trigger was found first and it is cheap: the FIRST run after a build.** Never after a plain
idle, never back to back. A control settles that it is not simply CPU state -- ten processes spun
for 55 s, then one run, and it came out clean.

| the frame path's budget | runs, each the first after a build | flipped |
|---|---|---|
| `2.0 ms` | 4 | **4** |
| no clock at all (control) | 4 | 0 |
| the ring as the bound (the repair) | 4 | 0 |

Fisher exact on 4/4 against 0/8: p = 0.002. **The clock is the mechanism.**

**THE REPAIR IS THE RING, and it invents no number.** `OsmField::Build` already walks a ring once
per call and skips what is settled, so the ring IS a bound -- the clock only made that bound smaller
and non-reproducible. It is gone; the ingest loop is bounded by `kVectorTiles`, the tile count of
the vector ring, which is what a stand can newly need. `Restand`, `Build` and `Update` now take a
`LongitudeLatitude` instead of a lat/lon pair, so five swappable-parameter findings went with it.

All nine places, after: eight render the digest recorded above, byte for byte, and Kaiserberg
renders `2c14838f`, the second of the two values the control table already recorded for it.

## AND THE INSTRUMENT IS MIXING TWO MEASUREMENTS -- the owner's specification, 2026-09-02

> "erst wird initial geladen. wenn ALLES fertig ist wird der erste frame gerendert und ab da wird
> fps gemessen. alles vorher ist preload."

Against that, `Shots::Draw` is wrong in two nameable ways, and the second one invalidates most of
the p99 column above.

- **`preload` GIVES UP.** It takes `kPatienceS` and returns without the world standing; the row then
  reports fps anyway with `Preloaded = 0`. Shibuya and CentralPark came out that way in the run
  above. Under the specification there is no fps to report from a place that did not finish
  loading -- the row REFUSES instead.
- **THE TIMED FRAMES DO STREAMING.** `advance()` calls `Restand` every frame, and the 120 timed
  frames walk the camera through the `walk` views while the preload stood only on the FIRST one. So
  every frame that crosses into an unvisited tile pays for that tile, and the number is called p99.
  That is CLAUDE.md's own trap -- a subject's rate quoted about a world -- inside the instrument the
  whole board scores itself with.

**What this costs the table above.** CentralPark reads p99 10.26 ms before the repair and 536 ms
after, and NEITHER is a frame time: the first is the 2 ms clock REFUSING the work, which is also
why that place's picture wandered between `8f5e8e3f` and `fd049bf7`; the second is the same work
done honestly, in the frames that are being timed. The repair did not make the frame slower, it
made the number stop hiding. Comparing the two as frame times is the mistake.

**What is to be true.** `preload` stands the world for EVERY view the run will walk through, and
returns only when it stands or refuses outright. The first frame comes after that. Then p99 is a
frame time, the frame path has no streaming left to do, and a per-frame budget can be as small as
the frame needs without touching a picture. That last clause is why the clock had to go first: with
a wall clock, even a fully preloaded world is not reproducible.

### CORRECTION, same day: the clock was A mechanism and not THE mechanism

After the repair above, and after the still-camera gate below it, Venice rendered `9c9a4758` again
on a first run after a build. The 4/4 against 0/8 measurement stands as evidence that the clock
HAS an effect; it does not support the sentence "the clock is the mechanism", and that sentence is
withdrawn. What is measured is that removing it made the flip rarer, not that it made it impossible.

**AND THERE IS A SECOND ONE, in the same sentence of CLAUDE.md.** `OsmField::Build` walks its ring
and skips a tile whose bytes have not landed; a later call picks that tile up. So `Tiles_` --
and with it `Points_`, `Rings_` and `Features_` -- is filled in ARRIVAL order and not in ring
order: everything ready at the first call in ring order, then everything ready at the second, and
so on. Which tile is in which group is decided by the network.

The three sinks (`StreetField::Ingest`, `WaterField::Ingest`, `Footprints::Build`) then consume
that array in the order it happens to hold, so what they assemble is combined in COMPLETION order.
That is the exact wording the fourth invariant forbids, and it is a far better candidate for a
one-level-per-pixel difference at the horizon than a missing tile ever was: the same features in a
different order, summed in floating point, differ in the last bits.

**What is to be true:** a field's tiles stand in a DECLARED order -- the ring's -- whatever order
their bytes arrived in, and a sink consumes them in that order. Then two runs that fetched the same
bytes assemble the same world whatever the network did.
