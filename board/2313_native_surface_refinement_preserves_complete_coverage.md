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
- Evaluate one point/target-triangle query per work unit. Upper distance is the minimum valid
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

Native runtime scratch/query utility remains open; this CPU gate does not finish 2313.

## Move-Vertrag geprüft

Commit ef78343aa behebt den reproduzierten Move/Step-SEGV: expliziter Feldtransfer,
Quelle Cancel, Self-Move unverändert, Copy unabhängig. ASan/UBSan 4741 Checks,
Crash-Probe und impliziter-Move-Mutant belegen Widerruf und Reset/Wiederverwendung.
Fünf fokussierte Tests, full lint 256/256 tidy-Units ohne Findings, 32/32 Guards PASS.
Logs: /tmp/outshine-refinement-move-{sanitized,original-probe,mutant,focused,full-lint}.log.

## Native utility probe and next proof decision

Fixed-target corner queries are implemented; official focused/full gates are pending.
ASan/UBSan: existing adaptive 55 checks, move 4669 checks, analytic sloped/collapsed/
translated/reversed/budget cases 456 checks PASS. Native reordered flat self: 5 checks PASS.
Removing convex tightening fails useful precision; replacing corner max with min fails
opening safety. Both are actual FAIL, not BUILD. Logs: /tmp/outshine-convex-bound-
{verification,controls}.log and /tmp/outshine-convex-{native,missing-convex,wrong-corner-min}.log.

Local 20x30 m native probe: flat Fine/Fine uses 61952 queries for a near-zero interval;
Fine/Shell uses 64448 for [1.899999979,1.911157473] m. Pitched 184-triangle self and
184/172 Fine/Shell exhaust 131072 queries during seeding: upper 37.214628331 m versus
previous 6.9483/6.9470 m. Safe but worse utility; four queries/target need a measured
follow-up, not larger caps or runtime acceptance. Log: /tmp/outshine-convex-probe.log.
Six single runs 50–102 ms locally; no p99/device claim. Scratch remains (8192+5)*152 =
1245944 bytes/task. Exact correspondence may shortcut only after full value verification;
an early region exit may retain lower zero, never an incomplete all-target minimum.

Before a BVH, each region is tightened against ONE fixed convex target triangle using
its three enclosed corner-distance upper bounds. For p=sum(lambda_i*p_i), choose
q_i in that target; q=sum(lambda_i*q_i) remains there, so distance(p,q) <= max_i U_i.
Then minimize this maximum across targets. Never maximize per-corner minima from
DIFFERENT targets: the filled-opening counterexample makes that unsafe.
Combine this valid upper with the existing sample+radius/parent bounds; count every
additional primitive query in sliced budgets. Existing all-target lower evidence stays.
Retain independent containment, rounded corner, degenerate, opening and native controls. Exact triangle correspondence
is optional only with full value verification; hashes alone certify nothing.

Independent Fraction oracle: 7200 exact rational interior probes cover arbitrary/sloped,
collapsed/collinear/self targets, binary scaling and large translations. Convex fixed-target
inequality holds; per-corner target switching yields 0 versus opening distance squared 1/4.
Log: /tmp/outshine-review-convex-oracle.log. This supports the decision, not C++ rounding,
budget/state or native runtime acceptance by itself.

## Aktiver Schritt: vollständige Eckwerte und begrenzte Zielarbeit

EvaluatePoint zählt Innenprobe und Eckabfragen einzeln. Zielwechsel setzt das Maximum
zurück; Moves übertragen Cursor/Maximum, Erschöpfung erhält Elternabdeckung.
Seed prüft zunächst das gleichindizierte Zieldreieck als Hint; der reguläre Scan prüft
jeden weiteren Kandidaten. Nullradius und vollständige Gleichheit aller drei Quell-
ecken mit Ecken EINES Zieldreiecks beweisen konvexe Überdeckung und gerichteten Fehler 0.
Kein Hash-/Indexbeweis. Auch degenerierte Regionen gelten; Lower darf dann exakt 0 sein.
Erreicht ein partielles Eck-Maximum die vorhandene obere Schranke, restliche Ecken
überspringen: das vollständige Maximum kann diese Schranke nicht mehr verbessern.
Die Innenprobe jedes übrigen Ziels bleibt für vollständige Lower-Minima erforderlich.
Gleichheit/Hint kosten begrenzte WorkUnits, keine Distanzabfrage. Caps unverändert.
Native flache/geneigte Selbstflächen mit verkehrter Reihenfolge müssen unter Caps nützen;
verschobene und gefüllte Öffnungen widerlegen falsche Korrespondenz. Bestehende Gates.
