Type: bug
Area: core
Tags: perf, instrument

**The memory ledger**

- **The layout guard on `mallinfo` is an algebraic tautology and cannot fail on the case it was written
  for.** `core/io/HeapProbe.cpp:34-36` accepts the struct when `uordblks + fordblks == arena + hblkhd`,
  and its comment claims "a field order that had slipped could not satisfy it". In the dlmalloc this
  sysroot links (`~/Git/emsdk/upstream/emscripten/system/lib/dlmalloc.c:3593-3599`) `arena = sum`,
  `hblkhd = footprint − sum`, `uordblks = footprint − mfree`, `fordblks = mfree`, so **both sides equal
  `footprint` in every heap state** — the identity says nothing about the heap and only tests four
  offsets. Measured under emscripten 6.0.3, `-pthread`, at three heap states (pristine, +14 MB, after
  free): the identity held every time with `hblkhd = 0`, and **940 of the 10 000 assignments of the four
  roles to the ten offsets satisfy it**. Four fields are permanently 0 (`smblks`, `hblks`, `hblkhd`,
  `fsmblks`), so **256 of those 940 are all-zero quadruples**: they pass the guard and make `LiveBytes()`
  return a constant 0 — the exact "zero that reads as measured" the round forbade, arriving through the
  guard rather than around it. Ruled out as harmless: the two realistic toolchain bumps *are* caught
  (`-sMALLOC=emmalloc` fails the identity, `u+f = 1 744` against `a+h = 134 144 384`; `-sMALLOC=mimalloc`
  is a link error), so the guard is fail-safe **today** and the defect is the claim, not the current
  reading. Right: a falsifiable probe instead of an identity — read `uordblks`, `malloc` a known
  `kProbe`, read again, `free`, read again, and require `after − before ∈ [kProbe, kProbe + 64)` and
  `back == before`. That fails on an all-zero struct, on a permuted order and on a stub, and costs one
  allocation at static init. Reproduction: the probe above, written as one translation unit — the file the original reading used lived under the system temp directory and is gone, which is why the recipe and not the path is what this line carries.
- **A correct comment was recorded as wrong, from a run with the wrong thread count.**
  `world/TerrainLoader.cpp:41-44` states "at 256 KiB per z14 grid this is 4 MiB a thread, 24 MiB at the
  six-thread ceiling", and the ledger's first reading was published as proof that "~24 MiB was wrong".
  The run measured had **`threads=4`** (a deleted run log: `threads=4
  inFlightCap=4 demCacheTilesPerThread=16`). The measurement closes exactly on four:
  `16 404.421875 KiB = 16 798 128 B = 64 × 262 144 + 4 × 5 228`, i.e. 16 full slots per thread at
  exactly 256 KiB and 5 228 B of `osmmesh_ctx` each (`dem_lru[128]` × ~40 B + ~108 B). So the comment is
  right to the byte and the correction is the error. Right: strike the correction; and note what the
  same arithmetic exposes — `kMaxTileThreads = 6` is a ceiling this host never reaches, so
  `inFlightCap` is 4 against the browser's six connections per origin, and the comment that ties those
  two numbers together ("which is why the in-flight cap and the thread count are the same number") is
  giving away a third of the transport on any host that reports six cores.
- **A 1 Hz probe is quoted as a peak of quantities that change every frame.** `HeapProbe::Sample()` was
  removed from `Outshine::CloseFrame` and now runs only inside `MemoryTelemetry::SampleTelemetry`, so
  `heapPeakKB` — the number a fixed linear memory has to be sized from — is the largest of ~1 sample per
  second, not of ~48. The same defect is already visible in the record: `poolSchedulerKB`, whose `Done_`
  map holds finished vertex buffers, reads **32 / 282 / 1 645 / 4 420 / 5 873 KiB** as the "peak" of five
  runs of one scene and one binary (a deleted run log, `…165201Z`,
  `…165641Z`, `…165743Z`, `…165844Z`), every large value landing at t = 1–18 s during load. A 180× spread
  across replicates of the same run is a sampling artefact, so the true peak is unknown and the quoted
  282 KiB is the friendliest of the five. Right: the high-water mark is kept where the quantity changes —
  `TilePool` already holds `QueueMutex_` when `Done_` grows — and the ledger reads the mark, not an
  instantaneous walk.
