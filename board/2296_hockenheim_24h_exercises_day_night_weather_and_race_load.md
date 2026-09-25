Type: proof
State: open
Architecture: planned
Parent: 2169
Depends: 2261, 2128, 2172, 2129, 2155, 2140, 2212, 2092
Priority: P2
Area: scenario, simulation, render, audio, streaming
Tags: hockenheim, day-night, weather, acceptance

# Hockenheim 24h exercises the integrated world under race load

## Boundary

After the OSM/DEM camera lap (2260) and one genuine vehicle lap (2261), use
the same native route and contact products for a reproducible 24-hour race
scenario. Racing rules, cars, pit strategy and audience belong to scenario
data/scripts and generic simulation contracts, never a Hockenheim branch in
the engine. A replay may advance simulation time faster than wall time, but
physics uses bounded fixed steps and every timestamp names a weather snapshot.

This is a concentrated integration proof, not worldwide completeness:
mountains, waterways, rail, interiors, other climates and arbitrary OSM
crossings retain their own WIs and acceptance scenes.

## Scene matrix

| Period | Required visible and functional effects |
|---|---|
| Dawn/dry | sun angle, long moving shadows, sky adaptation, warm tyres/road |
| Noon/cloud | cloud transmittance, readable shaded buildings, crowded track/pit |
| Dusk/rain | wet MR surfaces, spray, puddles, reflections, reduced grip/visibility |
| Night/fog | thousands of visible lamps, budgeted shadows, headlight cones and stereo passage |
| Return to dawn | continuous astronomy, weather drying, exposure/history without jumps |

Multiple vehicles follow certified lanes and avoid contacts; a pit entry/exit
tests route branching, low speed and shared track/yard geometry. Spectators,
marshals and parked vehicles may use bounded agents/instances. Sound must
preserve engine Doppler, distance, occlusion and left/right motion on stereo
speakers and headphones. OSM supplies geometry evidence; scenario supplies
race participants and weather schedule without claiming historical truth.

## Acceptance

- [ ] Continuous race replay: no route teleport, wrong-level shortcut,
      contact hole, geometry eviction under wheels or non-deterministic result
      at fixed source revision/seed. Report vehicle stations, collisions,
      weather snapshots and streaming state at sampled ticks.
- [ ] Open stills and moving captures at all table rows from cockpit, trackside,
      pit and interior/window. Compare linear light, depth, shadow, material,
      reflection and display AOVs. One missing pass cannot be masked by grading.
- [ ] Alter one light, caster, surface wetness or fog volume in a negative
      control; its predicted local image/audio/handling effect changes while
      unrelated scene state stays fixed.
- [ ] Measure p50/p95/p99 and worst frame, physics tick, GPU pass time,
      memory, IO/queue delay and cold/warm startup on target-class hardware.
      An isolated empty-track benchmark cannot pass the full-race budget.
