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

## FOUND -- two runs of one test were writing to one log, for over an hour

**Measured with `ps`:**

```
34845  01:16:15  .../tools-driver-stills-StillsAreTakenAlongTheDriveForTheEye
43667     59:30  .../tools-driver-stills-StillsAreTakenAlongTheDriveForTheEye
```

**Two processes from OLDER builds, alive for 76 and 59 minutes, both appending to the log a third run
had just truncated.** The binary on disk carried the current prints -- `DRAWS the stage` present,
`JOINED parts` absent -- while the log carried 32 `JOINED` lines and no `DRAWS` at all. A string that
exists nowhere in the tree was being written to the log of a test whose source no longer contains it.

**`: > log` sets the size to zero and does not move another process's stored write offset**, so a
truncated log refills with the older run's output and reads as though it were the new one's.

**It is my own doing in this session** -- overlapping runs launched while earlier ones were still
going, with `pkill` patterns that never matched. But the harness makes it possible: **one log path per
test name, no run identity in it, and nothing stopping two runs of the same test at once.**

## What must be true

- [ ] **A test's log holds that test's run and nothing earlier** -- truncated when the run starts, or
      stamped so a reader can tell which run a line belongs to
- [ ] **A second run of a test that is already running is refused**, or given its own log
- [ ] **A stale read is impossible rather than merely unlikely**: if the log carries the run's
      identity, a tool that greps the wrong one gets nothing instead of yesterday's answer

## Comments

**Filed because it cost hours rather than minutes.** The engine's own rule is that an instrument's
domain is part of its claim; a log whose domain is *"every run since someone last cleared it"* answers
questions nobody asked.
