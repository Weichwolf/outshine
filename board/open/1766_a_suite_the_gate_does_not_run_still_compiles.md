Type: bug
Area: test, tools
Tags: gate, blind-spot, named-only, interface-drift

# A suite the gate does not run still compiles

`test/run.sh tools` at HEAD, before this item's repair:

```
9 tests: 2 PASS  1 FAIL  2 TIMEOUT  0 SIGNAL  4 BUILD  0 SKIP  0 UNPREPARED
```

**Four of the nine did not compile**, and the fast gate said nothing, because `tools` is
named-only and a suite the gate does not RUN it also does not BUILD.

What the four were:

| failure | cause |
|---|---|
| `Wgs84.h` not found (x4) | `LayerIncludes tools/viewer` (test/run.sh:152) never named `-Isrc/data`, though the suite's own source list compiles `src/clients/GltfStudio.cpp`, whose header includes it |
| `outshine/Scenario.h` not found | the same line never named `-Iinclude`, though the list compiles `src/clients/Live.cpp` |
| `Global(const std::string &) override` hides a virtual | **interface drift**: `board:1621` moved `Script::Host::Global` to `std::string_view`, and this consumer was never rebuilt |
| two `-Wshadow` errors | a `sheet` inside a `sheet` |

The third row is the one that matters. A signature change in `src/` broke a consumer in
`tools/`, the change shipped green, and the breakage sat until an unrelated item ran the
suite by name. `-Wall -Werror` is only a rule for translation units somebody compiles.

This is the compile-time twin of `board:1765`: there the gate was silent about conformance
it did not measure, here it is silent about code that does not build. A corpus needs
fetching; **a compile needs nothing**, so there is no excuse for the second.

## What will be true

1. Every source the tree declares in a suite COMPILES on every gate run, whether or not the
   gate runs that suite. Building is cheap; a translation unit nobody compiles is a rule
   nobody enforces.
2. The check names the suite and the diagnostic, so the failure reads like any other.
3. `LayerIncludes` cannot declare a source it does not give the includes for -- the same
   drift that hid `-Isrc/data` for a year.

## Comments

- 2026-08-23 -- found while closing board:1554, which needed `test/run.sh tools` to prove it
  had not made things worse. The four build failures were already there; the run before the
  change and the run after it were identical, which is how they were noticed at all.
- Repaired in place: `-Isrc/data -Iinclude` added to `tools/viewer`'s includes, the
  `Global` override moved to `std::string_view`, the two shadows renamed. `tools/viewer` now
  builds 4 of 4. Two still FAIL for want of the khronos GENERATOR corpus, which is a
  different absence (and one those two report as FAIL where they should report UNPREPARED --
  filed separately if it survives the next round).
