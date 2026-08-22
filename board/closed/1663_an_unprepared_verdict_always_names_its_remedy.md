Type: bug
Area: test
Tags: harness

**An unprepared verdict always names its remedy**

1660's second half moved the "run test/harness/shared/corpus/prepare.py" suffix out of
`Unprepared` (test/harness/shared/Check.h:72-75 now prints only the caller's message) and
into the corpus callers. Two callers were missed and now print a bare path with no remedy:

- test/unit/gltf/ANegativeDeterminantNeedsNoSecondNormalFlip.cpp:127 —
  `Unprepared(kAsset.c_str())`, kAsset under PreparedRoot() (line 22): a prepared-corpus
  asset whose absence prepare.py cures, and the verdict no longer says so.
- test/unit/gltf/ANodeHierarchyFlattensIntoNamedParts.cpp:30 —
  `Unprepared(kThreeCubes.c_str())`, same class (line 17).

Before the migration both printed the guidance via the central suffix; after it they
regressed to a naked path. Every other corpus caller carries its own "-- run
test/harness/shared/corpus/prepare.py" (e.g. ADerivedCameraIsTheFramingRule…:211,
TheTriangleProjectsToTheOraclesArea.cpp:54, Parity.cpp:1076).

Demanded: the two callers say what cures them, same words as their siblings.

---

Closed: both callers name the cure themselves -- "is not prepared -- run
test/harness/shared/corpus/prepare.py" -- like every other corpus caller since the
Unprepared suffix moved to where it is true.
