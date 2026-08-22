Type: feature
Area: core
Tags: perf

A compliant contact is six numbers -- reach, stiffness, damping, travel, stop, limit -- and
it is the whole of what the physics knows about a thing meeting a surface. A wheel, a landing
gear, a foot and a bumper are the same mechanism at different numbers, and which one it is
belongs to the declaration that placed it.

`src/physics/Contact.h` states the shape; `Press()` answers with a reaction that carries both
the load and whether the travel and the limit were passed. The kind is nowhere in the physics
and nowhere in the type name -- JSBSim does the same thing, spelling even a landing gear as
`<contact type="BOGEY">` inside `<ground_reactions>`.

Proven by `test/unit/physics/AContactCarriesItsLoadAndSaysWhenItWouldLetGo.cpp`.

## Comments

The reach is derivable and must not be declared beside the stiffness as a free number: pressed
by its share of the body's weight, a contact leaves its anchor where the anchor says it stands,
so reach = anchorHeight + load/stiffness. On the F31 that is 0.45635 m at the front and
0.44909 m at the rear, and the car sitting level is a CONSEQUENCE of the four declared numbers
rather than a fifth one somebody tuned to make it look right.

The first name for this was `Strut`, with a `WheelRadiusM` field in it. Both were wrong the same
way: a noun from the vehicle appearing in the mechanism.

## Comments

Closed: the body already names its proving test -- src/physics/Contact.h states the six numbers and test/unit/physics/AContactCarriesItsLoadAndSaysWhenItWouldLetGo.cpp proves it; nothing here remained open.
