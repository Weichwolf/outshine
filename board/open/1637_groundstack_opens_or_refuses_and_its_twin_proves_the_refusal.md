Type: bug
Area: ground
Tags: hygiene, tests

**GroundStack opens or refuses, and its twin proves the refusal**

Born this hour (4a320f2e, move 2e slice 1), src/ground/GroundStack.{h,cpp} moved Lay's opening
block verbatim -- and the verbatim move carried four defects into a NEW public seam:

- **`Open` cannot refuse.** GroundStack.cpp declares `[[nodiscard]] bool Open(...)` and
  returns `true` unconditionally (line 28). A failed `RegisterDeclared` (line 16) is
  Claimed to the sink but execution proceeds and the stack reports Opened. A door whose bool
  is a constant is worse than void: every caller (`DriveAssembly.cpp:100
  if (!stack.Open(...)) return false;`) branches on a value that cannot vary. Refusal at the
  registration failure, or the bool dies.
- **Two numbers without origin.** GroundStack.cpp:22-23 `surface.Z = 12; surface.Grid = 64;`
  -- the DEM zoom and the grid width, no derivation, no `[SET]`, no population. They were
  origin-less inside Journey too; a fresh file does not inherit the exemption.
- **The boundary speaks `const std::string&`** (GroundStack.h:25) in the same hour the 1621
  sweep converted the tree's boundaries to string_view. The one concatenation
  (`assetsDir + "/sky"`) does not excuse the parameter type.
- **No twin in the mirror.** test/unit/ground/ carries no GroundStack case; the refusal
  behaviour above and the Close/Opened lifecycle (double-Open, Close-then-Pool) are unproven.
  ALayRefusesASceneItCannotDrive exercises AssembleDrive's refusals BEFORE the stack opens,
  so nothing guards this file.

Demanded: Open refuses on registration failure and on an unopenable store; the two surface
numbers carry their origin; the boundary takes string_view; test/unit/ground gains the twin
that fails on today's constant-true.
