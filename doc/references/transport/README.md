# Procedural transport from map inputs

Owner: [#11](https://github.com/Weichwolf/outshine/issues/11).
OSM supplies placement, distribution, classes and available parameters. Recipes construct
plausible functional transport; reconstruction of an exact digital twin is not the goal.

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

## Closed surface bodies (Python, #50)

The SIGGRAPH 2008 pipeline separates the retained street graph from 3D geometry
(page 2, Figure 2). Galin et al. describe parameterized road cross-sections and
explicit terrain-relative road/bridge/tunnel segmentation (page 8, Figures 17–18).
Both local PDF pages were inspected visually on 2026-10-13.

`transport.close_surface` implements one smaller geometry step: cap an already
validated conforming XY mesh at its supplied vertex heights with a vertically
offset bottom and walls on directed outer/hole boundaries. Interior interfaces
receive no duplicate walls. This does not implement either paper's complete
network, profile or terrain solver.

`surface_solid_benchmark.py` compares sorting and dictionary grouping on identical
T/ring fixtures and a saved Vienna tile, requiring byte-identical closed meshes
and an independent oriented-edge/volume audit. Runtime and worker peak RSS are
offline CPU measurements; the showcase opens the exact meshes in Blender
Workbench. Heights in these inspections are explicit recipe assumptions.

## Whole-face anisotropic heights (Python, #20)

Following the SIGGRAPH graph/geometry separation and Galin cross-section model
above, an affine triangle has a constant height gradient. Bounding its normalized
two-dimensional norm therefore bounds the entire face; bounding edges alone does
not. `transport.fit_surface_heights` uses shared vertex heights and minimizes the
maximum change from a coarse DEM prior with fixed vertex contacts when supplied.
This is a smaller fixed-mesh reference operator, not either paper's full road solver.

[Clarabel's Python cone formulation](https://clarabel.org/stable/python/getting_started_py/)
provides the selected second-order cone solve. Its
[settings](https://clarabel.org/stable/api_settings/) supply iteration/time/thread
limits. The experiment compares it with SciPy/HiGHS and an inscribed regular
16-sided approximation of the same norm bound: conservative, with a smaller
feasible set. Both use identical saved inputs, quality bounds and solver time.
An independent cross-product normal audit decides whether a result is valid;
timeout alone neither authorizes nor forbids output. Time limits can overrun
within a solver step; preparation and full-worker peak RSS are measured separately.

[Mapterhorn data access](https://mapterhorn.com/data-access/) specifies the 512-pixel
Terrarium WebP source. A saved zoom-11 Vienna tile is sampled at pixel centres;
source bytes/URL/hash and original OpenFreeMap IDs are retained. Recipe grade bounds
are procedural choices, not measured lane tags. Shared heights remove steps but
do not prove C1, superelevation, drivable turns or terrain/building/plant contacts.
The latter require the coordinated spatial products in #18/#20/#47 before an API change.
