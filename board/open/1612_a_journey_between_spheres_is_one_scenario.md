Type: feature
Parent: 1611
Area: core
Tags: scope

**A journey between spheres is one scenario**

The owner's proof-of-generality (2026-08-22): define a constant-1g-thrust rocket, board it on
Earth, fly to the Moon or Mars in minutes-to-hours, disembark, and move under LOCAL conditions.
Derivation check, 1g brachistochrone Earth->Moon: t = 2 * sqrt(d/a) = 2 * sqrt(3.844e8 m /
9.81 m/s^2) ~ 3.5 h -- the owner's "a few hours" is the physics, exactly.

## What the architecture already carries

- the rocket is a BODY with one actuator (thrust, declared 1g) -- the actor chain as ruled;
  driving badly on the Moon and jumping higher are emergent, never coded
- boarding and disembarking are POSSESSION RELINKS -- the mind releases the walker's seam and
  takes the rocket's, the same DrivenBy relation the player and the autopilot share; no new API
- the sky over each sphere is that sphere's declaration; providers are per-sphere (1611)
- time scale is a clock declaration; presence (1597) lets the departure sphere dissolve to
  field while the arrival sphere materialises

## What it demands beyond 1611

- [ ] the scenario declares a SYSTEM of spheres, each with radius, gravity, providers and sky
      -- one scenario, several worlds, and the library still knows none of them by name
- [ ] gravity becomes a queryable field of the declared system (dominant-sphere rule as KSP
      ships it, per the 1597 study -- patched, not n-body, until a measurement demands more)
- [ ] free flight is a physics regime: a body with no surface contact integrates thrust +
      gravity; the rails rung carries the closed-form arc (the study's on-rails, verbatim use)
- [ ] the frame follows the actor across spheres (floating origin across bodies -- the
      Krakensbane lesson: frame switches and contact constraints never compose in the same
      tick)
- [ ] the proof scenario: board on Earth, 1g out and 1g in, disembark on the Moon, jump --
      every number from the declarations, the jump height ratio exactly g_earth/g_moon
