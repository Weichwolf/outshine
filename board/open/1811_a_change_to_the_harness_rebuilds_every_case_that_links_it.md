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

- [ ] A case rebuilds when any header it includes directly changes, `test/harness/shared/` most
      of all, since every case in the tree links it.
- [ ] Proving test: touch a harness header, run a suite, and the case is rebuilt rather than
      re-run. Negative control: the stamp's coverage removed -> the stale binary runs again.
