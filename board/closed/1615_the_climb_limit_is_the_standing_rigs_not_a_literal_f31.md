Type: bug
Area: sim

**The climb limit is computed from the standing rig, not from F31 literals**

src/sim/CorridorLay.cpp:500-501:

    const double driveN = 400.0 * 3.08 / 0.333;
    const double climbLimit = driveN / (1610.0 * 9.80665);

The F31's peak torque, final drive, tyre radius and mass are LITERALS inside the engine's
corridor mechanism -- while the deduced truth sits in the parameter list: `stood.Envelope.DriveN`
and `stood.Envelope.MassKg` are exactly these numbers, derived from the declaration by
`Sim::Stand`. Any other vehicle's grade refusal is judged with the F31's drivetrain and weight.
The adjacent claim texts hardcode the same population -- "23.4 %", "3699 N against 15789 N",
"8022 heights" (CorridorLay.cpp:502-510) -- route-1/vehicle-1 numbers asserted as if they were
mechanism. This is the same disease board:1581's comment recorded for Lay's Munich constants;
the extraction moved it verbatim into the new home.

Demanded: `climbLimit = stood.Envelope.DriveN / (stood.Envelope.MassKg * g)` with g from the
declaration path (board:1611), and claim texts that speak the computed numbers, not a memory of
one vehicle on one route. (The g literal itself is board:1611's audit; this item is the vehicle
constants.)

---

**Closed (review 2026-08-22, evening).** Proof at HEAD (e5a1122): src/sim/CorridorLay.cpp:506
computes `climbLimit = stood.Envelope.DriveN / (stood.Envelope.MassKg * stood.Envelope.GravityMs2)`
— every term from the standing rig and the declared world; the F31 literals (400.0, 3.08,
0.333, 1610.0, 9.80665) and the hardcoded claim populations (23.4 %, 3699/15789) are gone from
the file. Task 1616 closed with gate 119/119, Munich 40/0, Kyoto 35/0.
