Type: feature
State: active
Parent: 1953
Area: host

# The frame runs on a task graph

**Benchmark** — Unreal: `FTaskGraph` with explicit dependencies, render and RHI threads. RAGE: `sysTaskManager` with fibers. **Both agree** — explicit dependencies, and 720p60 on four usable cores is unreachable from one thread.

**Both benchmarks agree and neither retrofitted it.** Unreal has `FTaskGraph` with a render thread
and an RHI thread beside the workers; RAGE has `sysTaskManager` and fibers. Dependencies are
declared, the graph schedules, and no stage waits on a thread that is doing something unrelated.

TARGET says nothing about threading at all, which means every line written until now assumed one
thread. The device is 2P+4E cores and the budget is 16.7 ms; one thread does not reach 720p60 with
a world beside the road, and threading added late is a rewrite of everything it touches -- which is
the reason this belongs in the refactor and not after it.

- [x] work is declared as steps with dependencies and a scheduler runs them: `src/base/Graph.{h,cpp}`
      -- fixed capacity, function pointers with a context, so declaring a frame allocates nothing.
      Every declared order holds on any number of hands, and the RESULT does not depend on how many
      there are.
      proof: outshine/physics/ScoreWhatATaskGraphOrders
- [ ] the FRAME runs on it. The graph stands and nothing has been put on it yet: the simulation
      step and the render encode still run in sequence on one thread. Splitting them is the next
      step and it needs the two to stop touching each other's state, which is board:1957's proxy
      question seen from the other side.
- [x] a task graph result is identical to the serial one, proven by a case running both --
      `ON ONE HAND 12 step(s) ran, summing 78` against `ON FOUR HANDS 12 step(s) ran, summing 78`,
      and the declared chain lands at 9 -> 10 -> 11 on four hands. A number that changed with the
      number of hands would mean the graph schedules rather than merely runs.
      proof: outshine/physics/ScoreWhatATaskGraphOrders
