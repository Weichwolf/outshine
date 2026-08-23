Type: task
Parent: 1610
Area: core
Tags: guards, recurrence

# Every guard in the tree spells its folder

1643 established the rule and closed on src/actor alone. The rest of the tree never
heard it: 196 headers across 15 directories still open with bare guards —
src/generators/Body.h:1 `BODY_H`, src/generators/Forest.h:1 `FOREST_H`,
src/generators/draw/TreeGrower.h:1 `TREEGROWER_H`, src/ground/tiles/TileMath.h:1
`TILEMATH_H`, and the count by directory: core 38, render/stages 24, generators 23,
ground 18, generators/draw 18, data 15, clients 15, gltf 11, scenario 10, core/io 6,
ui 5, render 5, tiles 4, render/plan 2, render/draw 2. No two collide TODAY (verified by
sort|uniq -d), but `BODY_H` and `FOREST_H` are one innocently-named header away from a
silent empty translation unit — the failure mode the rule exists to refuse at spelling.

What will be true: every guard in src/ spells OUTSHINE_<FOLDERS>_<NAME>_H, and a claims
test walks src/ and refuses a guard that does not spell its folder — the actor sweep
proved the mechanics; this closes when the walk is tree-wide and enforced, not repeated
by hand per layer.
