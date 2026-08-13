Type: bug
Area: core
Tags: oracle, instrument

**Bounds, allocation, and what the platform hides**

*Measured 2026-08-11 in `/private/tmp/claude-501/-Users-cosmo-Git-flightbox/b5db31bd-4b15-4bfc-83c1-21cc63c39b74/scratchpad`,
emsdk 6.0.3 / node 26.7.0 / clang, all at `-O2`: an index 400 kB past a live `std::vector` writes real
bytes and exits 0 **in the browser and on the native oracle alike** — the address is inside a mapped
heap in both cases. The premise "it segfaults natively" holds only for a write that leaves the mapping,
which a heap overrun almost never does. So the oracle is not louder than the browser for this class,
and the conclusion is stronger rather than weaker: there is no safety net on either target today.*

- **The one bounds check the whole tree leans on can be defeated by arithmetic.** `core/Span.h:33`,
  `Span::Sub`, asserts `first + count <= Size_` in `size_t`. The sum wraps, so `Sub(4, SIZE_MAX - 2)`
  passes the assert and returns a span of `SIZE_MAX - 2` elements over `Data_ + 4`; every subscript of
  that span then also passes `assert(i < Size_)`. Reachable through `generators/draw/DrawSet.cpp:21`,
  `placed.Sub(range.First, range.Count)`, where both arguments are computed. Right:
  `assert(first <= Size_ && count <= Size_ - first)` — one line, no runtime cost, and it is the
  difference between a checked type and a type that looks checked. `ES.103` (no overflow) and
  `Bounds.4`-style reasoning: the check that guards the range must not itself be unguarded.
- **The two directories that do the most unchecked pointer arithmetic hold no assertion at all.**
  Runtime `assert` sites, measured over 285 files and 33 777 lines: core 2 · core/io 0 · world 2 ·
  **world/terrain 0** · generators 5 · generators/draw 1 · render 1 · **render/stages 0** · clients 1 =
  **12**, one per 2 815 lines. `world/terrain/` is the only C-ABI code in the tree and indexes raw
  `float*` grids throughout; `render/stages/` is 19 files that compute every GPU buffer offset and
  extent by hand. (The figure "28 asserts" in circulation is `grep -c 'assert('` and counts the 16
  `static_assert`s; the runtime half is less than half of it.)
