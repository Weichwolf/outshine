Type: bug
State: active
Area: apps, test
Tags: driver, stills, gate, timeout

# The driver's eye finishes inside the gate's clock and says where it wrote

The driver is what the engine is judged by, and the only way to LOOK at it is
`apps/driver/test/stills/StillsAreTakenAlongTheDriveForTheEye`. In a clean worktree at
`38641b13` it measures nothing:

```
run.sh: apps/driver/test/stills/StillsAreTakenAlongTheDriveForTheEye was killed after 120 s
TIMEOUT ... 120033 ms
1 tests: 0 PASS  0 FAIL  1 TIMEOUT  0 SIGNAL  0 BUILD  0 SKIP  0 UNPREPARED  0 PARTIAL
run.sh: ... MEASURED NOTHING -- it was killed at 120 s before it finished
```

Its log file is **0 bytes** and it wrote **no PNG at all**: 120 s of Munich--Hamburg fetch and
weave, then SIGKILL, then silence. `TIMEOUT_S=120` (test/run.sh:32) is the gate's default and
the case declares no other; `--timeout` exists (test/run.sh:138) but nothing in the tree says
this case needs it.

The second half of the defect is that a run which DOES finish cannot be told from one that did
not, because the output path is fixed:

```cpp
      (std::filesystem::temp_directory_path() / "outshine-stills").string();
```
apps/driver/test/stills/StillsAreTakenAlongTheDriveForTheEye.cpp:516

Two checkouts write the same directory, a killed run leaves the previous run's pictures
standing, and `ls -t` on them reports the wrong hour. The hourly review is required to take a
FRESH screenshot every round and at HEAD it cannot prove that any picture it reads is this
round's.

## What will be true

- [ ] `test/run.sh apps/driver/test/stills` finishes inside the clock it is judged by, on a
      cold corpus, and publishes what it cost -- fetch, weave, fit, render -- as p50/p95/p99
      rather than as one wall time. If Munich--Hamburg cannot do that, the eye rides the short
      urban route of board:1858 and the country crossing keeps the long one.
- [ ] Every still is written under the run's OWN nest (`OUTSHINE_NEST`) or under a directory
      the case is told, and the case prints the absolute directory it wrote to as its last
      line, so a reader can tell this round's picture from last round's.
- [ ] A run killed before it finished leaves no picture behind that a later reader could
      mistake for its own -- negative control: kill the case at 10 s and show that nothing
      readable is left claiming to be a still of this drive.

## Comments

- 2026-08-25 -- filed by the hourly review after both of its attempts to look at the product
  failed: the first died on board:1860's layer walk, the second on this timeout. The pictures
  the review judged this round are from 07:07 in the MAIN nest, not from its own run, and it
  says so in its report.
