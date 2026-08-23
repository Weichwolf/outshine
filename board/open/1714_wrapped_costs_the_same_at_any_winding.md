Type: bug
Area: actor
Tags: frame-path

**Wrapped costs the same at any winding**

`Wrapped` (src/actor/path/Angle.h:10-14) subtracts 2π in a while-loop: O(|angle|/2π) per
call. Its frame-path input is unbounded: `ReferenceLine::Walk` accumulates
`out.HeadingRad = from.HeadingRad + HeadingAlong(...)` (ReferenceLine.cpp:127) and nothing
ever wraps the entry headings, so a corridor's stored heading grows with every turn —
roundabouts and switchbacks add ±2π each, and `Locate` calls
`Wrapped(headingRad - on.HeadingRad)` (Course.cpp:46) EVERY TICK. The tick term grows
linearly with the route's accumulated winding: a bounded-terms violation on the frame path,
invisible on short fixtures and paid on exactly the continental routes 1503 plans.

Closed 1652 kept the while-loop over `std::remainder` deliberately — value identity at the
±π boundary, tests are specification. That ruling stands and is not the question: the demand
is a VALUE-IDENTICAL bounded form — reduce once by `angle - kTurn * std::floor(angle / kTurn
+ 0.5)` (or fmod) to land within one turn, then the existing boundary handling — or wrap the
segment entry headings once at Lay so the frame-path input is already small. Proof: a unit
case feeding Wrapped 1e9 rad returns in O(1) with the same value the loop form gives on the
±π boundary cases 1652 protected.
