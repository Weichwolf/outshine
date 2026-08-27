Type: feature
State: open
Parent: 1597
Area: world
Tags: scope, presence
Supersedes: 1521

# Simulated infrastructure is an engine service the client parameterises

**Benchmark** — Unreal: procedural content is a plugin (PCG) the level parameterises, outside the engine module. RAGE: none. **Taking Unreal** — infrastructure generation is a library the scenario selects from, never an engine verb.

Living infrastructure — traffic first — is something outshine DOES, not something a client
builds. The client declares PARAMETERS (density, mix, a clock curve, per street class, each
number with its origin); the number of cars is a CONSEQUENCE, never a declared count.

The shape: OSM streets are the flow network the corridor machinery already reads; densities are
a conserving FIELD per edge (board:1597's field rung, one fundamental diagram both sides of
every seam); individuals materialise where they are measured.

The network has to carry what dense traffic needs, and every field says where it came from —
GTA V's `.ynd` is the CHECKLIST of what a shipped engine found it needed, and not one of its
values transfers because all of them are authored: lanes forward/backward (`lanes:forward`,
else `lanes` split by `oneway`, else the class default), link length (derived once and STORED —
a route query cannot afford a sqrt per edge), lane offset from the reference line, junction
(ways sharing a node), tunnel, slip road (`*_link` and the turn the geometry makes), width and
weight limits, surface, dead-endness (distance in links to the nearest cycle) and DENSITY,
which must be derived and has no answer yet.

## What will be true

- [ ] `<traffic density=... mix=... clock=...>` in the scenario or the same call in C++, and
      nothing else from the client.
- [ ] Flows come from densities, speeds from the corridor plans, materialisation from the
      standing measurements, conservation from the bookkeeping — all published.
- [ ] The picture that proves it: traffic on both carriageways from the driver's seat, at a
      density the declaration set, with the count as a consequence.
