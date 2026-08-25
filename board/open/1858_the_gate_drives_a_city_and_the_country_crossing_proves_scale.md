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

- 2026-08-25, SHARPENED by the hourly review -- the case this item asks for was WRITTEN and
  then DELETED without a word. `434ed886` added
  `apps/driver/test/AShortUrbanRouteCarriesEveryGeometryTheLongOneDoes.cpp`, 155 lines,
  Marienplatz to Nymphenburg at zoom 14. `38641b13`, six minutes later, removed all 155 lines
  in a commit whose subject and body speak only of `main.cpp` and board:1859 and never mention
  the deletion. `find . -iname '*Urban*'` at HEAD finds nothing under `apps/`.

  A proof is not scratch. If the case was wrong, the commit that removes it owes the
  measurement that says so; if it was merely unfinished, it belonged on a branch or in this
  item's body, not in a commit that claims to be about an entry point. The first checkbox
  above is therefore still open and the work that would have closed it exists only in
  `git show 434ed886:apps/driver/test/AShortUrbanRouteCarriesEveryGeometryTheLongOneDoes.cpp`.

  Note also, for whoever restores it: that file carried a six-line `//` comment block at its
  head (lines 27-32 of the deleted version). `test/` is the exception the comment rule allows,
  so the block may stand -- but it narrates the ITEM, and this item is where it belongs.
