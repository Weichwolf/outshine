Type: issue
Area: test
Tags: process, layering

# The fast gate does not stand on the sporadic proof's corpus

`CLAUDE.md` splits the suites by what they cost:

> *The unit mirror is the REGRESSION GATE and it is fast; the long driver suites are the
> sporadic full proof, run when named, never per edit.*

`test/unit/gltf/ADerivedCameraIsTheFramingRuleAndNotAQuotation.cpp:334` breaks that split. It
does not read one subject -- it SURVEYS the whole prepared render corpus:

```cpp
for (const auto &entry : std::filesystem::recursive_directory_iterator(kSuite, walking)) {
  if (entry.path().filename() != "manifest.json") { continue; }
  ...begins(kPreparedKhronosPrefix) || begins(kPreparedGrownPrefix)
```

169 declared cases: 148 under `test/render/khronos/glTF`, 21 under `test/render/outshine/grown`.
Measured against a swept temp directory, this one twin reported **160 UNPREPARED subjects** in a
single fast-gate run.

That means the fast gate was green only because a temp directory happened to hold a corpus that
no gate builds and that the machine may sweep at any moment. It did sweep it tonight. The
greenness was borrowed.

Two readings, both true:

| | |
|---|---|
| the twin's subject is right | the framing rule SHOULD be checked against every case the tree declares -- a rule that holds on one subject is not a rule |
| its LAYER is wrong | a survey over the sporadic corpus is a sporadic proof, and it is standing in the fast regression mirror |

`board:1797` now rebuilds a missing subject from its owning manifest, so the gate heals itself
rather than going red -- but healing 160 subjects is 160 Blender runs standing beside a 230 s
bound, which is the sporadic proof wearing the mirror's clothes.

## What will be true

- [ ] The survey moves to where a survey belongs -- a corpus suite run by name -- and the unit
      mirror keeps a twin that proves the framing RULE against a bounded, named set of subjects
      it can always have.
- [x] The fast gate's subjects are ones the gate itself can rebuild inside its own bound, and
      the trailer says how many it rebuilt.
- [x] Proving test: the fast gate against a swept corpus finishes inside `kFastGateBoundMs`
      with 0 UNPREPARED. Negative control: the survey put back into `unit/` -> the bound is
      overrun and the rebuild count is in the hundreds.

## Half repaid, and the open half is the LAYER (2026-08-24)

The gate no longer borrows its greenness. The survey judges what stands and NAMES what does
not -- `board:1765`'s form, applied to a survey instead of a suite:

```
NOTE cases the tree declares                  = 172 cases
NOTE cases whose subject was never fetched    =  47 cases
240 tests: 240 PASS ... 0 UNPREPARED  in 147930 ms
run.sh: gate headroom 145916 ms of 230000 (run 84084 ms, builds 63846 ms beside the bound)
```

125 of 172 judged, 47 named, and the survey is UNPREPARED only if it judged NOTHING. One
swept directory no longer becomes 160 findings.

**Negative control**, run: the per-subject `Unprepared` put back -> 160 UNPREPARED from this
one twin, the gate red, and the runner spending minutes per case rebuilding 127 khronos
subjects -- 19 GB of temp directory before it was stopped.

## What stays open

The twin still surveys a corpus the fast mirror does not own. The coverage it reports is now
honest, and its LEVEL is whatever the machine happens to hold -- 125 cases tonight, 172 after a
full preparation, 0 after a sweep. A regression gate whose coverage moves with a temp directory
is not a regression gate, even when it says so out loud.

- [ ] The full sweep moves to a corpus suite run by name, and the mirror keeps a twin over a
      bounded set of subjects the gate can always have -- the GROWN cases, which the tree
      generates rather than fetches.
- [ ] Proving test: the fast gate's judged-case count is the same number on a swept machine and
      a warm one. Negative control: the fetched families back in the mirror -> the count moves.
