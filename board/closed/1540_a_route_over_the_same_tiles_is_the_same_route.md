Type: bug
Area: world
Tags: instrument
Depends: 1539

**A route over the same tiles is the SAME route**

**Measured, twice, minutes apart, from a warm cache:**

| Run | Route length |
|---|---|
| A | 762.641848 km |
| B | 762.641902 km |

**54 mm apart over 762 km** -- a relative spread of **7.1e-8**. Both runs read the same tiles: *seconds
spent fetching and decoding = 0.031 s*, so nothing was refetched and upstream cannot be the cause.

## The harmless explanations, and why they are ruled out

| Explanation | Why not |
|---|---|
| **upstream OSM changed between the runs** | the fetch took 31 ms. Nothing crossed the network |
| **the clock enters the plan** | route planning is over a static graph; the clock reaches the drive, not the search |
| **the route genuinely differs** -- a different chain of ways | then the difference would be metres or kilometres. 54 mm is a rounding-scale difference along the *same* chain |

**What is left, and it is a hypothesis rather than a finding**: the tile streamer decodes on a worker
pool, so ways arrive in an order that varies between runs and their coordinates accumulate in that
order. Double-precision summation is not associative, and ~1e-7 relative over a few million terms is
exactly the scale this produces. `test/harness/claims/NoEnvironmentVariableDecidesAPicture.cpp` already
excuses `FB_TILEWORKERS` as *"a PACE and not a picture"* while noting that worker-count independence
**is a claim board:1513 owes a test for**. This is the first measurement that bears on it.

## What must be true

- [ ] **Two runs over one tile set produce one route, to the bit** -- and the instrument is the drive
      itself, run twice, differencing the published length
- [ ] **The order ways arrive in does not reach their coordinates** -- accumulate in a declared order,
      or accumulate in a way that does not care
- [ ] **The same holds across worker counts**, which is the debt `board:1513` carries and this test
      would settle

## Comments

**Nearly reported as a null result.** The first attempt compared two logs that were both copies of a
stale one -- `run.sh` takes a suite and not a test path, both invocations refused, and the previous
run's log sat there answering for them. *A partial run leaves the previous run's logs in place, saying
nothing about it*, and the file's own warning caught it only because the mtime was checked.

**A separate number moved for a separate reason, and conflating them would have cost the round**: the
route was **774.852 km** when first measured and is **762.64 km** now. That is a **1.6 %** change, four
orders of magnitude larger than the spread above, and it is upstream data changing between sessions --
not this defect. Two different findings that both look like *the route length moved*.

---

Closed -- the cause was ARRIVAL ORDER, exactly the class the harmless-explanation table
could not name: tiles stream in whatever order the scheduler grants, Lay records them in
that order, and Weave's snap-merge lets the FIRST point of a cluster become the node --
so the surviving coordinate, and with it every edge length, depended on which tile landed
first. 54 mm over 762 km was two different merge winners metres apart, diluted over the
route. The fix: Weave puts the ways into ONE canonical order (lexicographic over their
point content) before any merge -- the graph is a function of the declared ways, never of
the scheduler. Proving test:
test/unit/actor/path/ARouteIsAFunctionOfTheWaysAndNotTheirArrival.cpp -- a 3x3 mesh whose
corners only join by snapping, laid forward and reversed: pre-fix it measures 30 vs 24
nodes and 4449.48 vs 4453.55 m (the class, at metre scale); post-fix both orders weave the
same nodes and plan the same route TO THE BIT (CHECK ==, no tolerance). Negative-controlled
by running the test against the unfixed weave. Drive suite 1/1, gate 130/130.
