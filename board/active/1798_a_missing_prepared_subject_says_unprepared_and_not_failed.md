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

- [x] Every twin reading a prepared subject guards it and says `UNPREPARED` with the path.
- [x] Proving test: `run.sh unit/gltf` against a swept corpus rebuilds every subject it needs
      and reports 0 FAIL, 0 UNPREPARED. Negative control: the guard removed from one twin ->
      that twin FAILs and its owner is never rebuilt.

## Comments

- 2026-08-24 -- repaid. Three twins, not two: `TheTriangleProjectsToTheOraclesArea` already
  said UNPREPARED for its SUBJECT and said FAIL for the manifest beside it -- the same absence,
  two words, in one file.

  | twin | what it spent FAIL on |
  |---|---|
  | `AGeneratedBasisIsTheOneTheExporterWrote` | three prepared glTF subjects, unguarded |
  | `ADerivedCameraIsTheFramingRuleAndNotAQuotation` | an empty survey |
  | `TheTriangleProjectsToTheOraclesArea:37` | the case's prepared manifest, while guarding the subject four lines later |

- **Proving test**: `run.sh unit/gltf` against a swept corpus -- **60/60, 0 FAIL, 0 UNPREPARED**,
  having rebuilt `render/khronos/glTF/NormalTangentMirrorTest`,
  `render/khronos/glTF/NormalTangentTest`, `render/khronos/glTF/Triangle` and
  `render/outshine/grown/trs-hierarchy` from the paths the twins named.
- **Negative control**, run: the `Present` guard disabled in
  `AGeneratedBasisIsTheOneTheExporterWrote` with its prepared directory removed ->

  ```
  FAIL unit/gltf/AGeneratedBasisIsTheOneTheExporterWrote
  60 tests: 58 PASS  2 FAIL ... 0 UNPREPARED
  ```

  and `NormalTangentMirrorTest` was never rebuilt -- a CHECK names no path, so the runner
  cannot find the owner. With the guard back: UNPREPARED, owner rebuilt, PASS.
