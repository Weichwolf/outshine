Type: task
State: open
Area: src
Tags: hygiene, cpp23, optimisation
Supersedes: 1769

# Boundaries and layouts speak C++23

**Benchmark** — Unreal: `TArrayView`, `FStringView` and `TConstArrayView` at read-only boundaries for exactly this reason. RAGE: raw pointer + count. **Taking Unreal** — a view says "I traverse this" in the type, and a `const&` to an owning container says "I might keep it".

Every read-only boundary that takes `const std::vector<T>&` or `const std::string&` takes
`std::span<const T>` / `std::string_view` instead, and no call site copies into an owning
container just to traverse. Survey: 98 vector-ref and 76 string-ref boundary parameters remained
at the last count. `[[nodiscard]]`, `explicit`, `noexcept` and `constexpr` ride the same sweep
wherever a touched signature is missing them; the reviewer enforces it on every touched file,
and this item is the sweep over the rest of the tree.

**`std::mdspan` is the half that has barely started** — 3 files use it against a completed span
sweep and 8 uses of `std::expected`. Where it bites: a rectangle behind a pointer chase. A table
of rows as a vector of vectors with a `vector<bool>` proxy beside it (`src/scenario/Tables.h`) is
a gather per cell; a tile's field indexed by hand-rolled `y * width + x` is a layout no
compiler can prove contiguous. A field is an mdspan over one contiguous, pointer-free block, or
it blocks SIMD while being correct.

**AND THE TREE CARRIES ITS OWN `Span` BESIDE THE STANDARD ONE.** `src/base/spatial/Span.h` is 41
lines of `std::span` with different spellings -- `Data()`, `Size()`, `Empty()`, `Bytes()`, `Sub()`
where the standard has `data()`, `size()`, `empty()` and `subspan()`. Measured: **136 uses across
56 files.** It is why the sweep above keeps finding boundaries that already take a view and still
count as unswept: they take THIS view. A standard algorithm cannot be handed one without
unwrapping it, and a reader who knows `std::span` has to learn a second name for it. Found while
auditing the tree against SDL3 and the standard (board:2042, which records the rest of that audit
and hands this half here rather than working it twice).

## What will be true

- [ ] The two greps come back empty at every boundary, and a `const vector&` parameter is a
      finding.
- [ ] `src/base/spatial/Span.h` is DELETED and every one of its 136 uses is `std::span`. A second
      view type is not a boundary that was missed, it is a boundary that was answered wrong.
- [ ] Every field, tile and instance stream is viewed through `std::mdspan` over one block.
- [ ] A refusal that carries a reason is `std::expected`, never bool-plus-string, and never a
      line of prose the caller greps. The ONE door does the last of those today:
      `if (line.rfind("REFUSED", 0) == 0 && Why.empty()) { Why = line; }`
      (src/engine/Engine.cpp:35) reconstructs the engine's refusal by pattern-matching the text
      a `Sink` printed for a human, which is why `test/run.sh --drive` says `REFUSED REFUSED`.
