Type: debt
State: open
Area: base, engine, world, client
Tags: architecture, owner
Supersedes: 2113

# A number is a COUNTER a class declares, and the frame pulls it -- from every tier

**Benchmark** -- Unreal declares a counter with `DECLARE_CYCLE_STAT` and it appears in `stat`
groups without anyone writing a getter: the counter REGISTERS, it is not fetched; `UE_LOG` is
for events and Insights for mass data. RAGE does the same through telemetry channels beside
`bkBank`, logs for faults, the timebar for the frame. **Both agree**: three channels, and
FREQUENCY decides which -- a rare event is a LOG line, a per-frame aggregate is a STAT, a per-
entity-per-frame value is a TRACE. A measurement that needs its reader to know its name goes
unread.

## Where it stands, measured 2026-09-04

```
  LOG     LogTag enum in the door, 32 calls (23 error, 1 warn, 8 info)      DONE
          events as declared labels                                          0 of 32; {"msg", ...} 11 times
  STATS   Core::Ledger, engine tier only; world/ground CANNOT publish        the reason its
          classes once logged their numbers as text
          Making::NoteNames() / Yield::Notes() -- the pull pattern           exists; Notes() has ZERO readers
  BUS     src/base/io/Telemetry.h: Register, SetSink, Tick, schema, row     no source, no sink, no Tick
  CLIENT  PlaceCamera.cpp:372-406 three vector<double>, sorted for quantiles the hand-rolled shape
  getters nobody reads                                                       StackProbe::*, TilePool::SchedulerBytes
```

The owner's rule from the round that took the percentile vectors out of `Laying.cpp` stands and
narrows this item: a stat is a COUNTER or an EXTREME carried in O(1); nothing sorts, hashes or
takes a quantile to print a number. The client's frame series is the exception that proves the
shape -- p50/p95/p99 over 120 frames is what the frame bench IS -- and it belongs in one place.

## The solution

One pattern for every tier, the one `Making::NoteNames` already has: a class DECLARES what it
counts (`static constexpr` names) and exposes the counts; the frame PULLS them into the ledger.
`Yield::Notes()` gets its reader in `Asking.cpp` on the same day, which is what board:2111 is
waiting on. The ledger's row moves DOWN to `base/` so `world/ground` can fill one without
reaching the engine -- the ledger is a base type and the engine only owns the ONE that is
published. `Telemetry.h` is either that row type or it is deleted; two mechanisms for one job is
the finding, not the keeping.

The log's events become declared labels: `namespace Says` per file, `constexpr` names, the
eleven `{"msg", ...}` seams first, so a `static_assert` can hold the set the way the tree holds
shader entries.

## What will be true

- [ ] `world/ground` publishes counters through the same pull the generators use; the
      `Clock()` helpers and the getters nobody reads are gone or read
- [ ] `Yield::Notes()` is read and its eight names stand in the ledger
- [ ] `Telemetry.h` is the ledger's row or it is deleted
- [ ] The client's quantiles are computed once, in `include/math/Quantile.h` (already there),
      over a series the engine hands back (`Engine::bench` already does), and `PlaceCamera.cpp`
      holds no vector of its own
- [ ] 0 log calls carry a free-text `msg`; every event is a declared label

## What will show I was wrong

If declaring a counter costs more than a line, the declaration is too heavy and Unreal's
free-text message was right. Count the lines a new counter needs before and after.
