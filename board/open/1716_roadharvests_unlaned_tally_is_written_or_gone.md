Type: bug
Area: ground
Tags: hygiene, telemetry

**RoadHarvest's Unlaned tally is written or gone**

`src/ground/RoadHarvest.cpp:47` repeats verbatim the condition already `continue`d at
line 34 (`rule->Lanes <= 0`): the second branch is unreachable, so `out.Unlaned` and
`out.WithoutLanes` (lines 48-51) read 0/"" forever — a statistic that always answers the
same thing is telemetry that lies, the 1703 class (a product field nobody writes).

Same function, second defect: the kind dedup at lines 36, 43, 49 uses
`std::string::find(kind) == npos` — SUBSTRING matching, so a kind `way` is suppressed once
`motorway ` was recorded and the diagnostic under-reports the classes it exists to name.

Either Unlaned has a reachable writer with its own condition and a test that drives it, or
the field goes; the dedup matches whole tokens. `Reap` also swallows `Type != 2` features
per-way and silently returns an empty Reaped when the streets layer is absent
(line 12) — the counts exist, but the absent-layer arm publishes nothing at all where a
number would carry information.
