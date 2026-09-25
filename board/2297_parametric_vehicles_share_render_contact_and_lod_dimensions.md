Type: feature
State: open
Architecture: planned
Parent: 2169
Depends: 2150, 2171
Priority: P2
Area: generators, simulation, render, scenario
Tags: vehicles, procedural, materials, lod

# Parametric vehicles share render, contact and LOD dimensions

## Contract

A vehicle recipe is semantic data, not a hand-modelled mesh: class/archetype,
length, width, height, wheelbase, track, overhangs, wheel radius, cabin span,
body cross-sections, glazing, lights and material slots in SI units. A bounded
construction grammar makes sedan, hatch, wagon, van, bus, lorry and race-body
families; constraints reject impossible wheel/body intersections and impossible
visibility. Style variation changes panels, taper, roofline, fascia and colour,
not wheel contact or route clearance by accident. OSM does not identify car
models: scenario/world rules supply type distribution and seed.
Default visual era is plausible electric Solarpunk 2050: compact packaging,
functional lighting and repairable surfaces. A scenario may override era and
powertrain; generated geometry must follow its actual packaging and safety
constraints, not wear futuristic trim over an unchanged chassis.

Recipes may instead declare a design brief: payload/seats, target speed,
range, manoeuvrability, stability, cost and style bounds. Generate candidate
proportions, reject violated geometric/physical constraints, then select a
Pareto-valid design under declared priorities. Futuristic forms are welcome.
Mass/inertia, tyre loads, projected area and power/energy budget must derive
from the selected dimensions; drag estimates remain labelled approximations
until validated against an independent aerodynamic oracle. No fictitious
single "optimal car" without an objective and constraints.

The generator emits Outshine's native geometry and MR material IDs, then
derives near/mid/far LOD, shadow proxy, collision/contact shape and light
anchors from one validated recipe. Wheels and steering animate as independent
parts. Decals, dirt and wear are procedural layers or generated caches; no
required photo/paint texture assets. Imported glTF vehicles remain possible
through the same native contract. Geometry can be stylised between wasm-dd2's
coarse silhouette and costly hand-sculpted assets; simulation dimensions and
light placement remain physically coherent.

Batch recipes into a small set of shader/render states and instance per
archetype. Bounded worker generation and cache reuse prevent per-frame mesh
construction. A common wheel/contact contract serves 2261's arcade and
realistic handling models; a visual mesh is never the collision oracle.

## Acceptance

- [ ] Parameter extremes and negative cases check closed meshes, winding,
      finite normals, wheel clearance, steering envelope and material binding.
- [ ] Three body families across seed variants have distinct silhouettes from
      front/side/rear at 2, 20 and 200 m; open PNGs at day/night and wet/dry.
      Light clusters, glazing, underbody and tyres remain legible without
      repeating identical cars across the Hockenheim grid.
- [ ] LOD transitions preserve wheel centres, overall bounds, light anchors
      and collision dimensions; no pop or false contact in a moving convoy.
- [ ] Compare generation cost, resident bytes, draw count and GPU p95/p99
      for one car, a full race grid and dense city traffic. A 24-hour race
      (2296) uses these recipes without track-specific geometry code.
- [ ] Conflicting design briefs yield different Pareto choices. Increasing
      payload or tyre width updates mass/contact/clearance; impossible briefs
      fail with violated constraints rather than a cosmetically plausible car.
