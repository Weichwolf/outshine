Type: bug
State: open
Area: render
Tags: bug, perf

**A subject reduces on the ladder, and the car is what needs it**

[MEASURED] `ClusterDag` -- clusters, an edge-collapse simplifier over a cost heap, and `DagSelect`
choosing by projected pixel error -- **is reached by `src/world/TilePool` and by nothing else.**

## THE DEFECT IS NOT THAT THE SUBJECT PATH MISSES OUT. IT IS THAT A NOUN OWNS A MECHANISM.

**The owner's diagnosis, and it is deeper than the symptom.** `CLAUDE.md` states it exactly:

> *An engine is a mechanism and content is data. The engine spells verbs -- place, cull, quantise, draw;
> content spells nouns. **A noun appearing in the mechanism is the defect this decomposition exists to
> prevent.***

**`TilePool` owns the ladder, and `TilePool` is terrain.** So *terrain* -- a noun, one generator's
output among many -- owns *quantise*, which is one of the four verbs that sentence names. **The engine
must not know what terrain IS.** It knows generators and it knows geometry.

**So the repair is not "let subjects use it too".** It is that **the ladder is a property of GEOMETRY**,
and every consumer -- a tile, a car, a tree, a building, a bridge deck, a subject a scenario placed --
reaches the same mechanism because there is only one and it is named for what it does.

| today | what it must be |
|---|---|
| `TilePool` builds and selects a DAG | **geometry carries a DAG**, whoever produced it |
| `Gltf::Subject` has no ladder at all | it has the same one, because it is geometry |
| a generated part has no ladder | the same |
| *terrain* is a special case in the engine | **terrain is a generator's output and the engine cannot tell** |

**And the test of the repair is a grep that comes back empty**: no layer named for a noun may own a
verb. *That is checkable, which is what makes this a defect with a verdict rather than a matter of
taste.*

## Why it stayed invisible

**The render corpus never moves.** A case stands one subject at one declared camera and scores a
picture; nothing in it asks what the same subject costs at 200 m. **The F31 is what made it obvious**:
258 meshes and 30 MB, seen from the driver's seat at full detail and from two hundred metres as a
handful of pixels -- **the same asset spanning the whole ladder in one frame the moment there are two
cars.**

## What must be true

- [ ] **The ladder belongs to GEOMETRY and to no layer named for a noun.** `TilePool` becomes one
      consumer of it and not its owner
- [ ] **A `Gltf::Subject`, a generated part and a tile reach the SAME mechanism**, because there is one
- [ ] **A draw selects a rung by projected error**, which is the currency `CLAUDE.md` already declares
      and which the terrain path already uses
- [ ] **The DAG is built at STAND-UP and not per frame**, the same rule the visibility structure follows
      (`board:1464`)
- [ ] **A subject that cannot be reduced says so** -- an interior of 258 separate meshes may weld into
      very little or into nothing, and *no impostor* and *cannot be reduced further* are two different
      statements (`CLAUDE.md`'s own words about a generator's capability)
- [ ] **The cost is measured**: triangles submitted against distance, in the frame suite, over a
      declared population -- because *a ladder that never saved a triangle is a ladder nobody needs*
- [ ] **The picture is judged by eye at the transition**, because pop is what a ladder is paid in

## Comments

**This is a bug and not a feature, by the board's own question**: does the code claim to do it? The
engine's whole content story is *quantise a budget onto a ladder*, the machinery is written, and the
subject path silently does not use it.

**And the generalisation is the valuable half.** Letting subjects reduce would fix one picture; moving
the ladder out of a layer named `Tile` fixes the class -- *build the thing that unblocks ten things
before the thing that unblocks one*. **Every generator this engine grows -- a bridge deck, a rail
viaduct, a facade, a crowd -- gets a ladder for free the day it is geometry rather than the day somebody
remembers to wire one.**
