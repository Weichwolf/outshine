Type: bug
Area: gltf
Tags: oracle, khronos, instrument

**A unit test reports an absent prepared subject as a reader defect**

`test/outshine/unit/gltf/TheTriangleProjectsToTheOraclesArea.cpp:104-109`. The subject is the triangle case's prepared *scene.glb* -- named in prose rather than backticked,
because § I.26.10 rules it **untracked by design** and it now lives under the system temp root
(board:1364); `manifest.json` is the only tracked file in a case directory. Run against a tree carrying only tracked files (measured
2026-08-12, the manifest copied alone into an empty tree):

```
FAIL test/outshine/unit/gltf/TheTriangleProjectsToTheOraclesArea.cpp:105  the Khronos Triangle reads as a .gltf with its buffer beside it
       test/outshine/render/triangle/scene.gltf: cannot be opened
```

**The harmless reading is real and does not cover it:** the preparer is meant to have run, and the
refusal sentence does name the missing file, so nobody is misled for long. What is wrong is that *my
subject was never prepared* is being spent as *the reader failed*, in the one test in this tree that
checks anything against an outside answer — and it is the test whose red will be read hardest.

**Right** is a tier and not a skip: `board/` § I.20 now carries a `corpus` tier for a test
whose subject is a prepared artefact. Until it exists, the test's own first claim is *the subject is
present*, distinct from *the subject reads*. A `--allow-skip` entry is the wrong answer — it makes the
test green forever, which is the defect class this harness was built to close.


---

**Closed by the backlog adjudication (2026-08-22).** A missing prepared subject reports UNPREPARED -- its own trailer category in run.sh -- never a reader defect.
