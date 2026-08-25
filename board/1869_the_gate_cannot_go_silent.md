Type: bug
State: open
Area: test
Tags: gate, process, regression

# One bad source fails ONE case, and never the whole gate

**Third occurrence, third shape, measured 2026-08-25 at a3ebe3e0.** `make` and `test/run.sh`
both exit 2 having run ZERO cases:

```
apps/viewer/EveryCaseTheTreeDeclaresConfigures.cpp:8:10: fatal error: 'Check.h' file not found
run.sh: apps/viewer/EveryCaseTheTreeDeclaresConfigures.cpp does not build into build/outshine-apps
```

With that one directory parked the same HEAD runs 582 cases and reports
`553 PASS 6 FAIL 3 SIGNAL 5 BUILD 15 UNPREPARED` (board:1882). So the gate had a verdict to give
and gave none, for a source under `apps/` that is on no frame path and in no suite.

The two earlier occurrences were an UNDECLARED layer (`tools/host/DelayedTransport.cpp` at
1af2c00b) and a NEW entry point (`apps/driver/src/main.cpp`); this one is a source that does not
COMPILE. All three are the same defect: the gate's build phase treats one file as fatal to the
run.

**Declaring or fixing the one file is not the fix.** A gate that a single source can silence is a
gate whose green means nothing, because the run that would have said so never happened. An
undeclared layer, an unbuildable source and an unlinkable programme are each a FAILING CASE with
a name — one red line in the trailer — and every other case still runs and still reports.

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
