Type: bug
Area: sim

**The tick's signature carries no noun it ignores**

`DriveTick(const Corridor &, const Rigged &, const Vehicle &car, DriveState &, double, const Taken *)`
— `car` is never read in the body (src/sim/DriveTick.cpp:26-194; the only occurrence of `car`
is the parameter itself). Every number the tick uses arrives through `Rigged` and
`DriveState` (`drive.CarWidthM` carries the width). Six call sites
(test/unit/sim/ADriveTickHoldsTheCarToTheDeclaredWorld.cpp:119, tools/driver/*: four files)
haul `drive.Car` to a parameter nothing consumes.

`-Wall -Werror` does not catch it (unused-parameter is -Wextra), so the signature lies
silently. Demanded: the parameter is deleted at DriveTick.h:67 and all call sites; if a
future tick needs the declaration, it takes it the day it reads it.

---

Closed -- the parameter is deleted from DriveTick.h/.cpp and all six call sites; every number
the tick uses arrives through Rigged and DriveState, as the body already said. Proven by the
build itself: unit/sim green and `test/run.sh --audit-link` closes over every declared suite
(the driver tools compile and link against the honest signature).
