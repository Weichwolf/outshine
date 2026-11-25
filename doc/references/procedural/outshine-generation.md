# Outshine generation methods

Generate a plausible, connected world from sparse map, elevation and weather
data. Research determines geometry, recipes and context. Established methods
handle presentation, spatial storage, navigation and simulation.

The new native implementation uses the [Python reference package](../../../lib/),
its fixtures and independent holdouts. Compare topology, contacts, visual
coverage, work and memory before porting. Reuse portable research data and
algorithms. [Legacy native code and experiments](https://github.com/Weichwolf/outshine/tree/legacy/2026-11-25)
provide historical comparisons.

| Area | Reference | Next research |
|---|---|---|
| [Transport #1964](https://github.com/Weichwolf/outshine/issues/1964) | [Canonical ports](../../../lib/src/outshine_ref/transport/canonical_ports/), [height fitting](../../../lib/src/outshine_ref/transport/port_heights/), [junction pads](../../../lib/src/outshine_ref/transport/junction_pads.py), [fixtures](../../../lib/tests/fixtures/) | Generalised and clipped ways, grade-separated continuity, bounded recipe choice, bridge and tunnel transitions, connected terrain and building contacts |
| [Buildings #1963](https://github.com/Weichwolf/outshine/issues/1963) | [Roof and facade recipes](../../../lib/src/outshine_ref/buildings/), [interiors](../../../lib/src/outshine_ref/buildings/interiors/) | Incomplete classes and heights, regional variation, difficult footprints, industrial types and reliable placement |
| [Vegetation #1961](https://github.com/Weichwolf/outshine/issues/1961) | [Habitats and plant reference](../../../lib/src/outshine_ref/vegetation/), [species recipes](../../../data/world/species/) | Site and season-dependent populations, ground contact, forest edges, understory and near/mid/far appearance |
| [Ground #1980](https://github.com/Weichwolf/outshine/issues/1980) | [Material composition](../../../lib/src/outshine_ref/materials/), [ground recipes](../../../data/world/ground-materials.json) | Terrain-relative material distribution, drainage, erosion and coherent surface variation |
| [Water #1966](https://github.com/Weichwolf/outshine/issues/1966) | [Water profiles](../../../lib/src/outshine_ref/water/) | Shorelines, crossings, plausible connected levels and constraints shared with transport and terrain |

Primary methods include [procedural roads and interactive street modelling](../transport/README.md),
[procedural building modelling](../buildings/siggraph/2006-procedural-modeling-of-buildings.pdf),
[plant ecosystems](../vegetation/siggraph/1998-plant-ecosystems.pdf), and
[parameterised trees](../vegetation/siggraph/1995-creation-and-rendering-of-realistic-trees.pdf).
