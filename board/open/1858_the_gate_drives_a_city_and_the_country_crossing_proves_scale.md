Type: task
Parent: 1795
Area: apps
Tags: driver, gate, cost

# The regression gate drives a city and the country crossing proves scale

`apps/driver/test/APlannerFindsTheRoadFromMunichToHamburg` fetches, weaves, routes, fits and
plans 775 km. It holds the nest lock while it does, and the owner's observation is the measured
one: **the geometry it exercises is the easiest in the class table.**

| | motorway route | inner-city route |
|---|---|---|
| radii | the widest the classes allow | the tightest |
| corners per km | a handful | tens |
| class changes | rare | every few hundred metres |
| corners sharing a straight | almost never | routinely |
| junctions, crossings | sparse | dense |

A reconstruction defect hides in the first column and shows in the second. `board:1795`'s own
measurement says so: 947 of 2204 corners under their class minimum on Munich--Hamburg, a rate a
systematic factor produced, not the sharp turns -- only 24 of 2480 legs turn past a right angle
on the whole 775 km.

## What will be true

- [ ] `apps/driver/test/AShortUrbanRouteCarriesEveryGeometryTheLongOneDoes` -- Marienplatz to
      Nymphenburg, about 20 km inside Munich -- is the case the queue runs every round, and it
      asserts that what it carries IS urban geometry (corners per kilometre, tight radii, class
      changes) rather than assuming it.
- [ ] `APlannerFindsTheRoadFromMunichToHamburg` keeps every claim it makes and becomes the
      SCALE proof, run by name: two million stations, 500 m of elevation, an hour of fetch. It
      is what says the reconstruction holds at country size, and that question is not an hourly
      one.
- [ ] Both are measured against each other once, so the report says what the long route proves
      that the short one cannot -- otherwise the long one is cost without a claim.

## Comments

- 2026-08-25 -- filed on the owner's observation mid-round: *"warum so eine lange strecke und
  keine kurze representative? mit 20 km im urbanen bereich ist doch alles abgedeckt."* The
  answer is that 20 km urban covers the GEOMETRY completely and the SCALE not at all, so the
  two cases split by what they prove rather than one replacing the other.
