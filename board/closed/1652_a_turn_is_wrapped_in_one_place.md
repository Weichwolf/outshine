Type: task
Area: actor
Tags: hygiene

**A turn is wrapped in one place**

Round 2's unfiled note, worked and closed in one sitting: kTurn and Wrapped(angleRad) stood
verbatim three times -- src/actor/path/Fit.cpp, src/actor/mind/Course.cpp,
src/actor/mind/Pilot.cpp. One header now holds them (src/actor/path/Angle.h,
outshine::kTurn = 2 pi from std::numbers, outshine::Wrapped), the three copies are deleted,
call sites unchanged by enclosing-namespace lookup. The while-loop form is KEPT deliberately:
std::remainder differs at the +/- pi boundary and tests are specification -- value identity
beats fashion. Proving state: unit/actor/path, unit/actor/mind, unit/sim 15/15.
