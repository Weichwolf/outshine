Type: feature
State: active
Parent: 1953
Area: engine
Progress: gpu-driven

# Update, render and audio run independently

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
- [ ] audio runs on its own thread against a deadline the frame rate does not set
- [ ] proof: a headless run of N steps takes measurably less wall time than the same N steps with
      a picture, and the trajectory is IDENTICAL -- same fixed step, same order, same numbers
