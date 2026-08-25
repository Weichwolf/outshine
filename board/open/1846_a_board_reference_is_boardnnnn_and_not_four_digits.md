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

- [ ] The predicate is `board:NNNN` again -- including the comma list `board:1836,1837` the old
      `NumbersIn(message, "board:")` handled -- or the widening is DECLARED: stated in the
      claim's sentence, argued in an item, and bounded so that a measurement in prose cannot
      stand in for a reference.
- [ ] `Note("processes the walk spawns", ...)` counts the calls it reports, or goes.
- [ ] Proving test: the existing walk, plus a fixture message that touches `board/2528` and
      says only `the corpus is 2528 MB` -> FOUND. Negative control: HEAD -> green, because the
      lax scan cannot tell a size from a reference.

## Comments

- 2026-08-25 -- filed by the hourly review. `board:1842`'s cost argument is sound and the one-call
  walk should stay; what must not stay is a performance change that rewrites what the guard
  means without saying so.
