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

---

**Learned from the reference study (2026-08-22; ZoneGraph/MassTraffic, REDengine, Cities
Skylines, SUMO, Ubisoft Meta AI/Census, MSFS).** No shipped system exposes density zones plus
time curves as a documented API -- the poles are MSFS (global scalar x world-data derivation),
MassTraffic (type mix + tag filters against a baked lane net), SUMO's calibrator (a DECLARED
MEASURE with vehicle existence as the control variable), and Origins' Meta AI (three simulation
tiers over virtual bookkeeping). Skylines and Cyberpunk are the counter-models: full emergence
admits no parameter, and the monolith admits no API.

The minimal reference-true form falls onto this architecture almost seam for seam:

| piece | reference | sits here |
|---|---|---|
| lane net derived, never authored | ZoneGraph bake; MSFS from road type/width | OsmField -> RoadHarvest -> Wayfinding -> Carriageway, at runtime instead of a Houdini bake |
| demand declaration | SUMO flow (period="exp(lambda)") + turn ratios | density per OSM highway class, type mix, seed, optional day-curve multiplier -- ambient traffic is goal-less, so no OD matrix and no global pathfinding (the Skylines trap) |
| density as a REGULATED measure | SUMO calibrator | the LWR field itself: declared rho(class, t) is the setpoint, materialisation/dissolution at the rungs is the actuator |
| tiers over bookkeeping truth | Origins Meta AI virtual/bulk/real | the presence ladder: field (LWR accounts) -> rails (time-gap following + SpeedProfile -- MassTraffic's own model, no IDM needed) -> body (Rig promotion near the camera) |

Budget stays engine-side and hard (AC Unity's fixed N per tier): a density declaration changes
the FIELD, never the frame budget -- degrade on detail, refuse on existence.

Pitfalls, sourced, into the contract: (1) frustum-as-spawn-policy is the documented failure
(Cyberpunk turn-around, MSFS gate despawns) -- the field never despawns, only representations
change rungs, with hysteresis; (2) every shipped junction has a deadlock VALVE (SUMO teleports
after 300 s, CS1 despawns) -- ours is explicit: demotion into the field, never a silent
teleport, or the declared density lies exactly in the jam; (3) calibration oscillation: the
control period is much longer than a frame, field accounting continuous, materialisation
damped; (4) lane choice is running state on the rails rung, never part of a baked path
(CS1 -> CS2); (5) THE SEAM IS THE DEFECT (MSFS's visible one sits at data->behaviour): every
promotion/demotion carries a conservation assertion over the accounts. What no reference
delivers and outshine must contribute: the conservation bookkeeping between rungs -- exactly
where every studied system leaks.
