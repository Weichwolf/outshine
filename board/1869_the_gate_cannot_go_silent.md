Type: bug
State: open
Area: test
Tags: gate, process, regression

# One bad source fails ONE case, and never the whole gate

**The symptom is gone; the mechanism is not.** Measured 2026-08-25 at 235e3f47: `make` exits 0,
links `build/liboutshine.a` (163 objects), `build/outshine-driver` and `build/outshine-viewer`,
and `test/run.sh` runs its 25 claims and the corpora to a trailer. The one file was repaired.

The three occurrences that filed this item were an UNDECLARED layer
(`tools/host/DelayedTransport.cpp` at 1af2c00b), a NEW entry point (`apps/driver/src/main.cpp`)
and a source that did not COMPILE (`apps/viewer/EveryCaseTheTreeDeclaresConfigures.cpp:8`, a
missing `Check.h`). Each time the fix was the file, and each time the gate went silent again the
next time some other file broke -- because nothing in `test/run.sh` turns an unbuildable source
into one red LINE instead of an exit. **This item is open until the negative controls below
exist**; a fourth occurrence is otherwise a matter of when.

**Declaring or fixing the one file is not the fix.** A gate that a single source can silence is a
gate whose green means nothing, because the run that would have said so never happened. An
undeclared layer, an unbuildable source and an unlinkable programme are each a FAILING CASE with
a name — one red line in the trailer — and every other case still runs and still reports.

**A fourth occurrence landed, and this time the gate is red for a programme that is not
broken.** Measured 2026-08-25 at 817ea333, a full run in its own worktree:

```
844 tests: 825 PASS  2 FAIL  0 TIMEOUT  0 SIGNAL  2 BUILD  0 SKIP  15 UNPREPARED
run.sh: apps/viewer/src/main.cpp does not BUILD AND ANSWER --help ... (board:1860)
apps/viewer/src/main.cpp:10:10: fatal error: 'Face.h' file not found
run.sh: 1 program(s) build and answer --help, 1 do not; 2 compile through the DOOR alone
run.sh: THE FAST GATE OVERRAN ITS BOUND -- 1205550 ms of RUN over the declared 230000 ms
```

Both halves are wrong. `make` links `build/outshine-viewer` and it runs, and the same script's
own door check -- which passes `-I"$layer/parts"` (test/run.sh:585) -- compiles it. The failing
check is `EveryProgramStillLinks` (:569), which spells `-Iinclude` by hand instead of asking
`GroupIncludes` (:202 declares `-Iapps/viewer/src/parts` for exactly this group) and never
compiles the companion its own `ProgramCompanions` declares at :292,
`apps/viewer/src/parts/Face.cpp`. **One include truth, three spellings**, and the third one is
the only one that fails. The verdict does reach the exit code -- `EveryProgramStillLinks ||
compileBlind=1` (:1443) feeds `red` (:1513) -- so the gate is RED for a programme `make` links
and that runs. A false red costs exactly what a false green does: the next reader stops
believing the line.

**And the run never got as far as that verdict.** The fast gate overran its bound by a factor
of five -- 1205550 ms of RUN against a declared 230000 ms -- and `exit 1` at test/run.sh:1510
leaves before `red` is computed, so the overrun MASKS every count behind it (board:1799).

## What will be true

- [ ] An undeclared source, a source that does not compile and a programme that does not link
      each produce ONE named FAIL, and the rest of the gate runs to its trailer.
- [ ] The trailer counts them: they appear beside "the gate did not run, still compile" as their
      own population, never as an exit.
- [ ] Negative controls, three of them: a file under an undeclared directory, a file with a
      missing include, a programme with an unresolved symbol -> exactly one case goes red each,
      the trailer still prints, and removing it turns that case green.
- [ ] `make` and `test/run.sh` agree on this: `make` may fail loudly on a broken programme, but
      the TEST runner reports.
- [ ] The programme check asks `GroupIncludes` and `ProgramCompanions` for what a programme is
      built from, exactly as `make` does. A programme that `make` links and that answers
      `--help` is not reported broken.
- [ ] A red line reaches the EXIT CODE. A trailer that names a broken programme and exits 0 is
      the same silence this item was filed for, wearing the opposite face.
