Type: bug
Area: harness
Tags: perf, instrument

**A measurement taken on a busy machine never enters the archive, and UNPREPARED says why**

1476 gave the frame instrument its busy-machine sense: load average against half the
threads, this run's floor against the archive's median
(test/render/outshine/frame/TheFrameCostIsPublishedAgainstItsOwnFloor.cpp:565-608). Two
gaps remain in the same block:

1. **The busy check guards only the failing branch.** `loaded`/`inflated` are consulted
   solely when `priced <= bound` (:601). When the instrument happens to resolve on a loaded
   machine, `Archive()` (:610-612) writes the inflated FloorMs/P50 rows unconditionally --
   and the archived row carries neither load1 nor any population marker, so
   `archivedBound` (the median the busy-detector itself reads, :578-587) is fed by the very
   runs it exists to disqualify. Every number carries its origin and population; these rows
   lost theirs. Demanded: a run with `load1 > quietBound` does not archive (or archives
   with the load in the row and the median reads only quiet rows).

2. **The UNPREPARED channel hardwires the wrong remedy.** Check.h:74 appends
   `-- run test/harness/shared/corpus/prepare.py` to EVERY Unprepared, so the busy-machine
   verdict ends by telling the reader to prepare a corpus that has nothing to do with load.
   Demanded: the remedy text belongs to the caller, not the channel -- Unprepared prints
   what it was given; the corpus callers say prepare.py themselves.

kBusyFloorInflation = [SET] 2.0 with its incident named -- that part stands.
