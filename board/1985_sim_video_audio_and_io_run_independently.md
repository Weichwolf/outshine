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

- [ ] a compute worker never blocks on a socket or a disk -- it is handed the bytes when the
      IO threads have them, rather than asking for them in a loop
- [ ] `FetchBlockedMs` on a compute worker reads zero, which is the measurement that would show
      the separation is real rather than declared
