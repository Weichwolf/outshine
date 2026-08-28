Type: feature
State: open
Parent: 1953
Area: engine
Progress: gpu-driven

# Sim, video, audio and IO run independently

**Benchmark** — Unreal names four: **game**, **render** (one frame BEHIND), **RHI** and **audio**,
over an `FTaskGraph` worker pool. RAGE names three: **update**, **render**, **audio**, over
`sysTaskManager` fibers. **Both agree** on the three that matter and on the one-frame lag, and the
lag is affordable only because the renderer holds its OWN copy fed by explicit deltas -- which is
the scene row of the settled table, already TARGET here and already built (board:1957).

**Audio makes this a precondition rather than an improvement.** A mixer must hand the device a
buffer every few milliseconds or it glitches, and that deadline has nothing to do with the frame
rate. There is no version of the audio programme (board:1982) that works on one thread.

And the same split is what makes HEADLESS the fast path: with no frame to pace against, update
runs as fast as it can, which is what a dedicated server and every offline run are. Today
`Engine::Advance` refuses outright when no picture stands, so a pure physics run is impossible.

- [x] `Advance` steps the world with no picture standing, and the fall it integrates is the
      semi-implicit Euler closed form to a micrometre.
      proof: outshine/door
- [x] the tick is named phases with a stated handoff -- `Updates()` owns the world and
      `Draws()` hands the delta on; the mixer reads a snapshot of where sources stood.
      proof: outshine/door
- [ ] the third phase is the mixer's own, not the frame loop's
- [ ] render runs on its own thread, one frame behind, and the simulation never waits for it.
      **BLOCKED ON board:1957, and the block is measured rather than assumed.** The sim reaches
      the renderer through TWENTY-FIVE distinct verbs called directly -- `Eye`, `Carry`,
      `Scrolled`, `Redeclare`, `Restand` are writes and `SubjectBatchCount`, `Spent`, `Aimed` are
      READS BACK. Unreal forbids exactly this: nothing crosses as a call, `ENQUEUE_RENDER_COMMAND`
      puts a lambda on a queue and the game thread keeps its own mirror rather than reading the
      render thread's. RAGE double-buffers the draw list. Both agree the boundary is
      ONE-DIRECTIONAL and BUFFERED, and a read-back is the thing that cannot exist.
      So the thread is not the work. The BOUNDARY is, and board:1957 owns it.
- [x] the mixer reads a SNAPSHOT and never the live world, so it is callable from the device's
      own thread -- which is where the deadline lives, because the CLIENT owns the process and
      therefore the device. `Updates()` publishes into one of two buffers and releases an index;
      `Mixes` acquires it. No lock, and the simulation never waits on a mix.
      proof: outshine/audio, outshine/door
- [ ] proof: a headless run of N steps takes measurably less wall time than the same N steps with
      a picture, and the trajectory is IDENTICAL -- same fixed step, same order, same numbers

## IO is the fourth, and the tree measures its own cost already

**Benchmark** — Unreal: `FIoDispatcher` and the async loading thread sit BESIDE the task graph,
never on it. RAGE: streaming threads beside `sysTaskManager`. **Both agree** — a fetch blocks, and
a blocking task on a compute worker is a worker doing nothing while holding a slot.

`TilePool`'s workers do three jobs from one queue (`TilePool.cpp:426-435`): `Rank::Fetch` polls
the network, `Rank::Mesh` stitches and decodes, `Rank::Dag` builds the cluster DAG. One is IO and
two are compute, and the first can hold a thread for the whole of its poll bound -- 30000
attempts by default, which board:1915 measured at roughly a minute.

**The number that says so is already published.** `TilePool::Ledger` carries `FetchBlockedMs`
beside `MeshCpuMs`, and the pool subtracts one from the other to get CPU time
(`spanMs - (tFetchBlockedMs - blockedBefore)`) -- so the tree knows exactly how long a worker
spends waiting rather than working, and spends it anyway.

**IO ALREADY HAS EIGHT THREADS, and that changes what is wrong here.** `Fetching`'s default is
`kDefaultThreads = 8` (`src/host/Fetching.cpp:11`), so the transport is asynchronous and off the
frame path already. The defect is narrower and worse than "IO needs a thread":

    the 8 IO threads do the waiting
    AND a compute worker sits beside them asking `Sources_.Collect` every few ms

Both wait for the same bytes. One of them is a core.

**Which gives the rule its second half: IO threads scale with CONCURRENT REQUESTS, compute
threads scale with CORES.** Eight is a reasonable number of tiles to have in flight; it has
nothing to do with how many cores the machine has, and a machine with two cores still wants
eight requests outstanding because seven of them are asleep. That is why the two pools are sized
by different quantities and cannot be merged into one.

