Type: task
State: open
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

## What will be true

- [ ] ONE ceiling is declared for the world's resident set, in bytes, and it is the number a reader
      finds when they ask what this engine costs
- [ ] `world-ground` and `tile-worker` stand under it, because they are 99 per cent of the bytes
- [ ] what exceeds it YIELDS, least-needed first, and the count of what yielded is published
- [ ] Shibuya holds under a stated figure rather than at whatever it reaches, and the figure is in
      the commit that sets it

## What this does NOT cover

Whether 3.9 GB is too much. The target holds 8 GB for everything including the operating system, so
it is; but this item is about the bound EXISTING, not about its value. A ceiling that is met by
being generous still turns a consequence into a decision, and the value can then fall on evidence.

The graphics side is separate and unmeasured here: `/usr/bin/time -l` reports the process, and what
the device holds is its own accounting.
