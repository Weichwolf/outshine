Type: feature
State: active
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
- [ ] render runs on its own thread, one frame behind, and the simulation never waits for it
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

- [ ] a compute worker never blocks on a socket or a disk: `PoolTerrain::Take` requests through
      `TilePool::Bytes` and the fetch's completion posts the mesh job that was waiting on it
      -- built as a COMPLETION QUEUE after io_uring's shape, not as a chain of wake-ups.

      **THIRD ATTEMPT, ROLLED BACK, AND IT FAILED DIFFERENTLY** -- which is worth more than the
      two before it, because the failure is now specific. Measured: the blocking call is
      `TilePool::BytesBlocking`, reached from `PoolTerrain::Take` on a COMPUTE worker, and
      `outshine/geo/ScoreWhenAWaitForATileEnds` already reads `fetches that ran on a COMPUTE
      worker: 1 and 2` -- it prints the number and does not CHECK it, so the separation has been
      declared and never held.

      The attempt: `Take` asks through non-blocking `Bytes`, records the fetch key it awaits in a
      thread-local, and the worker PARKS the mesh job under that key -- under `QueueMutex_`, and
      only if the fetch is still in `Posted_`, so a fetch that completed first requeues instead of
      parking. The carrier's one completion site releases the parked jobs into `Queue_`.

      What happened: 818 asks in 5 seconds and `condition_variable wait failed: Invalid argument`.
      So the requeue arm SPINS -- a mesh job whose fetch is not posted goes straight back on the
      queue and is picked up again at once -- and something about the wake-up is wrong besides.
      The park half may well be right; the requeue half is a busy loop wearing a completion
      queue's clothes.

      **AND THE ROOT IS ONE LAYER LOWER THAN EVERY ATTEMPT HAS BEEN.** Chased after the rollback:
      `FetchInto` POLLS -- a bounded number of attempts with a sleep between them -- and returns
      `Pending` meaning *not ready, ask again*, whereupon `Posted_.erase` takes it out of flight.
      So a fetch in this pool has no completion to wait for, and `Data::Transport` is why: its
      verbs are `Begin` and `Collect`, submission and poll, with nothing for *wait until a ticket
      lands*. **A completion queue cannot be laid over a layer that cannot complete**, which is
      what all three attempts were doing.
      The completion EXISTS and is not handed up: `Host::Fetching` runs its own threads on
      blocking `curl_easy_perform`, so a ticket's answer is known the moment that call returns.
      So the order is: `Data::Transport` gains the missing verb -- one place to wait, io_uring's
      own shape -- THEN the pool stops polling, THEN a mesh job can park. Attempted in the other
      order three times.
      **The verb is in.** `Transport::Await(forMs)` waits until a ticket lands or the time is up
      and says which; its default sleeps and says no, so a transport that cannot complete behaves
      exactly as before. `Host::Fetching` implements it -- its worker threads already know the
      moment `curl_easy_perform` returns, and now they notify -- and `TilePool::FetchInto` waits
      on it where it used to `sleep_for(kPollMs)`.
      **What it does NOT yet prove, stated plainly**: no test in this tree shows a number moving.
      `ScoreWhenAWaitForATileEnds` reads 0.008 s and 88 asks before and after, because it measures
      the CALLER's poll loop and its stub transport answers at once; the driver's drive runs on a
      warm cache and fetches nothing. The gain needs a cold cache and a live network, which no
      case here has. What WOULD show the change is bad is the same case's wait-end time and the
      geo suite -- both unchanged, 12/12 -- so this is a shape held without a regression, and the
      number that pays for it arrives with the next step, not this one.
      Guarded meanwhile: `outshine/geo/ScoreWhenAWaitForATileEnds` now CHECKS the number it had
      only printed. It stands at 1 and 2 and may only fall.

      **FOURTH ATTEMPT: it reached 0 and 0, and hung the driver.** With `Await` in place the park
      finally worked in the wait case -- `fetches that ran on a COMPUTE worker: 0 and 0`, geo
      12/12, door 33/33 -- because the completion now CARRIES ITS OUTCOME: a job whose fetch
      completed goes back on the queue, and a job whose fetch GAVE UP has its own key erased so
      its caller sees Pending and asks again next round. That is what the third attempt got wrong
      and why it spun 818 times.
      One more condition was found and handled: with ZERO carriers nobody serves `Carrying_`, so
      a parked job waits on a completion that cannot come. `PoolTerrain::Take` blocks as before
      when the pool carries nothing, and the door suite went green on that.
      Then the gate hung: `./build/outshine-driver --headless --offline --frames 8` sat for
      seventeen minutes where it takes seconds. So there is a THIRD condition, in the offline
      path, that neither the wait case nor the door reaches -- and it is the one to find before a
      fifth attempt. Rolled back.
      The pattern across four attempts is worth naming: each one moved the failure somewhere new
      -- deadlock, deadlock, spin, and now a hang only the offline driver sees. The suites that
      go green are not the ones that catch this, and the gate's driver run is.
- [ ] `FetchBlockedMs` on a compute worker reads zero, which is the measurement that would show
      the separation is real rather than declared
