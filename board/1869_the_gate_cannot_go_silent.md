Type: bug
State: open
Area: test
Tags: gate, process, regression

# An undeclared file fails ONE case, and never the whole gate

At 1af2c00b `test/run.sh` runs ZERO cases and exits printing one line:

```
run.sh: tools/host/DelayedTransport.cpp is under test/tools/host, and the harness knows no such
directory -- declare it in LayerIncludes and LayerGroups to run its tests, or in NotTheHarnesses
if the Makefile judges it
```

4d4981ec moved `tools/host/CurlTransport.*` into `src/host` + `include/outshine` and left
`DelayedTransport.cpp` behind with no layer declaration. The whole regression gate went dark on
a file that is not on any frame path. Second time in one hour: the same shape killed the gate
when `apps/driver/src/main.cpp` appeared, and the repair then was to declare the one file.

**Declaring the file is not the fix.** A gate that a single unknown path can silence is a gate
whose green means nothing, because the run that would have said so never happened. An unknown
layer is a FAILING CASE with a name — one red line in the trailer, `N tests: ... 1 FAIL` — and
every other case still runs and still reports.

## What will be true

- [ ] An undeclared source produces one named FAIL and the rest of the gate runs to its trailer.
- [ ] The trailer counts it: undeclared sources appear beside "the gate did not run, still
      compile" as their own population, never as an exit.
- [ ] Negative control: add a file under an undeclared directory -> exactly one case goes red,
      the trailer still prints, and removing it turns that case green.
