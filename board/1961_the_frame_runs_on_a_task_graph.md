Type: feature
State: open
Parent: 1953
Area: host

# The frame runs on a task graph

**Both benchmarks agree and neither retrofitted it.** Unreal has `FTaskGraph` with a render thread
and an RHI thread beside the workers; RAGE has `sysTaskManager` and fibers. Dependencies are
declared, the graph schedules, and no stage waits on a thread that is doing something unrelated.

TARGET says nothing about threading at all, which means every line written until now assumed one
thread. The device is 2P+4E cores and the budget is 16.7 ms; one thread does not reach 720p60 with
a world beside the road, and threading added late is a rewrite of everything it touches -- which is
the reason this belongs in the refactor and not after it.

- [ ] frame work is declared as tasks with dependencies and a scheduler runs them
- [ ] the simulation step and the render encode overlap, proven by a case over the two timelines
- [ ] a task graph result is identical to the serial one, proven by a case running both
