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
