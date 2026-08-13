Type: feature
Area: world
Tags: perf

**0.5 Exhaustion, and how it is said**

- [ ] Only a **declared** refusal is terminal: a 204 from the tile server, and nothing else. A timeout, a give-up, a transport failure, a 4xx that is not 404-by-contract, a decode failure and a failed allocation are all delays or defects, and none of them may retract a rung — the miss carries its reason to the caller instead of arriving as one absent-shaped answer (`world/TilePool.cpp` `RunMesh`, `Classify`, `Provider`). Ordered after the retraction above because it is what makes the retraction safe, and the reason a permanently coarse quadrant is the quietest failure this streamer can produce
- [ ] A refusal path: a piece of world that does not fit the budget is refused **by name**, counted, and the run continues — the reference logs the object and its size and skips it (`ObjManStreaming.cpp:752-759`)
- [ ] A failed allocation on an elastic path evicts, retries once, then refuses that piece of world
- [ ] A failed allocation anywhere else aborts loudly, naming the item and the bytes (`core/io/Heap.h` exists; `world/TilePool.cpp:359` is its only caller)
- [ ] Refusals and exhaustion published as columns, so they appear in the record and not only in a log line
- [ ] The toolchain's silent-null allocation behaviour turned off, so a null check is handling rather than dead code that looks like it
