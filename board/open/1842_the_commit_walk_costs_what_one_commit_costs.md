Type: bug
Parent: 1802
Area: test
Tags: claims, cost, hygiene

# The commit walk costs what one commit costs

`test/harness/claims/ACommitCarriesTheItemItNames` runs in the FAST gate and walks every commit
since it was born:

```cpp
test/harness/claims/ACommitCarriesTheItemItNames.cpp:64   const std::vector<std::string> commits = Lines(Ask("git log --format=%H " + range));
test/harness/claims/ACommitCarriesTheItemItNames.cpp:68   for (const std::string &commit : commits) {
test/harness/claims/ACommitCarriesTheItemItNames.cpp:69     const std::string message = Ask("git log -1 --format=%B " + commit);
test/harness/claims/ACommitCarriesTheItemItNames.cpp:72         NumbersIn(Ask("git show --name-only --format= " + commit + " -- board/"), "/");
```

Two `popen` per commit, forever. The range is `born..HEAD` and `born` never moves, so the cost
is `2 * (commits since 2026-08-25)` process spawns on every fast-gate run. At this tree's
observed rate -- 39 commits in the 75 minutes this review covers -- that is roughly 750
subprocesses per working day added to a gate whose whole point is that it is fast, and it never
comes back down. The gate's own bound (`board:1778`) measures the RUN; this case is inside it.

The information the walk recomputes is also immutable: a commit's message and its touched paths
cannot change once written. Every run after the first re-derives the same verdict for the same
commits.

Two more on the same file:

- `std::isdigit` is called at `ACommitCarriesTheItemItNames.cpp:39` and `<cctype>` is not
  included (`:1-5` are `<cstdio>`, `<string>`, `<vector>`, `"Check.h"`). It compiles because
  libc++'s `<string>` drags it in. That is an accident of one standard library.
- `NumbersIn(..., "/")` (`:72`) mines four-digit runs after any `/` in a path. It happens to
  work because `board/<dir>/NNNN_...` puts the digits after the second slash, and no board
  directory name starts with four digits. It is a parse by coincidence where the filename
  grammar is known.

## What will be true

- [ ] The walk is bounded: it judges the commits SINCE the last run it recorded, or it batches
      the whole history into ONE `git log --format=...%H%x00%B%x00 --name-only` and parses the
      stream -- one subprocess, not two per commit.
- [ ] `<cctype>` is included where `std::isdigit` is used.
- [ ] The board-item number is parsed from the filename grammar (`board/<state>/NNNN_`), not
      from "digits after a slash".
- [ ] Proving test: the case prints the number of subprocesses it spawned and the wall time it
      took, and a claim asserts the count is bounded by a constant rather than by the log.
      Negative control: the per-commit loop restored -> the count tracks `git rev-list --count`.

## Comments

- 2026-08-25 -- filed by the hourly review. The rule the case enforces is right and was earned
  by three measured violations; what it costs to enforce grows without bound.
