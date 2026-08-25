Type: feature
State: open
Area: world
Tags: instrument

**One pilot serves every way of moving, and the currency is CURVATURE**

A walker, a car, a train and an aircraft all answer the same question -- *where am I on this line and
what must I do about it* -- and they answer it with the same number. **The demand is a curvature in
1/m**, and everything that differs between the four is a conversion below it that the base cannot
spell.

| | Converts the demand with | Its tightest turn is | The channel it does not have |
|---|---|---|---|
| **walk** | `omega = v kappa` | `turnRate / v`, unbounded at rest | no vertical, no lock |
| **drive** | `delta = atan(L kappa)` | lesser of `tan(deltaMax)/L` and `grip g / v^2` | no vertical |
| **fly** | `phi = atan(v^2 kappa / g)` | `g tan(bankLimit) / v^2` | -- it is the only one with a vertical |
| **rail** | nothing at all | the rail's | **the whole lateral channel** |

**Rail is the case that proves the shape.** A train consumes the same demand and converts its lateral
half into NOTHING -- what the mode publishes instead is the unbalanced lateral acceleration the
passengers feel, `v^2 kappa - g sin(cant)`, and exceeding the cant deficiency is a refusal of the SPEED
rather than of the path. A base that had a steering angle in it could not have served that.

## What must be true

- [x] **Resection lives once.** `src/pilot/Course.h` answers station, signed offset, height error,
      heading error, curvature, slope and bank from a position and a heading, over a window around the
      last station so the cost is a window and not a line
- [x] **The base demands a curvature** -- `src/pilot/Pilot.h`, pure pursuit over a sighted CHORD, with
      the one declared number a look-ahead TIME. It publishes what it asked beside what it took, and
      says when no point on the line was within reach at all
- [x] **Four modes convert it and derive their own limits** -- `src/pilot/{Walk,Drive,Fly,Rail}.h`
- [ ] **A fifth mode swims**, and the water's own medium decides its limit
- [ ] **The mode a scenario declares reaches the pilot** -- `board:1495`'s `Mind`, so a kind carries
      `uses="drive"` and gets this
- [ ] **A demand becomes forces and never a pose.** Today the modes publish a steering angle, a turn
      rate and a bank; the vehicle that applies them through `src/physics/` is `board:1501`

Proven by `test/unit/pilot/APilotHoldsALineByAimingAtIt.cpp` and
`test/unit/pilot/EveryWayOfMovingConvertsTheSameCurvature.cpp`.

## Comments

**Stanley was tried first and refuted, with the number.** The law `delta = psi + atan(k e / v)` applied
at the FRONT axle cancels its own feedforward on a constant curve: the front axle's heading relative to
the tangent is geometrically `L/R`, which on R=400 with L=2.81 is 0.007025 rad -- and the feedforward
`atan(L kappa)` is 0.0070249 rad. Measured heading error was 0.0070877. The two subtract to nothing and
the car drifts out until the cross-track term rebuilds the steer, giving 0.175 m of deviation on a road
with no defect in it.

**Pure pursuit needs no feedforward term at all**, which is why it replaced it: with both ends of the
chord on a circle, `2 sin(alpha) / chord` is `1/R` identically, at any look-ahead. Nothing to double
count, and the same expression carries the error correction. Off a straight line it reduces to the
published `2 e / d^2` (Coulter, *Implementation of the Pure Pursuit Path Tracking Algorithm*,
CMU-RI-TR-92-01, 1992).

**Aim along a CHORD and not an arc length.** Aiming an arc length ahead is what makes pure pursuit cut
corners; the chord is what makes it exact on a circle.

**The residual on a clothoid is a named term, not noise.** Where curvature ramps at `c` per metre, the
steady-state offset is `c d^3 / 6`: predicted 0.054253 m at a 1 s look-ahead on the 840 m synthetic
road, measured 0.056839 m, and 0.006782 predicted against 0.007341 measured at 0.5 s -- a ratio of 7.74
against the cube's 8. The consistent 5-8 % excess is the next term and is not yet named.

**So the look-ahead time is a real decision and not a constant.** Tracking improves as the CUBE of a
shorter look-ahead and stability leaves with it. A scenario declares it; the engine's default is 1 s.

**Sharpened (review round 19, 2026-08-23):** the fly conversion divides by
`within.GravityMs2` unguarded (src/actor/mind/Fly.cpp:26) and `BankLimitOf` leaves
`cos(BankRad) -> 0` reachable when `LoadFactorLimit <= 1` and `BankLimitRad >= pi/2`
(Fly.cpp:7-14, LoadFactor at :30 -> inf). Drive got its Stand-time refusal in 1705/1706;
Fly and Rail have NO assembly gate yet -- today only the unit fixture feeds them sane
envelopes. The day this feature wires Fly into an assembly, a Stand-style refusal
(gravity > 0, a bank limit the load factor can carry) must precede the first tick, with the
1705-shaped negative proof.
