Type: bug
Area: sim
Tags: origin

**The drive tick reads the drag area the rig derived**

src/sim/Rigging.cpp:126 derives the one drag area at stand-up:
`Envelope.DragArea = declared.DragCoefficient * declared.FrontalM2`. src/sim/DriveTick.cpp:46
derives it AGAIN from the car (`car.DragCoefficient * car.FrontalM2`) and feeds
Physics::Resist with it (DriveTick.cpp:141 pairs this second origin with
`stood.Envelope.AirDensity`, which IS read from the envelope). One derived number, two
spellings: a rig that ever adjusts its drag area (damage, trailer, aero mode) drives with the
plan's drag and resists with the declaration's.

Demanded: DriveTick reads `stood.Envelope.DragArea`; the derivation stands once in Stand. The
DriveTick twin pins it: a stood rig whose envelope drag area is perturbed must change the
tick's resistance.

---

Closed (review 2026-08-22, night round): DriveTick.cpp:140 is the ONE read --
`Physics::Resist(wrench, body, stood.Envelope.DragArea, stood.Envelope.AirDensity)` -- and no
second derivation from the car remains in the file (grep DragCoefficient over src/sim/DriveTick.cpp:
empty). The derivation stands once at Stand (Rigging.cpp), pinned by
AVehicleDeclarationStandsUpAsARig.cpp:112 (Envelope.DragArea == Cd * FrontalM2) and ridden by
ADriveTickHoldsTheCarToTheDeclaredWorld per task 1632's closing. Task 1632 closed.
