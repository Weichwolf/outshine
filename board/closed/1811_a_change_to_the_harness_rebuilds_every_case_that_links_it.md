Type: bug
Area: test
Tags: gate, staleness, measured

# A change to the harness rebuilds every case that links it

`Check.h` gained a field (`board:1810`) and the gate ran the OLD binary:

```
$ grep '^CHECKS ' .../apps-driver-APlannerFindsTheRoadFromMunichToHamburg.log
CHECKS 43 FAILURES 2 SKIPPED 0 UNPREPARED 0        <- the format before the change

$ grep -c 'Check.h' .../apps-driver-APlannerFindsTheRoadFromMunichToHamburg.d
0
```

The case's first include is `"Check.h"` and its dependency file does not mention it. What the
file DOES hold:

```
apps-driver-APlannerFindsTheRoadFromMunichToHamburg: \
  tools/host/CurlTransport.cpp tools/host/CurlTransport.h \
  src/data/Transport.h
```

## The mechanism

`test/run.sh:1233` compiles the case source AND its layer's extra sources in one command with a
single `-MF "$plainBinary.d"`. The compiler writes the dependency file for **one** translation
unit -- the last -- so the `.d` describes `tools/host/CurlTransport.cpp` and nothing about the
case itself.

`BinaryStamp` (`test/run.sh:1109-1120`) then builds its freshness stamp from that `.d` plus the
source paths and the library objects. So:

| what changed | does the case rebuild |
|---|---|
| the case's own `.cpp` | yes -- it is in `stampFiles` by name |
| a library header | yes -- through `$OBJECTS`, which the library build already refreshed |
| **a header the case includes directly**, `Check.h` among them | **no** |

The gate therefore runs a stale binary whenever the harness changes, and reports its verdict as
if it were current. It was caught here only because the trailer FORMAT changed and `ReadTrailer`
refused the old line -- a change that altered behaviour without altering the format would have
passed silently.

## What will be true

- [x] A case rebuilds when any header it includes directly changes, `test/harness/shared/` most
      of all, since every case in the tree links it.
- [x] Proving test: touch a harness header, run a suite, and the case is rebuilt rather than
      re-run. Negative control: the stamp's coverage removed -> the stale binary runs again.

## Repaid (2026-08-24)

`BinaryStamp` stamps `test/harness/shared/*.h` and `test/harness/shared/render/*.h` by name.
Every case in the tree links the harness, and a single `-MF` over a multi-source compile cannot
describe more than one translation unit -- so until the build gives each source its own
dependency file, the harness is stamped explicitly and the reason is written where the stamp is
built.

- **Proving test**: an isolated experiment with a settling run, because a leftover stamp from
  the fixed build masks the defect:

  | | |
  |---|---|
  | harness headers OUT of the stamp, settle, then edit `Check.h` | the case is **not** rebuilt -- the defect |
  | harness headers IN, settle, then edit `Check.h` | rebuilt |

- What found it: `board:1810` changed the trailer FORMAT, and `ReadTrailer` refused the old
  line. **A change that altered behaviour without altering the format would have passed
  silently**, and the gate would have reported a verdict from a binary that no longer matched
  its source.
- Gate 259/259.
