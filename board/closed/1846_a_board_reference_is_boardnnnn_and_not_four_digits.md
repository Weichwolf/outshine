Type: bug
Parent: 1802
Area: test
Tags: claims, predicate, drift

# A board reference is `board:NNNN` and not any four digits

`board:1842` replaced two `popen` per commit with one `git log` call. It also, silently and in
the same commit, replaced the predicate the claim exists to enforce.

Before (`ACommitCarriesTheItemItNames.cpp`, at 3f52567e):

```cpp
const std::vector<std::string> named = NumbersIn(message, "board:");
```

After (`test/harness/claims/ACommitCarriesTheItemItNames.cpp:79-89`):

```cpp
    std::vector<std::string> named;
    for (size_t scan = 0; scan + 4 <= message.size(); ++scan) {
      bool four = true;
      for (size_t step = 0; step < 4; ++step) {
        four = four && std::isdigit((unsigned char)message[scan + step]);
      }
      ...
      named.push_back(message.substr(scan, 4));
    }
```

Any isolated run of four digits anywhere in a commit message now counts as naming a board item.
The commit message is prose full of measurements, and this session's own messages already carry
the false-positive class. Measured over the range the claim binds (`3f52567e..HEAD`, 25 commits
touching `board/`):

| | |
|---|---|
| commits that pass ONLY under the lax scan | 1 -- `3f52567e`, which touches `board/1610`, `board/1826`, `board/1831` and writes them as bare `1610,1826,1831` |
| bare four-digit tokens in messages that are NOT board references | `1181`, `1182`, `2528`, `3600` |

`2528` is the corpus size in MB, `3600` is seconds in an hour, `1181`/`1182` are case counts.
The board is at 1843 and climbing; the day `board/open/2528_*.md` exists, a commit that touches
it and quotes a disk number passes a guard that was built to say the log is the record.

The claim's own sentence is unchanged -- *"a commit that touches a board item names it"* -- so
the predicate drifted under a sentence that still says the old thing. That is the exact defect
`board:1824` and `board:1841` were filed against, one level up.

A second number beside it has no origin:

```cpp
test/harness/claims/ACommitCarriesTheItemItNames.cpp:105   Note("processes the walk spawns", 2.0, "popen");
```

`2.0` is a literal asserting the thing the item was closed on. Nothing counts the `popen` calls,
so the day a third one is added the note keeps printing 2.

## What will be true

- [x] The predicate is `board:NNNN` again -- including the comma list `board:1836,1837` the old
      `NumbersIn(message, "board:")` handled -- or the widening is DECLARED: stated in the
      claim's sentence, argued in an item, and bounded so that a measurement in prose cannot
      stand in for a reference.
- [x] `Note("processes the walk spawns", ...)` counts the calls it reports, or goes.
- [x] Proving test: the existing walk, plus a fixture message that touches `board/2528` and
      says only `the corpus is 2528 MB` -> FOUND. Negative control: HEAD -> green, because the
      lax scan cannot tell a size from a reference.

## Comments

- 2026-08-25 -- filed by the hourly review. `board:1842`'s cost argument is sound and the one-call
  walk should stay; what must not stay is a performance change that rewrites what the guard
  means without saying so.

## Sharpened 2026-08-25 -- the predicate is back, the guard against the next drift is not

`9117e1c9` restored it, and correctly:

```cpp
test/harness/claims/ACommitCarriesTheItemItNames.cpp:88
    std::vector<std::string> named = NumbersIn(message, "board:");
test/harness/claims/ACommitCarriesTheItemItNames.cpp:89
    for (const char *also : {"board/open/", "board/closed/", "board/active/"}) {
```

The path spellings are a widening too, and a defensible one: `board/open/1844_...` names an item
as unambiguously as `board:1844` does. The `Note("processes the walk spawns", 2.0, "popen")` this
item also named is gone -- the walk publishes `Note("commits this rule has bound so far", ...)`
(`:106`) instead, which counts something it actually counted.

