Type: feature
Area: world
Tags: scope, presence

**Simulated infrastructure is an engine service the client parameterises**

The owner's direction (2026-08-22, generalised from the first cut): living infrastructure --
traffic on the streets, and by the same shape whatever else infrastructure carries -- is
something OUTSHINE does, not something a client builds. The client declares PARAMETERS
(density, mix, a clock curve); the number of cars is a CONSEQUENCE, never a declared count.
No thousand-car demo: a density is a property of the infrastructure simulation, and what the
picture shows follows from it.

## The shape

- the engine owns the infrastructure simulation as a service over the data it already streams:
  OSM streets become the flow network (the corridor machinery reads the same edges), densities
  live as a conserving FIELD per edge (board:1597's field rung -- one fundamental diagram both
  sides of every seam), and individuals materialise where measured, at the presence ladder's
  rungs
- the client's whole contribution is declaration: `<traffic density=... mix=... clock=...>`
  -- in the scenario XML or through the same assembly API, per the parity law; per street
  class, with the population and origin every number carries
- the engine derives everything else: flows from densities, speeds from the corridor plans,
  materialisation from the standing measurements, conservation from the bookkeeping

## The picture that proves it

A city seen from above, the camera moving, the declared density flowing through the streets:
car count MATERIALISED and count REPRESENTED IN THE FIELD published side by side, conservation
residue zero at the seams, determinism (measure twice, same cars), and the frame floor held as
p50/p95/p99. Instancing (1538) is forced by the same picture -- one mesh, real instance counts.

## Open questions for the reference study (running)

Which parameters the client declares vs. derives (density per street class? a demand model?
time-of-day curves?), how shipped engines cut the service/content seam (MassTraffic+ZoneGraph,
REDengine crowds, SUMO's three demand forms, ambient-zone models), and the documented pitfalls
(junction deadlocks, spawn visibility, density calibration).

Depends: 1597
