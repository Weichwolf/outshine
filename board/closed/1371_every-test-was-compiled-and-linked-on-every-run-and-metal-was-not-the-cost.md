Type: bug
Area: harness
Tags: perf, instrument

**Every test was compiled and linked on every run, and Metal was not the cost**

**The guess was the Metal compiler and the measurement refutes it.** Apple's shader compilation is
warm-cached by macOS and it is not where the time goes:

| | |
|---|---|
| a test that creates **no** device | **0.00 s** |
| a device **plus eight shader variants** compiled from our own text | **0.13 s**, and identical on the second and third run |
| one render case, cold then warm | **0.57 s** then **0.26 s** |

## Where it actually went

[MEASURED] on `outshine/shader`, 24 test arms:

| | |
|---|---|
| sum of the tests' own reported time | **7.7 s** |
| warm wall clock for the whole suite | **21.7 s** |
| **unaccounted for** | **14 s — nearly two thirds** |

**`run.sh` compiled and linked every test source on every run, three arms each.** The binary's
modification time changed on a run where nothing had. Eight tests times three arms is 24 compile-and-link
commands at roughly 0.6 s, which is the 14 s exactly.

## The repair is the tree's own pattern, not a new one

`-MMD -MP` already produces a `.d` file for every library object. The test binaries now get one too, and
**two things decide freshness because one is not enough**:

- [x] **the `.d` prerequisites** — every header the compile actually read, so a touched header rebuilds
- [x] **the command, written beside the binary** — `$compileDefine` splices a layer's own include set
  into the binary, so a widened include set must rebuild even when no file moved and no timestamp changed

**[MEASURED] before and after, same suite, same 24 green:**

| | |
|---|---|
| warm wall clock | **22.5 s → 7.1 s** |
| speedup | **3.2x** |

## And the check was tested for the direction that matters

**A freshness check that fails to rebuild is a machine for serving stale binaries**, so it was exercised
in all three directions rather than trusted:

| | |
|---|---|
| a header the test reads, touched | **rebuilt** |
| the test's own source, touched | **rebuilt** |
| nothing touched | **skipped** |

*The first two are the ones worth running. A check that only ever skips looks identical to a fast build
until the day it serves a binary of code nobody is running.*
