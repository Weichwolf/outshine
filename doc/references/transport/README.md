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

## Conforming ground and explicit contacts (Python, #20)

Galin's section 6.2 and Figure 20 describe terrain modification around the generated
road, using refined terrain and excavation/embankment. Page 9 was inspected visually
on 2026-10-13. The current experiment partitions one domain into existing traffic
faces and their exterior with
[constrained Delaunay triangulation](https://shapely.readthedocs.io/en/2.1.2/reference/shapely.constrained_delaunay_triangles.html).
A shared bounded height solve avoids incompatible independently fixed road/soil
contacts. This is a coarse C0 composite top surface; it does not implement the paper's
clothoid trajectories, adaptive terrain fidelity, earth volumes or smooth transitions.

The footprint comparison checks
[coverage union](https://shapely.readthedocs.io/en/stable/reference/shapely.coverage_union.html)
against general union only after verifying edge-matched nonoverlapping inputs; output
coverage and boundary audit are identical. `geometry.SurfaceSampler` uses the
[query-only STRtree](https://shapely.readthedocs.io/en/2.1.2/strtree.html) over projected
facets, then evaluates their affine planes. The benchmark compares the distance
predicate with boxes plus the same exact distance filter and requires equal complete
contact results. Shared topological features select deterministically; stacked,
unshared or conflicting hits are unresolved. Surface kind/frame/source association
remain explicit at the caller; a projected spatial index has no implicit ground meaning.

## Fixed-plan terrain refinement: rejected visual candidate (#20)

For the real stair/bridge terrain repair (#434), Desbrun et al., SIGGRAPH1999,
[original paper](https://people.eecs.berkeley.edu/~jrs/meshpapers/DesbrunMeyerSchroderBarr.pdf),
[local PDF](siggraph/1999-implicit-fairing.pdf), page2 §2.1 equation2 and page3 §2.7,
provides the membrane-energy and hard/soft-contact background. Both pages were
inspected visually. The experiment uses its own constrained scalar displacement
energy on fixed XY, with whole-facet class/bridge bounds; it does not implement
the paper's complete curvature-flow or volume-preservation algorithm.

Research starts with Lindstrom et al., SIGGRAPH 1996,
[primary paper](https://www.cs.princeton.edu/courses/archive/spr01/cs598b/papers/lindstrom96.pdf),
[local PDF](siggraph/1996-continuous-height-field-lod.pdf): edge delta segments and
compatible neighbouring levels (page 4, Figure 3). Duchaineau et al., Visualization
1997, [primary paper](https://www.classes.cs.uchicago.edu/archive/2003/fall/23700-1/docs/roam.pdf),
[local PDF](visualization/1997-roaming-terrain.pdf), page 4 Figures 3–5, describes
paired splits preserving conformity. Both diagram pages were inspected visually.

`ground_refinement.py` compares paired edge midpoints and face centroids on the
same saved Vienna mesh, 2500 added vertices, 65536 faces and 2:3 soil-grade limit.
Old vertices/roads/outer edges remain fixed. Each new height is projected onto
the exact scalar interval induced by whole-child gradient bounds; face-disjoint
batches keep those intervals independent. A local 45-point-per-face DEM check
further restricts changes. This is not ROAM's hierarchy or optimality algorithm;
the DEM check is sampled and is not a global source-error certificate.

The first version reduced RMS but increased held-out extreme errors. The local
quality interval corrected the measured regression. Centroids then improved
mean/RMS/p95 at lower cost than edge midpoints, while midpoints had the smaller
measured maximum. The actual 3D inspections still show triangular tent forms
and sharp creases. This candidate remains a rejected visual experiment, not an
approved terrain generator or library migration. Numeric grade/coverage and a
smaller aggregate source error do not prove an aesthetic improvement.

Fine triangles stay within their original affine parent. Maximum vertical
coarse/fine difference is therefore attained at a fine vertex. Linear vertex
morphs preserve gradient limits by convexity of the norm, but do not remove the
tent shapes or establish C1. The next method must reconsider interior terrain
connectivity instead of accumulating more fixed-edge fans.

## Soil connectivity after fixed-point refinement (#20)

Hoppe et al., SIGGRAPH 1993, [primary paper](https://hhoppe.com/meshopt.pdf),
[local PDF](siggraph/1993-mesh-optimization.pdf), separates mesh connectivity from
positions and uses local edge swaps (page 5, Figures 4–6, visually inspected).
The experiment implements a smaller fixed-XYZ soil exchange, not the paper's
joint vertex/topology optimization. Roads, interfaces and outer edges remain fixed.

[GEOS Delaunay through Shapely](https://shapely.readthedocs.io/en/stable/reference/shapely.delaunay_triangles.html)
selects a candidate diagonal on strictly convex four-point patches; an angle gain
avoids cocircular tie cycles. The equal-budget alternative uses data-dependent
swaps. Both check every proposed whole-face gradient and the same 60 local DEM
probes, requiring lower squared residual and no larger sampled local maximum.
Face-disjoint batches retain a valid partial result on work-budget exhaustion.
Near-degenerate floating-point patches are skipped conservatively; neither path
claims complete constrained Delaunay or globally optimal fitting.

Reconnection removes many star-shaped ridges in opened same-camera 3D views and
improves independent saved-source probes without changing any XYZ. It remains C0.
Once new edges cross old parents, the earlier root-face vertical LOD certificate
no longer applies. Do not interpolate meshes using that invalid certificate.
Height/time/mesh/DEM limits and measured host costs are separate from runtime gates.

## External SDK operation handbook (#19)

The user-supplied CityEngine SDK 3.4.12206 core is an external comparison source.
`esri_transport_catalog.py` keeps exact ELF ranges, demangled signatures and PLT
imports; `esri_transport_ghidra.py` reuses the repository's Buildings protocol with
restricted disassembly and fresh complete exports. Boundary jumps use explicitly
recorded [Ghidra CALL_RETURN models](https://ghidra.re/ghidra_docs/api/ghidra/program/model/listing/FlowOverride.html).
These constrain analysis; they do not establish external control flow or C++ types.
Binary hashes, instruction ranges, call sites, generated reference hashes and
unverified paths remain in build outputs, never in the native library/runtime.

Isolated scalar oracles verify the public SDK version, offset helper, edge query
and line/ray modes. Analytic controls and saved traffic footprints distinguish
full inset domains from the helper's largest fragment, closed source endpoints
from SDK half-open queries, and real 3D contacts from closest-approach results.
Keep Python's required full domain/contact semantics; do not infer a generator
from matching individual functions or copy decompiled implementations.

SDK triangulation calls resolve to its pinned `libglutess.so` dependency. The
[Khronos GLU callback ABI](https://github.com/KhronosGroup/OpenGL-Refpages/blob/main/gl2.1/xhtml/gluTessCallback.xml)
permits an opaque-handle reference without guessed SDK object layouts. Raw
triangle strips can contain exact-zero primitives; the disclosed CAD adapter
removes those and conforms declared boundary points. This verifies the backend
with that adapter, not SDK Triangulator attributes, modes or Processor extrusion.
Those object ABIs remain open. Research continues from SIGGRAPH's graph/geometry
separation and the bounded Python comparisons above; the SDK is a handbook.
