# A generator includes what it uses

State: withdrawn

Measured 2026-09-04, after board:2110 cut the generators by subject:

```
  Yield   used in 10 files under the subject areas, included by NONE of them
  Claim    2                                        included by NONE
  Cover    3                                        included by NONE
  Rank     2                                        included by NONE
```

They arrive through `base/Making.h`, which includes `Ground.h` and `Yield.h`. So a subject area
uses a type it never asked for, and the day `Making.h` stops needing `Yield.h` -- a change that
looks local and harmless -- ten files in five areas stop compiling for a reason none of them
names.

**THIS EXACT FAULT WAS PAID FOR TWICE IN ONE DAY.** `Physics::Rigid` reached `EngineHeld.h` only
through `DriveAssembly.h`, and deleting the driver broke a header that never mentioned the driver.
`Ribbon.cpp` used `Astride` and `StandAt` while its only route to them was an include I read as
dead and removed. Both were found by the compiler, at link time, after the fact -- and both were
one line of missing include away from being impossible.

## Why it is not just tidiness

The cut in board:2110 claims each area is independent, and the measurement it offered was
**0 cross-area includes**. That number is true and it is not the whole picture: an area that
reaches a shared type through another shared type has a dependency the include graph does not
show. The claim "these areas are independent" is only as good as what the includes actually say,
and right now they under-report.

## What Unreal does, what RAGE does

Unreal enforces include-what-you-use in its build system (IWYU is a named UBT mode, and
`Build.cs` failing on a missing dependency is the same idea one tier up). RAGE compiles each
project against a declared include set for the same reason. **They agree**, so this is closed:
the file that uses a type names the header.

## What will be true

- every file under `src/generators/<area>/` includes the header of every type it names
- `misc-include-cleaner` reports 0 in the generator tier
- removing an include from a shared header cannot break a file that does not mention it

## What will show I was wrong

`clang-tidy --checks=misc-include-cleaner` over `src/generators/`. Today it is silent about these
ten files, because the check reports what a file uses without including only for the STANDARD
library unless the tree's own headers are mapped in -- which is the second half of this item: the
guard has to be able to SEE the fault before the repair can be trusted.