So the work is not "give IO a thread". It is: **a compute worker is NOTIFIED, never polls.**

**WHY FOUR IS THE RIGHT NUMBER, AND IT IS NOT BECAUSE THERE ARE FOUR CORES.** A core count is a
correspondence, not an argument, and this device has 2P+4E rather than four of anything. The
argument is that IO is a DIFFERENT KIND OF WORK:

| | bound by | deadline |
|---|---|---|
| sim · video · audio | CPU | yes — 16.7 ms, or a mix buffer |
| IO | latency | none: it takes what it takes |

A thread with a deadline and a thread without one cannot share a queue, because the one without
will eventually be holding the slot the one with needs. And the cost is asymmetric in our favour:
**an IO thread that is blocked consumes no core** — it is asleep, holding a context and nothing
else. An IO task on a compute worker consumes a core, because the slot is held whether or not
work is happening. That asymmetry is the whole of it, and it holds on two cores as firmly as on
sixteen.

**HALF BUILT, AND THE MEASUREMENT NAMES THE OTHER HALF.** `TilePool` now has a second pool:
`Carriers_` serve a `Carrying_` queue that holds only `Rank::Fetch`, so fetch JOBS no longer take
a compute slot. `Config::Carriers` sizes it (default 2) and `Ledger::FetchOnCompute` counts what
still slips through.

    fetches that ran on a COMPUTE worker: 1 and 2

Those are not jobs. They are the SYNCHRONOUS reads a mesh job makes while stitching:
`PoolTerrain::Take` calls `BytesBlocking`, which checks the disk cache and then the network on
whatever thread asked. **So it is not only network** -- the cache lookup is a disk read on the
same thread, and the question "where is it read from" is the wrong one. What matters is that the
caller WAITS.

**And the non-blocking half already exists, unreachable.** `TilePool::Bytes` posts a fetch job
and returns Pending -- request now, be told later, which is the pattern. Switching `Take` to it
is ONE LINE and it breaks the wait: the mesh job gives up, nothing re-posts it when the bytes
land, and `MeshAwaited` returns Pending immediately (measured: *entered the wait: NO*).

**THE MISSING PIECE IS A COMPLETION QUEUE, AND LINUX HAS THE SHAPE.** I said this was
`Work::Graph`'s question and that was wrong on inspection: `Graph::Runs()` is a BATCH -- it runs
every declared step and returns -- so it schedules a frame, not a long-lived stream of jobs.

The right model is **io_uring**: a submission ring and a COMPLETION ring. The requester puts work
in one; whoever finishes it puts the result in the other; and the consumer blocks on the
completion ring for *any* completion rather than on its own request. `epoll` is the same idea one
level up -- one place to wait, told which of many is ready.

**Two attempts at the chain model both deadlocked, and that is the evidence.** Attempt one:
`notify_one` across two queues woke an arbitrary waiter, so a compute worker found its own queue
empty and slept while the carrier never woke -- the driver hung for 600 s. Attempt two: a mesh job
was parked in a `Waiting_` map keyed by the fetch it needed, and the drop path did not release it,
so `MeshAwaited` slept out its 5 s bound. Both are the same mistake in different clothes: a
wake-up CHAIN has a link for every pair, and every link is a chance to lose a notification.

A completion queue has one link. That is why it is the shape to build, and why this predicate is
not being forced through with a third variation of the same error.

- [x] **a compute worker never blocks on a socket or a disk.** `PoolTerrain::Take` asks through
      the non-blocking `Bytes`, records the fetch key it awaits, and the worker PARKS the mesh job
      under that key; the carrier's single completion site releases it. Five attempts, and each
      failure named the condition the next one needed:

          1, 2  deadlock  -- built over a layer with no completions at all
          3     spun 818 times in 5 s -- the completion did not carry its OUTCOME
          4     hung the offline driver 17 minutes -- a REFUSAL was not remembered
          5     holds all three, plus the repair the fourth missed: `SourceSet` refuses in TWO
                places and only one carried the retry deadline. A source that answers but whose
                bytes cannot be taken is refused just as definitely as one that says no, and it
                handed back a deadline of zero -- a refusal remembered for no time at all.

      And one condition that is not a bug but a bound: with ZERO carriers nobody serves
      `Carrying_`, so `Take` blocks as before. The separation exists where IO threads do.
      proof: outshine/geo/ScoreWhenAWaitForATileEnds reads `fetches that ran on a COMPUTE worker:
      0 and 0`; the offline driver finishes 8 frames in seconds where it sat for seventeen
      minutes; outshine/door 33/33, gate GREEN.
      negative control: the count is CHECKED at zero now -- it read 1 and 2 through four attempts,
      printed and unproven.
