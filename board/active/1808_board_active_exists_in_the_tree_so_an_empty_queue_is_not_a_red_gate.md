Type: bug
Area: board, test
Tags: process, gate

# board/active/ exists in the tree, so an empty queue is not a red gate

`HEAD` is RED on the fast gate, in a fresh checkout, for the sole reason that nothing is being
worked on right now.

```
$ git ls-tree 959a0d23 board/
040000 tree f23096d7c7db39da8e2a39d84618791904ad82cd	board/closed
040000 tree 189418b77f0a24b5779bd5029596ab60d48394a4	board/open
```

`board/active/` is not in the tree at `HEAD` -- git does not carry empty directories, and
`959a0d23` moved the last active item out. `CLAUDE.md:441` cites it:

```sh
ls board/active/                                     # in flight NOW
```

and `harness/claims/EveryPathCitedInADocumentResolves` walks exactly that:

```
NOTE   CLAUDE.md:441 cites board/active/, which is not in the tree
NOTE citations that do not resolve = 1 citations
FAIL test/harness/claims/EveryPathCitedInADocumentResolves.cpp:138
CHECKS 2 FAILURES 1 SKIPPED 0 UNPREPARED 0
```

Measured in a clean `git worktree` at `959a0d23` (the reviewer's own gate, this round).

## Why the main nest does not see it

In `/Users/cosmo/Git/flightbox` the directory still stands on disk as an untracked leftover of
the `git mv` that emptied it. So the gate's verdict there is a function of a stale directory
entry that no commit carries, and every fresh clone, every CI run and every reviewer worktree
disagrees with the nest. That is worse than the red itself: **the gate's answer depends on
untracked filesystem state.**

## What will be true

- [x] `board/active/` is part of the tree whether or not an item is in flight -- a tracked
      marker file, the same way any repository keeps a directory that its process requires.
      `board/active/.gitkeep` is the cheap form; a `board/active/README` that states the
      directory's contract is the honest one, and `CLAUDE.md`'s state machine already writes
      that contract.
- [x] `EveryPathCitedInADocumentResolves` stays exactly as strict -- it is right, and it caught
      a real hole. Nothing about the claim is relaxed.
- [x] Negative control: the marker removed -> the claim names `CLAUDE.md:441` again, which is
      the measurement above.

## Comments

- 2026-08-24, reviewer round -- the empty queue is a legitimate and desirable state (the board's
  own definition of done is "the board holds no open item"). A process whose success state
  turns its own gate red is a process that punishes finishing.

---

## It costs a second verdict, and that one is a crash

`harness/claims/BoardActiveNamesWhatTheQueueIsWorking` does not report on the same tree --
it ABORTS:

```
SIGNAL  harness/claims/BoardActiveNamesWhatTheQueueIsWorking    286 ms
libc++abi: terminating due to uncaught exception of type std::filesystem::filesystem_error:
  filesystem error: in directory_iterator::directory_iterator(...):
  No such file or directory ["board/active"]
```

`test/harness/claims/BoardActiveNamesWhatTheQueueIsWorking.cpp:29`:

```cpp
for (const auto &entry : std::filesystem::directory_iterator("board/active")) {
```

The throwing overload, on a path the claim does not own. So the full trailer at `HEAD` is

```
257 tests: 255 PASS  1 FAIL  0 TIMEOUT  1 SIGNAL  0 BUILD  0 SKIP  0 UNPREPARED
```

and both non-green verdicts have the same cause.

The irony is on the record: that claim's own comment (`:35-44`) says *"A gate that cannot be
green at the finish line is not a gate"* -- written for `board:1790`, about the same directory,
about the same terminal state.

- [ ] Second box: a claim REFUSES, it does not abort. `std::filesystem::directory_iterator`
      takes an `std::error_code` overload; a missing `board/active` is a `CHECK` with a
      sentence, so the reader sees a verdict rather than a signal. `SIGNAL` must mean the code
      crashed, never that a directory was absent.
- [ ] Negative control for the second box: `board/active` removed with the marker in place ->
      `FAIL` naming the directory, not `SIGNAL`.

## Repaid (2026-08-24)

`board/active/README.md` states the drawer's contract and keeps it in the tree. Not a
`.gitkeep`: the directory has a contract -- what stands here, who moves it, and that an EMPTY
drawer is the board's own definition of done -- and a file that states it is worth the same
byte count as one that says nothing.

**And the second verdict is a verdict again.** The claim read the directory with the throwing
overload of `directory_iterator`, on a path it does not own, so at `HEAD` it ABORTED. A claim
that aborts reports neither answer it could have given, which is worse than either. It reads
with the non-throwing overload now and ASSERTS the drawer is present -- an empty drawer stays a
legal answer, an absent one does not.

- **Proving test**: `harness/claims/BoardActiveNamesWhatTheQueueIsWorking` and
  `harness/claims/EveryPathCitedInADocumentResolves`, both green with the drawer tracked.
- **Negative control**, run: `board/active/` removed entirely ->

  ```
  NOTE the drawer is not in the tree
  FAIL **THE DRAWER THE QUEUE USES IS PART OF THE TREE**
  FAIL harness/claims/EveryPathCitedInADocumentResolves
  27 tests: 25 PASS  2 FAIL  0 SIGNAL
  ```

  Two named failures where there was one named failure and one crash. `EveryPathCitedInADocumentResolves`
  is untouched -- it was right, and it found this.
