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

## Closed 2026-08-25 — overtaken, and by the sharper cut

This item asked for a short urban route to REPLACE the country crossing as the regression gate.
The owner cut deeper the same hour: **`apps/driver` has no tests at all.** Everything the driver
uses is library, and the library's unit tests cover it — a case that fetches a country, drives
it and then asserts is an experiment wearing a unit test's clothes.

So both cases are gone, all 2080 lines of `apps/driver/test/`, and what replaces them is the
product itself: `test/run.sh --drive` builds the driver, drives a declared route and leaves ten
stills evenly along it, which the hourly architect judges (board:1863, board:1865).

The item's own measurement stands and was worth making: Munich–Hamburg exercised the widest
radii and the fewest class changes per kilometre in the class table, held the nest lock for
minutes, and its defect showed as a rate rather than as a corner. A short urban route is the
better subject — and it is now a `--from`/`--to` on the driver, not a case.

Proving test: none of its own; the case it asked for was written (`434ed886`) and deleted with
the directory (`38641b13`). What proves the replacement is `test/run.sh --drive` leaving ten
stills, and board:1865 holding the ledger they are judged against.
