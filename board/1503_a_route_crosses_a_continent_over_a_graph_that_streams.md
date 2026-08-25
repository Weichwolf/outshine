Type: task
State: open
Parent: 1498
Area: world
Tags: scope

**A route crosses a continent over a graph that streams**

`board:1496` gives `navigate` a route over the loaded graph. **Munich to Hamburg is 800 km and the
loaded graph is a few tiles**, so this is a different problem: routing over data that does not fit and
is not there yet.

## What must be true

- [ ] **A long route is planned over a COARSE graph and refined as it streams** -- the motorway network
      first, the local roads when the car is near them. *That is what every router does and the
      mechanism is hierarchical: an overlay graph of high-class ways, and the detail is a local search*
- [ ] **The route is by MODE**, and a car route never uses a footway, which is `highway=*` doing the
      work
- [ ] **A route that cannot close is a named refusal quoting where it stopped**, and the suite counts
      that as a ROUTER finding and not a world one
- [ ] **Planning is not on the frame path**: a request and a completion, the same shape a generated part
      takes
- [ ] **The graph the router walks is the reference line of `board:1499`**, so a route that says *turn
      here* names a place the geometry agrees exists
- [ ] **A route is reproducible from its seed**, both endpoints and the path between them, or the suite
      cannot re-drive a crash

## Comments

**Routing and drawing are separate requirements, and fetching for one by the rules of the other is
what makes a continental route look expensive.** Measured on Munich to Hamburg at zoom 10:

| | square covering both cities | corridor along the line |
|---|---|---|
| tiles | 841 | **166** |
| points | 2 647 016 | 467 257 |
| nodes | 2 154 004 | 384 053 |
| route | 752.421067 km | **752.420705 km** |

**Five times less data and the same answer to 0.4 m over 752 km.** The corridor walks the great circle
in steps of one tile's ground size and fetches a ring of 2 around each station; the square exists only
because a renderer wants everything a camera might see.

The next reduction is hierarchical: plan coarse at a zoom where only the motorway network survives,
then refine only along what the coarse plan chose. That is what GTA's separate path graph is
(`board:1521`) and it is the shipped answer to a graph that does not fit.
