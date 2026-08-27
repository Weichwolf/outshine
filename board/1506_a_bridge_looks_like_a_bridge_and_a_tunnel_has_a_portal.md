Type: task
State: open
Parent: 1813
Area: generators
Tags: scope, picture

# A bridge looks like a bridge, and a tunnel has a portal

**Benchmark** — Unreal: bridges and tunnels are authored meshes placed on a spline. RAGE: the same, in the map. **Neither reconstructs** — so the reconstruction is ours, and what it owes is the four plausibilities: geometric, physical, static, architectural.

A raised ribbon of tarmac with nothing under it is geometrically correct and looks like a bug.
The structures must WORK and LOOK RIGHT: the drive suite decides the first, the eye decides the
second.

Every one of them is the same mechanism — a cross-section profile swept along the reference
line, produced by a generator taking `(kind, params, seed, budget)` like any other, so a bridge
reduces on the LOD ladder like a tree does and a distant viaduct is a ribbon with piers rather
than nothing: deck with edge beams, parapet, piers at declared intervals down to the ground,
abutment where the deck meets the embankment, portal cut into the slope, bore the car drives on,
and a wall where no slope fits.

## What will be true

- [ ] Each structure is a generator kind with a capability, on the same ladder as everything else.
- [ ] The static plausibility is a number: span against depth, pier spacing against load, and a
      clearance under the deck that a vehicle of the class below could pass.
- [ ] A still from the seat shows a bridge that reads as a bridge and a portal that reads as a
      portal, and the owner says so.