- **One measured pool already sits outside `Pools`, and forgetting the next one still compiles.**
  `poolRegionsKB` is `Sim_.GeneratorHeapBytes()`; it is a published column and is folded into
  `poolSumKB` by hand at the telemetry site (`pools.Sum() + generator`), not by `Pools::Sum` — the file
  that did it went with the browser-era clients, and the shape returns with its replacement.
  So the claim that "a measured pool outside the sum no longer compiles" is false in both directions: a
  tenth field added to `World::Pools` and omitted from `Sum()` compiles silently, and one pool is outside
  the struct today. `C.41` is about constructors leaving an object fully initialised and does not bear on
  this at all. Right: `std::array<size_t, (size_t)Pool::Count>` indexed by an enumeration with `Sum()` an
  accumulate — then a new pool is a new enumerator and the sum covers it by construction, which is what
  the open registry line in the old scope ledger §0.1 asks for.
- **`TelemetryBus::Tick` never checks that a source pushed as many fields as it declared**
  (`core/io/Telemetry.cpp:27-33`). The header is written once from the schema and every row is written
  from `Row_.Fields()` with no comparison, so a source that declares N channels and pushes N−1 shifts
  every column to its right in silence, for the whole run and every run after. This round added five
  channels and split the pushes across three new private functions, which is exactly the shape that
  makes it easy; the counts do match today — verified in a deleted run log,
  column 74 is `heapResidualKB` and `heapKB − poolSumKB − heapResidualKB = 0` in all 138 rows. Right:
  `Push` takes the channel it fills, or at minimum `Tick` refuses a row whose size is not the schema's.
- **`Heap.cpp`'s exhaustion line prints `liveBytes=0` when the layout guard failed.**
  `core/io/Heap.cpp:16-20` formats `HeapProbe::LiveBytes()` unconditionally, and `LiveBytes()` returns 0
  when `LiveBytesKnown()` is false. The CSV was taught that an unmeasured quantity is empty; the abort
  message, which is the one place a reader has nothing else to go on, still prints a zero that reads as
  measured. The root is the interface: `LiveBytes()` returns `size_t` and answers 0 for "unknown", so
  every caller has to remember `LiveBytesKnown()` and one already does not. Right: `[[nodiscard]] bool
  TryLiveBytes(size_t *out)`, the shape `core/GroundSample.h` and `core/WaterDepth.h` already carry —
  and **not** `std::optional<size_t>`, which was this line's earlier recommendation and is wrong for
  the same reason the defect exists: `*opt` reads the payload without anyone having consulted the
  state, so the zero would stay spellable. With `Try` the number is unreachable except through the
  answer (`I.13`-style reasoning: make the interface carry the invariant, not the comment).
- **The probe's published cost is the frame thread's wait, not the work it imposes.** `mallinfo()` walks
  every chunk under dlmalloc's global lock, `TilePool::SchedulerBytes()` walks `Queue_` and `Done_` under
  `QueueMutex_`, and `TilePool::ByteCacheBytes()` walks the whole table under `CacheMutex_` — all three
  once a second, all three stalling the six worker threads for their duration. `heapProbeMs` (p50 0.21,
  p99 0.53–0.81, max 1.49 ms over five runs) measures only the caller. In thread-milliseconds the ledger
  costs up to seven times what it publishes, and the tile threads are where the world is built. Right:
  measure the stall on the worker side too, or stop walking — an allocation counter and per-pool
  high-water marks answer the same questions in O(1).
- **The frame-distribution comparison offered as evidence cannot fail.** The round reports the frame
  distribution "indistinguishable before and after" with `maxMs` 23.44 → 23.83. One probe per second at
  the measured 48 fps (a deleted run log, `fps` 47.1–50.0 from t = 41 s)
  touches 2.1 % of frames, which lands at p98 — *below* the published p99 — and `maxMs` is a single order
  statistic, so a 1.5 ms addition is invisible by construction at every statistic quoted. The p50 is
  21 ms, so the addition is 1 % of a frame that is already 26 % over its 60 Hz budget. The host load the
  round disclosed is not the weakness; the design of the comparison is. Right: force the probe every
  frame for one declared run and compare the distributions of that against the unprobed run — then the
  effect is 100 % of frames instead of 2 %, and the per-frame cost follows by division.
