# Procedural infrastructure from map inputs

Owner: [2281](../../../board/2281_semantic_osm_edges_drive_native_road_alignment.md).
OSM supplies placement, distribution, classes and available parameters. Recipes construct
plausible functional infrastructure; reconstruction of an exact digital twin is not the goal.

| Reference | Relevant method | Boundary |
|---|---|---|
| [Galin et al., Eurographics 2010](https://perso.liris.cnrs.fr/egalin/Articles/2010-roads.pdf), [local PDF](eurographics/2010-procedural-generation-of-roads.pdf) | Smooth trajectories, procedural cross-sections, road/bridge/tunnel geometry and terrain excavation/embankments | Use model/contact generation; retain useful supplied OSM layout rather than rerouting every road |
| [Chen et al., SIGGRAPH 2008](siggraph/2008-interactive-procedural-street-modeling.pdf) | Street graph separated from generated geometry | Tensor-field network invention is not required for existing map inputs |
| [OSM2World RoadModule](https://github.com/tordanik/OSM2World/blob/master/core/src/main/java/org/osm2world/world/modules/RoadModule.java) | Road classes, lane layout, separate junction/connector recipes and surface selection | Java reference implementation; not an A18 Pro performance or functional-network proof |
| [SUMO OSM import](https://sumo.dlr.de/docs/Networks/Import/OpenStreetMap.html), [source](https://github.com/eclipse-sumo/sumo/tree/main/src/netbuild) | Functional road/rail networks, junctions and explicit type mappings | Original OSM topology differs from clipped/generalized MVT; elevation reconstruction is heuristic |
| [CARLA standalone OpenDRIVE](https://carla.readthedocs.io/en/latest/adv_opendrive/), [source](https://github.com/carla-simulator/carla/tree/ue5-dev/LibCarla/source/carla/road) | Independently generated drivable road meshes, junction smoothing and spatial mesh chunks | OpenDRIVE already contains a richer road description; diagnostics must not hide gaps with widened roads or safety walls |

These are reusable building blocks, not proof of a complete functional 240 km world.
No requirement to add their runtimes, formats or source code to Outshine.

## Local source review

Research checkouts live outside Outshine in `/Users/cosmo/Git/outshine-references/`:
`OSM2World` (`8ec26a9e`), `sumo` (`fcc45c07`), `carla` (`28244ba2`), reviewed 2026-10-09.
Shallow sparse clones retain relevant geometry, network,
elevation and test modules. Record the actual commit when deriving an implementation.
Inspect topology, vertical constraints, shared boundary vertices and mesh recipes separately.
Evaluate selected methods against the same real Place inputs and visual/functional requirements.
