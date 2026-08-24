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
- [ ] The fast gate's subjects are ones the gate itself can rebuild inside its own bound, and
      the trailer says how many it rebuilt.
- [ ] Proving test: the fast gate against a swept corpus finishes inside `kFastGateBoundMs`
      with 0 UNPREPARED. Negative control: the survey put back into `unit/` -> the bound is
      overrun and the rebuild count is in the hundreds.
