Type: bug
State: active
Area: generators
Tags: measured, flora
Depends: nothing

# Tree growth processes every spawned shoot without invalidating iteration

Native birch render in `build/tree-native/birch.png` is only a trunk. The new native
geometry/material oracle fails because no leaf surface exists. `TreeGrower::GrowOnce`
uses a range-for over Queue_, while SpawnShoot appends to that vector. The loop caches
its original end and may hold iterators invalidated by reallocation. Spawned branches
are not reliably processed. This is a growth defect, independent of 2111's missing renderer handoff.

Use an indexed work queue, copy each tip before appending children, process new entries
within the existing node/shoot bounds. Do not raise limits or supply a substitute crown.
Test a branching plant, deterministic repeat, nonbranching/totholz case and capacity bounds;
restore the range-for as negative control. Inspect native geometry PNGs. Existing tree
materials/native adapter in 2171/2111 expose this defect and remain separate responsibilities.

Benchmark: bounded growing work queue, no undocumented Unreal/RAGE internal algorithm.
