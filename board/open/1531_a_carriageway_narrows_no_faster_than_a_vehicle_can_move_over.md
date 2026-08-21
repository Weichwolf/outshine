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

## Comments -- the taper now leads, and what is left is not dynamic

**The lane centre is now pulled in AHEAD of a narrowing**, by the same backward sweep the speed profile
uses: what fits at station i also limits how far out the car may be at station i-1. It pulls in by up
to **2.478 m** and touches **109 stations**. And the lane centre keeps the tracking budget clear of the
edge -- 0.7195 m -- instead of sitting on it, which was worth 12 km of route.

Two instrument defects fixed on the way, both the same shape as every other: an index computed on the
96.53 m station grid and used on the 2 m grid, so the car read the road's width from 110 m away while
it was at km 5.35; and `Airborne` counted a wheel in the air and a wheel off the road as one thing, so
a normal suspension lift read as leaving the carriageway.

**And then the useful null result.** Halving the tracking budget given to each speed term dropped the
mean from 175.37 to 157.59 km/h -- and the car reached **126.408 km against 126.407 km**, with the same
0.842 m of error at the same station. *A failure that does not move when the speed drops by 18 km/h is
not a cornering error.* Whatever puts the car 0.842 m out of its lane at km 126.4 is geometric, and
the next thing to look at is what the reference line itself does there.
