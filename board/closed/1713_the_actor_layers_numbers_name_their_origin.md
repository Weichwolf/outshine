Type: issue
Area: actor
Tags: hygiene, numbers

**The actor path and mind layers' numbers name their origin**

1706 (closed this round) put origins on every sim-layer number; the layer below it went
unswept in the same round. Naked numbers standing today:

- `src/actor/mind/Course.h:8` — `kChordSteps = 12`: no [SET]/derived tag.
- `src/actor/mind/Course.cpp:71` — `1.0e-6` chord convergence; `:86` — `1.0e-3` out-of-reach
  threshold. Neither says what population it was set against.
- `src/actor/path/ReferenceLine.h:12-16` — `kMaxCorridorSegments/Knots = 262144`,
  `kResectionCoarseM = 1.0`, `kResectionRefinements = 24`, `kTangentTolerance = 1.0e-9`:
  five bounds, zero origins. kResectionCoarseM is the per-tick coarse-scan step — its origin
  is a frame-budget claim and must say so.
- `src/actor/path/Fit.cpp:73,74,125,229,240` — `96.0` appears FIVE times inside
  `1.0 + swing*swing/96.0`, and the tangent formula
  `radius * (shiftShare * tan(half) + 0.25 * swing)` four times (136, 244, 249, and inside
  CornerRadiusM:76): the derivation (clothoid shift, presumably 1528's measurement) is
  written nowhere and the copies will drift — one named helper, one origin.
- `src/actor/path/Fit.cpp:153` — `pass < 24` while the error text says "twenty-four measured
  corrections": measured where, on what corpus?
- `src/actor/path/Wayfinding.h:14` — `kStartReachM = 250.0`; `Wayfinding.cpp:19` — the
  `1.0e-6` pole clamp on cos(lat).
- `src/actor/path/SpeedProfile.cpp:64` (`4.0 * CorneringNPerRad ...`), `:71` (`6.0 *
  HoldWithinM ...`): the cbrt laws carry factors whose derivation lives only in 1522's board
  body, not beside the code that computes them.

Same bar as 1706: derived · measured · [SET], with unit and population, beside each number.

---

Closed -- every listed number carries its origin beside its declaration: Course's chord
budget and both thresholds; ReferenceLine's five bounds (kResectionRefinements is now a
DERIVATION: 24 halvings of the metre bracket = 60 nm; the segment bound cites the route-leg
bound it mirrors); kStartReachM and the pole clamp; the cbrt laws cite 1522's derivations
with the 4 and the 3! named. The five 96.0 copies and four tangent formulas are ONE pair of
named helpers (ShiftShare/TangentShare) carrying the clothoid-shift derivation s = R tau^2/24
with tau = swing/2 -- exactly swing^2/96 -- so the copies cannot drift. The fit's
"twenty-four measured corrections" refusal text stopped claiming a measurement nobody
recorded: it names the budget it is. Value identity held by the existing fit and speed-plan
proofs (unit/actor/path 10/10, gate green).