**The item stays open on its own last box.** Its proving test was to be *"a fixture message that
touches `board/2528` and says only `the corpus is 2528 MB` -> FOUND"*, and there is no such
fixture: `NumbersIn` is a file-local helper, the walk runs only over real history, and nothing in
`test/unit/` mirrors it. So the exact failure this item exists to record -- a predicate rewritten
under a sentence that still says the old thing -- would land again with the gate green, twice in
a row now, and the second time nobody would be reviewing the diff of a performance commit.

- [x] `NumbersIn` (or a named predicate replacing it) has a unit twin that feeds it the four
      forms: `board:1844`, `board:1844,1845`, `board/open/1844_x.md`, and prose carrying `2528`.
      Negative control: `board:1842`'s lax scan -> the fourth case FOUND.

A second defect of the same helper, found by reading it and not by the walk: `NumbersIn` tests
only the FIRST character for a digit and then takes four bytes regardless
(`ACommitCarriesTheItemItNames.cpp:38-39`), so `board:18` yields the token `"18 a"` and a
message naming a two- or three-digit item names garbage. The board's numbers are four digits
today; the helper does not say so and does not check it.

## Closed 2026-08-25 -- the predicate left the case and became a type

Both open boxes are answered by the same move: `NumbersIn` is gone and
`test/harness/shared/BoardNames.h` stands in its place -- a `constexpr` scanner over
`std::string_view`, returning NUMBERS in a fixed table rather than four-byte substrings.

```cpp
inline constexpr std::string_view kMarkers[] = {"board:", "board/open/", "board/closed/",
                                                "board/active/"};
[[nodiscard]] constexpr bool DigitsAt(std::string_view text, size_t at) noexcept;
[[nodiscard]] constexpr Named NamedIn(std::string_view text) noexcept;
```

`DigitsAt` tests all four digits AND refuses a fifth, which is the second defect this item found:
the old helper tested `text[from]` alone and then took four bytes, so `board:18 ` yielded the
token `"18 a"` and `board:18446` yielded `1844` -- an item nobody wrote.

**The twin the item asked for is the compiler.** Seven `static_assert`s in the header carry the
four forms and the three shapes that must name nothing:

| fed | says |
|---|---|
| `board:1844` | one item, 1844 |
| `board:1836,1837` | two, the comma list a closing pair writes |
| `board/open/1844_label.md`, `board/closed/…`, `board/active/…` | the path names as unambiguously as the reference |
| `the corpus is 2528 MB and an hour is 3600 s` | NOTHING -- the lax scan's exact false positives |
| `board:18 and the rest` | NOTHING -- fewer than four digits after a marker |
| `board:18446` | NOTHING -- five are not four |

Two runtime CHECKs stand beside them, because a `constexpr` proof over literals cannot see the
history the walk is handed:

- `unreadable == 0` -- an item file whose directory is not one of the four markers is walked past
  in SILENCE, and silence is the failure mode this whole item is about.
- `overflowed == 0` -- the table is 64 wide; a message that overruns it would read as a commit
  that named fewer items than it did.

Proving test: `test/harness/claims/ACommitCarriesTheItemItNames` (IV.23), 4 CHECKS, 43 commits
bound. Negative controls, all three run:

| control | result |
|---|---|
| the markers reduced to `{""}` -- board:1842's lax scan, any four digits anywhere | **compile error**, `BoardNames.h:75` — `the corpus is 2528 MB and an hour is 3600 s` names an item |
| `DigitsAt` reduced to `text[at]` is a digit -- the four-byte take behind a one-char test | **compile error**, `BoardNames.h:74` — `board:18 and the rest` names `18 a` |
| `board/active/` removed from the markers -- a directory the walk cannot read | `FAIL ...:140 AND EVERY BOARD PATH THE WALK IS HANDED IS ONE IT CAN READ` |

The first two are the sharper result: the drift this item was filed against can no longer land
as a green run, because it can no longer compile.
