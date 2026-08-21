Type: bug
Area: harness
Tags: instrument

**A run reads ITS OWN log, and not the last one's**

`test/run.sh` writes each test's output to a path derived from the test's name, and the file
**accumulates across runs**. A tool whose source no longer contains a print still shows that print in
its log, because the line is from a previous binary:

```
$ grep -c "JOINED parts" src/clients/Live.cpp    # removed and committed
0
$ tail -1 <the log>
JOINED parts 260  slots 24  partSlot 260         # still there
$ wc -l <the log>
2863                                              # several runs deep
```

**`CLAUDE.md` names this exact trap** -- *a partial run leaves the previous run's logs in place, saying
nothing about it; a count quoted without the trailer may be a measurement of the past* -- and the
harness's own logs walk into it.

## What it cost, measured in this session

**Three separate diagnoses were read off a stale log**, and one of them was reported before the mtime
was checked. It also cost a wrong conclusion that had to be withdrawn: two runs were compared for
determinism when both had refused and neither had written, so the file being compared was one older
run copied twice.

## What must be true

- [ ] **A test's log holds that test's run and nothing earlier** -- truncated when the run starts, or
      stamped so a reader can tell which run a line belongs to
- [ ] **A stale read is impossible rather than merely unlikely**: if the log carries the run's
      identity, a tool that greps the wrong one gets nothing instead of yesterday's answer

## Comments

**Filed because it cost hours rather than minutes.** The engine's own rule is that an instrument's
domain is part of its claim; a log whose domain is *"every run since someone last cleared it"* answers
questions nobody asked.
