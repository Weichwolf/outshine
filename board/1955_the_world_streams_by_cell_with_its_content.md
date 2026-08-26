Type: feature
State: open
Parent: 1953
Area: world

# The world streams by cell, with its content

A cell brings its ground, its structures and its actors in and out TOGETHER, and nothing in the
tree holds the whole of anything.

**Both benchmarks are built around this rather than having added it.** Unreal's World Partition
replaced the hand-placed level and made the cell grid the world's own shape; RAGE streams map
nodes and an `fwEntity` belongs to the node it stands in. Neither has a code path that assumes the
world is resident, which is why neither has one to repair.

CURRENT streams ground tiles and nothing else: structures and actors are stood up whole. That is
the assumption both engines do not make, and it is cheaper to remove before the actor chain is
built on top of it than after.

- [ ] a non-resident cell is represented by a COARSER proxy and never by nothing (Unreal HLOD, RAGE LOD hierarchy)
- [ ] a cell carries ground, structures and actors and they arrive and leave together
- [ ] no resident term scales with the world's extent, proven by a case over two extents
- [ ] `apps/driver` drives out of the declared extent and the picture does not break
