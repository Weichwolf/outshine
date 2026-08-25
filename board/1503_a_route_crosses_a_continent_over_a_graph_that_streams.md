Type: task
State: open
Parent: 1498
Area: world
Tags: scope

# A route crosses a continent over a graph that streams

Munich to Hamburg is 800 km and the loaded graph is a few tiles: routing over data that does not
fit and is not there yet. Fetching for a ROUTE by the rules of DRAWING is what makes it look
expensive — measured at zoom 10, a square covering both cities is 841 tiles and 2 647 016
points; the corridor along the line is **166 tiles and 467 257 points**.

## What will be true

- [ ] A long route is planned over a COARSE graph and refined as it streams — the motorway
      network first, the local roads when the car is near them: an overlay graph of high-class
      ways with a local search for the detail.
- [ ] The route is by MODE, and a car route never uses a footway.
- [ ] Planning is off the frame path: a request and a completion, the shape a generated part
      takes.
- [ ] The graph the router walks is the corridor's own reference line, so a route that says
      *turn here* names a place the geometry agrees exists.
- [ ] A route that cannot close is a named refusal quoting where it stopped, counted as a ROUTER
      finding and not a world one; a route is reproducible from its seed.
