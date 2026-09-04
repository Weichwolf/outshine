# Refine names one thing

State: withdrawn

Four unrelated things answer to `Refine` in this tree:

| where | what it actually is |
|---|---|
| `base/spatial/Refine.h::Divide` | the 1-to-4 red-green split of ONE triangle at marked edge midpoints |
| `generators/terrain/GroundYield.cpp::Refine` | the adaptive tessellation LOOP over a whole mesh |
| `generators/building/RoofSurface.cpp::Refine` | subdividing a dome N times, unconditionally |
| `engine/Laying.cpp::RefineChords` | inserting stations along a road curve |

They share a word and change for entirely different reasons: a dome is subdivided because a dome is
round, a road gains stations because a curve needs sampling, and the ground is refined because
something stands on it. **This is the trap CLAUDE.md records** -- `[12 + axis]` stood eight times
and was four meanings -- and the guard is the same: rename per type and let the compiler be the
oracle, never one regex over the word.

The header one is the only one that is a PRIMITIVE, and it already has the better name (`Divide`)
inside a file named after the loop that uses it.

## What will be true

Each of the four says what it does, and none of them says `Refine` unless it is the only one left.
The file `base/spatial/Refine.h` is named for its content rather than for a caller.

## What will show I was wrong

`grep -rn '\bRefine' src include` names one thing, or names several that a reader cannot confuse.
Today: four, in four tiers.
