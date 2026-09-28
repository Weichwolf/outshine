Type: feature
State: active
Architecture: ready
Parent: 2312
Depends:
Priority: P0
Area: base, generators
Tags: geometry, proof, lod, bounded

# Native surface refinement preserves complete coverage

## Problem, evidence and existing capability

2312 supplies outward point/triangle queries, rounded point/region enclosures and
resumable native mesh envelopes. The envelope compares all corners with one target
point and is valid but too loose for useful LOD selection. The independent filled
opening has Hausdorff distance 0.5 m despite zero source-vertex error. A prototype
reaches [0.4921875, 0.5096568] m; this does not prove native adaptive code.

## Ownership and implementation

- generators/building/StructureSurfaceRefinement.h/.cpp owns a worker-side task
  composed with StructureSurfaceErrorTask. Keep that coarse-envelope contract intact.
  Inputs are the same immutable scoped Raised pair, pinned source key and common frame.
- Establish complete validated coarse coverage before adaptive work. New scoped views
  must not outlive the pinned inputs. Cancellation/source changes revoke every result.
- Two bounded heaps hold source-triangle regions, one per direction. Start each from
  all native wall/roof index runs. Each node owns three PointEnclosures, an interior
  sample, its enclosing radius and a conservative upper distance. Deterministic ties.
- Evaluate one target triangle per work unit. Upper distance is the minimum valid
  target upper estimate plus region radius, rounded outward. The sample lower bound
  requires minima over ALL target triangles, minus its enclosure deviation rounded
  down. Samples establish lower evidence only; radius establishes interior coverage.
- Refine the largest upper region into four exact geometric children using enclosed
  midpoints. Keep the parent's certificate until all four children are evaluated;
  publish replacement atomically. Child upper bounds may inherit the parent's upper.
- Initial hard caps: 65536 triangles/input, 131072 triangle queries, 4096 live regions,
  target global interval width 0.02 m [SET]. Slice budget bounds primitive work.
  Reusable task-owned scratch is bounded. Count queries, splits and heap capacity.
- Exhaustion returns the last complete upper certificate, with its proved lower
  evidence. Input count beyond region capacity skips adaptive precision, never coverage.
  Zero query budget retains the coarse bound. Invalid/nonfinite arithmetic fails closed.
- This slice has no renderer consumer and grants no smaller runtime LOD error.
  2312 still owns StructureBuildTask/product integration and independent visual acceptance.

## Falsifiable acceptance

- Native filled opening encloses analytic 0.5 m with interval width <= 0.02 m.
  Reversed direction, triangle order and differently tessellated planes preserve safety.
- Parallel planes, collapsed triangles, sloped profiles and native BuildingMesh pairs
  retain complete coverage at every exposed certificate; no vertex-only oracle.
- Tiny slice/query/region budgets, mid-child exhaustion, cancellation and stale keys
  cannot expose partial coverage. Copy/move preserve independent scoped progress.
- Removing radius, midpoint deviation, all-target lower minima or source invalidation
  must cause actual test FAIL, not BUILD. Independent analytical/rational controls.
- make format; make suite SUITE=outshine/src/generators/building/StructureSurfaceRefinement;
  existing TriangleDistance/TriangleRegion/StructureSurfaceError suites; full make lint.
  Measure native query counts and scratch bytes before selecting runtime caps.

## Current evidence

Native adaptive case: 55 checks, zero failures in a direct optimized build. Removing
region radius or using only the first target lower bound each causes actual FAIL.
One-unit and arbitrary slices, exhausted initialization/child budgets, reversed pairs,
parallel/tessellated/collapsed surfaces and source/cancel/copy/destination-move guards pass.
Commit 618371b14: make format, all four official focused suites and full make lint
including clang-tidy pass. Gate result: /tmp/outshine-refinement-restored-gate-result.txt;
logs: /tmp/outshine-refinement-restored-{focused,full-lint}.log. No runtime consumer.
Native BuildingMesh/sloped/large-anchor/permutation controls and measured runtime
scratch/query utility remain open; passing this CPU gate does not finish 2313.

## Priorisierter Move-Vertragsfehler

ASan/UBSan-Probe 2026-09-28: Reset(cap/frame), Step(100), move(task), dann Step auf
dem Quellobjekt ergibt SEGV in UpdateUpper. Quelle meldet RegionCount=0 und weiterhin
Bound; der implizite Move überträgt Heaps, lässt Phase/Teilfortschritt aktiv.
Log: /tmp/outshine-review-moved-refiner.log; reproduzierbare Quelle im gleichen Temppräfix.
Der bestehende Test benutzt nur das Move-Ziel und deckt diese Grenze nicht ab.
Nächster Schritt vor weiteren Fixtures: expliziter Transfer mit gültigem widerrufenem
Quellzustand. Ziel behält vollständigen Fortschritt; Quelle liefert kein Bound und
Step einen erwarteten Fehler, darf Cancel/Reset und anschließend neue Arbeit ausführen.
Copy bleibt unabhängig bei denselben gepinnten Inputs; Kopien außerhalb des Framepfads.
Move-Konstruktion und -Zuweisung während Seed/Child/Split/Complete prüfen, auch Self-Move;
Rückkehr zum impliziten Move muss den Guard tatsächlich verletzen. Kein Entfernen des Tests.
