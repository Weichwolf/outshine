Type: bug
Area: generators
Tags: instrument

**A carriageway narrows no faster than a vehicle in it can move over**

Once the car drives in ITS LANE rather than down the centreline (`board:1529`), a width change becomes
a lateral manoeuvre. On the Munich to Hamburg corridor the car reaches **km 5.349 of 774.872** and then
has two wheels off the carriageway while sitting **0.023 m from its own lane centre** -- it is exactly
where it should be, and the road is not.

**The numbers.** A 12 m carriageway with two lanes puts the lane centre 3.0 m from the reference line.
Where that meets a narrower road the lane centre must move, and how fast it may move is derived: the
shift over one look-ahead must stay inside the tracking budget, `budget / (T v)`, which on this route
is **11.13 mm per metre**. A 2.1 m shift therefore needs **189 m** of road. The narrowing arrives
sooner.

Before lanes existed the car drove **771.475 km** down the centreline, where a width change costs
nothing. That number was bought by driving into the oncoming lane, so it was never real.

## What must be true

- [ ] **The taper LEADS the narrowing.** A road that goes from four lanes to two tapers before the
      lanes end, not at them -- the forward-backward sweep over the lane centre must be given the
      narrowing as a hard constraint at its far end and allowed to start early
- [ ] **A narrowing that no vehicle could follow is a NAMED REFUSAL of the ROUTE**, not a crash. The
      planner should not choose a way whose entry demands more lateral movement than the approach
      allows -- which is a turn restriction of the same family as `board:1524`'s corner test
- [ ] **The width comes from the way and not from the station grid.** Today it is sampled onto the
      DEM's 96.53 m posts and then onto a 2 m grid, so a width change lands up to 96 m from where the
      way actually changes
- [ ] **Where two ways of different width meet, the junction blends them** -- `board:1499` already
      carries junction blending as an open line, and this is the same defect seen from the side

## Comments

**This is a finding the centreline hid.** Every number measured before lanes -- 771 km, a 2.13 m worst
deviation -- was measured on a car that used the whole carriageway to hold its line. The lane
constraint is what made the road's own geometry start mattering, and the first thing it found is that
the width in the data changes as a STEP where a real road has a taper.
