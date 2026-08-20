Type: task
Parent: 1480
Area: world
Tags: scope

**outshine ships the fast intelligences, and navigation reads the world it already loaded**

**A reflex mind is the ENGINE'S OWN**, so the engine must have some. A scenario names one with
`uses="navigate"`, and the set of names is a registry the engine publishes -- the same shape the
generators and the providers take.

**Navigation is nearly free here and that is the point.** The world is loaded from OSM, so the road
graph, the footway graph and their tags are ALREADY IN THE TREE: `highway=footway` is where a person
walks, `highway=residential` is where a car drives, and a routing graph over them costs the streamer's
own data rather than an authored navmesh. *An engine that generates its world from the real one gets
its navigation from the same bytes.* **That is the thing this decomposition buys that an authored-world
engine cannot have**, and it is worth saying plainly.

## What the engine ships, and each is a verb

| name | what it answers | what it costs |
|---|---|---|
| `navigate` | a route from here to there over the OSM graph, by mode | a search at request, a follow per tick |
| `steer` | the next velocity, given a route and what is nearby -- arrive, seek, separate | microseconds, every frame |
| `avoid` | a velocity that misses what is moving, over the next second or so | microseconds, every frame |
| `follow` | keep station on another instance at a declared distance | microseconds |
| `look` | where the head and the eyes point, which is what makes a body read as alive | microseconds |

**Every one is a mechanism and none is a noun.** There is no `guard`, no `patrol`, no `flee` -- those are
scripts over `navigate` and `steer`, which is where a game's behaviour belongs.

## What must be true

- [ ] **The set of reflexes is a REGISTRY the engine publishes**, and `uses` naming one outside it is a
  refusal listing what there is
- [ ] **`navigate` reads the streamer's own graph** and never a second copy, or the route and the world
  drift the first time a tile reloads
- [ ] **A route is by MODE** -- on foot, by car, by boat -- and a mode with no edge is a named refusal
  rather than a route through a wall
- [ ] **A search is bounded and answers what it achieved**: a route, a partial route with the distance
  it reached, or a refusal -- **both directions**, like every capability here
- [ ] **A search is NOT on the frame path.** It is a request and a completion, the same shape as a
  generated part; the FOLLOW is what runs per tick and it is O(1)
- [ ] **Steering takes nothing from the allocator**, because it is per actor per frame
- [ ] **The cost is measured at a declared population**: N actors navigating and steering, p50/p95/p99,
  in the scenario suite -- because *how many actors* is the number a world of this size lives or dies by

## Comments

**The three tiers came from the owner and they are the shipped shape.** Looked up rather than recalled:
the three-layer architecture is *a reactive feedback control mechanism, a reactive plan execution
mechanism, and a mechanism for performing time-consuming deliberative computations* -- and game AI has
run reactive-plus-deliberative since F.E.A.R. popularised GOAP in 2005. **The tiers are a LADDER and not
a choice**, exactly as the LOD rungs are: an actor's body is steered every frame, its plan is stepped a
few times a second, and its intent is decided every couple of minutes.
