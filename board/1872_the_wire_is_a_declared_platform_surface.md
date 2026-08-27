Type: issue
State: open
Area: src, include
Tags: architecture, dependencies, door

# The library's platform surface names the wire, and the public header is a door

**Benchmark** — Unreal: platform surface behind `FPlatformProcess` and friends; the public header names the abstraction, not the OS. RAGE: `sys` layer. **Both agree** — the wire is named at one boundary and nowhere else.

4d4981ec put `#include <curl/curl.h>` (src/host/Fetching.cpp:3) inside `src/`. CLAUDE.md's first
rule says **SDL3 · SDL3_GPU · SDL3_\* are the only platform surface**, and `liboutshine.a` now
requires libcurl. One of two things is true and neither is written down: either a fetching wire
is a shipped BATTERY the rule permits and the rule must say so, or it stays outside `src/` and
the engine takes only the abstract `Data::Transport`. The tree currently does the first while the
charter says the second. This is an owner-level call on the charter, not a repair the queue may
make silently.

The header that came with it is a moved file, not a door:

- `#ifndef CURLTRANSPORT_H` (include/outshine/Fetching.h:1) — the public interface carries the
  include guard of the `tools/` file it used to be; every other header under `include/outshine/`
  is `OUTSHINE_*`, and a guard that names a class that no longer exists will collide.
- The door publishes its worker pool: `<condition_variable>`, `<map>`, `<mutex>`, `<thread>` are
  included by a header a client includes (:4, :6, :7, :9), and
  `std::map<uint64_t, Transfer> Transfers_;` (:56) with `std::vector<std::thread> Threads_;`
  (:60) stand in it. `Engine` hides everything
  behind one `S_`; this one hands the client its mutexes.
- `std::map` keyed on ticket under a mutex is the pointer-chasing shape CLAUDE.md already files
  against `TilePool` — a slot table and a ring do the same work without the tree.

## What will be true

- [ ] CLAUDE.md's platform-surface rule states where a wire lives, argued once, and the tree
      agrees with it.
- [ ] `include/outshine/Fetching.h` guards on `OUTSHINE_FETCHING_H`, exposes no thread, mutex or
      map, and forward-declares its state.
- [ ] Tickets index a slot table; no per-request node is allocated on the fetch path.
