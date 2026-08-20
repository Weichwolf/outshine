Type: bug
Area: render
Tags: bug, perf

**A subject reduces on the ladder, and the car is what needs it**

[MEASURED] `ClusterDag` -- clusters, an edge-collapse simplifier over a cost heap, and `DagSelect`
choosing by projected pixel error -- **is reached by `src/world/TilePool` and by nothing else.** So the
LOD ladder `CLAUDE.md` puts at the centre of this engine exists **for terrain and for nothing else**:
every glTF subject draws at full detail, always, at every distance.

**That has been invisible because the render corpus never moves.** A case stands one subject at one
declared camera and scores a picture; nothing in it asks what the same subject costs at 200 m. **The
F31 is the first subject that makes it obvious**: 258 meshes and 30 MB, seen from the driver's seat at
full detail and from two hundred metres as a handful of pixels -- **the same asset spanning the whole
ladder in one frame the moment there are two cars.**

## What must be true

- [ ] **A `Gltf::Subject` builds a cluster DAG like a tile does**, and the mechanism is the one already
      here rather than a second one
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
