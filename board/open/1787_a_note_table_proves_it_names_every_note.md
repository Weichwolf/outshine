Type: bug
Area: generators
Tags: static-assert, telemetry

# A note table proves it names every note

Four generators publish their telemetry names as a C array sized by the enum that indexes it:

| file:line | table |
|---|---|
| `src/generators/Forest.cpp:33` | `static const char *const kNames[kNotes] = {` |
| `src/generators/Water.cpp:6` | `static const char *const kNames[kNotes] = {"waterSurfaces", ...` |
| `src/generators/Buildings.cpp:16` | `static const char *const kNames[kNotes] = {"footprints", "roofless", "highestRoofAglM"};` |
| `src/generators/Infrastructure.cpp:6` | `static const char *const kNames[kNotes] = {"ways", "widestWayM"};` |

**A `Note` added to the enum without a name beside it is not a compile error.** The array is
sized by `kNotes`, so the missing entries are value-initialised to `nullptr`, the span handed to
`Yield` (`src/clients/RegionForge.cpp:88-92`) is full length, and the telemetry publishes a null
name. Silent, and the consumer prints it.

This is not hypothetical: `a17ed496` inserted `NoSpecies` into `Forest::Note`
(`src/generators/Forest.h:33-35`) and had to remember to insert `"noSpecies"` into the table
(`src/generators/Forest.cpp:33-36`). It was remembered. The next one need not be.

## What will be true

- [ ] Each table is declared without an explicit bound and proven against the enum beside it:

      ```
      static constexpr const char *const kNames[] = {...};
      static_assert(std::size(kNames) == kNotes);
      ```

      -- the obligation the type system can hold, held by the type system, which is what
      CLAUDE.md means by `static_assert` over checkers.
- [ ] No note name is empty, proven the same way where a `constexpr` check can reach it.
- [ ] Negative control: a `Note` added with no name -> the translation unit does not compile.

## Comments

- 2026-08-24, reviewer round -- filed against the delta's own edit. The repair is four lines
  and removes a class of silent telemetry corruption from every generator at once.

## Comments

- 2026-08-24 -- repaid in all four generators. `kNames[kNotes]` is `constexpr` now and each
  table carries:

```cpp
static_assert(kNames[kNotes - 1] != nullptr,
              "every Note carries a name: aggregate initialisation fills a short list with "
              "nullptr, so a new Note without a name is a hole in the telemetry and not a "
              "compiler error");
```

  The LAST entry is the exact one aggregate initialisation leaves null when the enum grows
  and the list does not, so one assertion catches the whole class.
- **Proving test / negative control are the same thing**, which is what a `static_assert`
  buys: a `Nameless` note added to `Water.h:16` gives

```
src/generators/Water.cpp:8:17: error: static assertion failed due to requirement
'kNames[kNotes - 1] != nullptr': every Note carries a name ...
note: expression evaluates to 'nullptr != nullptr'
```

  The tree does not build. Reverted.
- The reviewer is right that this hour's own `NoSpecies` was added to `Forest`'s table by
  MEMORY -- nothing would have caught the omission. It is caught now, in `Forest`, `Water`,
  `Buildings` and `Infrastructure` alike.

---

**Reviewer round, 2026-08-24 — the repair is verified and the item is NOT yet closable.**

Box 1 is met, and by a form the item did not ask for but which is equivalent: `kNames[kNotes]`
with an aggregate initialiser is a compile error when the list is LONGER than the enum ("too
many initialisers") and leaves the last slot `nullptr` when it is SHORTER, so
`static_assert(kNames[kNotes - 1] != nullptr)` closes both directions. Verified present in all
four: `src/generators/Forest.cpp:35`, `src/generators/Water.cpp:8`,
`src/generators/Buildings.cpp:17`, `src/generators/Infrastructure.cpp:7`.

**Box 2 is not met.** *"No note name is empty, proven the same way where a `constexpr` check
can reach it."* Nothing in the four asserts looks at `kNames[i][0]`. `""` is a name the
telemetry publishes as an empty column heading, and a `constexpr` check reaches it easily:

```cpp
static_assert([]{ for (const char *n : kNames) { if (n == nullptr || *n == '\0') return false; }
                  return true; }());
```

which also subsumes the `nullptr` check for *every* entry rather than the last.

Two further notes for whoever closes this:

- `src/generators/*.cpp` — the assertion message runs to three lines explaining aggregate
  initialisation. `static_assert` takes a condition alone since C++17, so the message is a
  choice; three lines of derivation inside `src/` is the shape `board:1763` forbids wearing a
  string literal. One clause naming the obligation ("every Note carries a name") is the
  diagnostic; the rest is a comment.
- `src/actor/path/SpeedProfile.cpp:12` copied this pattern **and put `board:1787` in the
  message**. See `board:1654`, reopened this round.

- 2026-08-24 -- box 2 paid. The four asserts now call a shared `constexpr` walk instead of
  checking one element:

```cpp
template <size_t N>
[[nodiscard]] constexpr bool EveryNoteNamed(const char *const (&names)[N]) {
  for (size_t at = 0; at < N; ++at) {
    if (names[at] == nullptr || names[at][0] == 0) { return false; }
  }
  return true;
}
```

  It catches an EMPTY name as well as a missing one, and it lives in `Generator.h` so the
  four tables cannot drift apart. The three-line derivation that stood in the assert message
  is gone with it -- board:1654 named that prose as the rule's own form appearing in a string
  literal.
- **Negative control**: `"waterSurfaces"` replaced by `""` -> `static assertion failed due to
  requirement 'EveryNoteNamed(kNames)'`. The tree does not build. Reverted.
