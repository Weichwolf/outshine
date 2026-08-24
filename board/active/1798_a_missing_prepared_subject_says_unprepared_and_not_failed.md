Type: bug
Area: test
Tags: process
Parent: 1797

# A missing prepared subject says UNPREPARED and not FAILED

Five unit twins read a prepared subject through `PreparedRoot()`. Three guard it:

```cpp
if (!Present(kThreeCubes.c_str())) {
  Unprepared((kThreeCubes + " is not prepared -- run ...").c_str());
  return Report();
}
```

Two do not, and go red instead:

| twin | what it does |
|---|---|
| `test/unit/gltf/ANodeHierarchyFlattensIntoNamedParts.cpp:29` | guards, says UNPREPARED |
| `test/unit/gltf/ANegativeDeterminantNeedsNoSecondNormalFlip.cpp` | guards |
| `test/unit/gltf/TheTriangleProjectsToTheOraclesArea.cpp` | guards |
| `test/unit/gltf/AGeneratedBasisIsTheOneTheExporterWrote.cpp:59` | **`CHECK(mirrorRead)` -- FAIL** |
| `test/unit/gltf/ADerivedCameraIsTheFramingRuleAndNotAQuotation.cpp` | **FAIL** |

```
FAIL test/unit/gltf/AGeneratedBasisIsTheOneTheExporterWrote.cpp:59
     the mirror case's own glTF reads and flattens
       CHECK(mirrorRead)
```

Two consequences, and the second is the one that bites:

1. **An unfetched corpus reads as a broken engine.** `UNPREPARED` is the tree's honest word for
   "this run judged nothing here" (`board:1765`, `board:1766`); `FAIL` is the word for "the code
   is wrong". A twin that spends the second word on the first condition makes a swept temp
   directory look like a regression.
2. **It defeats the rebuild of `board:1797`.** `RebuildOwner` finds the owning manifest by
   reading the prepared path the case NAMES in its log. A `CHECK` failure names no path, so the
   runner cannot tell which case owns the missing subject and cannot rebuild it. The three
   guarded twins healed themselves in the same run; these two did not.

## What will be true

- [ ] Every twin reading a prepared subject guards it and says `UNPREPARED` with the path.
- [ ] Proving test: `run.sh unit/gltf` against a swept corpus rebuilds every subject it needs
      and reports 0 FAIL, 0 UNPREPARED. Negative control: the guard removed from one twin ->
      that twin FAILs and its owner is never rebuilt.
