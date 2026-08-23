Type: bug
Area: actor
Tags: correctness, residual-of-1719

**The cell walk's cross-row span carries the cosine ratio**

1719 put the column modulus on each row's own latitude — correct — but both walks still use
a span derived as if neighbouring rows had the SAME column width in degrees:

- src/actor/path/Wayfinding.cpp:180-196 — the Weave merge searches centre ± 1 column in the
  rows above and below. A node within SnapM great-circle distance sits within 1 column of
  the centre only in the point's OWN row; in the equatorward neighbour row the offset in
  that row's columns is bounded by `cos(rowLat_eq)/cos(rowLat)` ≈ `1 + tan(lat)*SnapM/R`
  columns, so within `tan(lat)*SnapM/R` of a column boundary the offset is 2 and the merge
  is MISSED — two nodes that should weld stay apart and the network splits. Derivation of
  the window: at 85N with SnapM 50 m it is 9e-5 of boundary placements, at 89.9N 4.5e-3;
  vanishing but nonzero at every latitude, and the failure is a false "network in pieces".
- src/actor/path/Wayfinding.cpp:274-299 — `Within` uses `across = ceil(reach/Snap) + 1` for
  every row; the same ratio scales with `reach/Snap`, so the +1 slack is exhausted once
  `reach/Snap * tan(lat)/R * Snap > 1`. The scan fallback at line 280 masks large reaches
  only when the box exceeds the occupied cell count.

Demanded: the cross-row span is derived from the two rows' own `LonCellDeg` ratio
(`span = ceil(ratio) + 1` per visited row), with the derivation as the number's origin; a
proof arm places two nodes SnapM apart across a row boundary at the adversarial fraction
and shows the weld.
