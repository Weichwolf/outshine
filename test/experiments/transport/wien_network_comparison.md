# Vienna ground traffic around the retained bridge/portal field

Owner: [#303](https://github.com/Weichwolf/outshine/issues/303), under #21.
Python experiment; source originals, declared omissions and the old accepted
products remain separate. No native runtime or complete worldwide-generator claim.

## Geometry and height planning

The former common-ground view from PR #302 still used only 71 of 3,259 source
records. The same archived three OpenFreeMap tiles contain much more ground
traffic. Existing component recipes plan that traffic, retaining source IDs and
attributes. Known old floor/level decisions are preserved. Actual portal air is
reserved with curb/endpoint-repair construction reach, followed by a whole
footprint test. Nonzero or ambiguous `level` tags without a selected surface
are now rejected by the shared flat planner as `deferred_floor_level`, rather
than flattened to the ground. Layer and indoor checks remain independent.

Two constructions compare clipping old/new cells with retriangulating their
identical physical traffic union. Both retain the old footprint. The selected
procedure instead reuses the union **as a domain**, avoiding an intermediate
traffic triangulation for its height plan.

A direct fit with a variable at every dense boundary vertex is retained only as
a bounded negative cost comparison. The selected height field reuses the old
coarse terrain support from PR #302, extended to a planning rectangle. The
rectangle is not rendered and acquires no additional source tile. Coarse faces
touching traffic receive conservative road-gradient constraints; roofs, old lower
surfaces and clipped upper/traffic intersections provide whole affine bounds.
The eight-second solve budget and actual final tolerances remain unchanged.

Road construction uses grade 0.0249 as a numerical construction margin; actual
emitted traffic is independently checked at the original 0.025 limit, soil at
2/3. Upper C1/portal geometry remains unchanged. Actual roads require 4.5 m under
the old deck underside, all soil 0.02 m below that underside, and roofs 2 m below
the 0.25 m soil shell underside. These are explicit procedural recipe values.

Rejected comparison constructions are recorded as absent and cannot prevent
the independent selected field from running. The direct dense construction may
fail its own degenerate-face guard before reaching its solver. No such failed
surface is exported or rendered.

Lifting unconstrained fine triangles across height-field creases is insufficient.
Cutting neighbours independently also failed the real coverage check. The final
method globally nodes the field edges, traffic boundary and actual world boundary,
polygonizes that arrangement, and triangulates its cells. Every emitted vertex
keeps its owned XY and queries only its height. Whole-face normals, complete
road/world domains, old footprint preservation, interior-crack absence, source
axes and actual clearances are replayed. Closed bodies and supplied flat corner
normals are exported for the renderer. Solver status alone proves nothing.

The real indexed bodies exposed Blender 5.2.2 custom-normal packing errors even
without degenerate float32 faces. Sharp edges and reapplying packed normals did
not preserve all supplied vectors. Transport therefore supplies its exact
FLOAT_VECTOR/CORNER normal attribute directly to the material's object-to-world
normal transform and Principled input. A real Eevee pixel control verifies the
attribute/shader path; import checks preserve float32 vectors exactly. Packed
mesh-normal deviations are measured and unused, never silently accepted as the
intended source normals. Shared camera/mesh/daylight APIs remain unchanged.

## References and provenance

First reference: [Chen et al., SIGGRAPH 2008](../../../doc/references/transport/siggraph/2008-interactive-procedural-street-modeling.pdf),
page 2/Figure 2, separates network and 3D geometry; [Galin et al., Eurographics 2010](../../../doc/references/transport/eurographics/2010-procedural-generation-of-roads.pdf),
section 6.2/page 9/Figure 20, motivates coordinated road/terrain construction.
Those pages were optically inspected in the preceding terrain work. The reduced
height basis is our bounded procedural approximation, not an implementation or
reproduction of those papers or CityEngine.

The arrangement uses the existing GEOS/Shapely primitives; see the primary
[GEOS overlay documentation](https://libgeos.org/doxygen/classgeos_1_1operation_1_1overlayng_1_1OverlayNG.html)
and [Shapely polygonization API](https://shapely.readthedocs.io/en/stable/reference/shapely.polygonize.html).
No coordinate-precision reduction, foreign source code or SDK runtime is added.

Archived complete capture SHA256:
`aaea9506c3459045f5f6c2eb2df4f96b222e47e1acd88f377ec66e9f70126475`.
Old height-support input:
`e740d969b6b6baa230c50cb1c3475350d8610b1685119c2bb619732acde89442`.
All receipt bytes and old bridge/portal hashes are checked. New source ledgers
cover the original 3,259 records once; every omitted source has a concrete reason.
The separate new lower plan needs consumer binding before library/native adoption.

## Verification and presentation

Reproduce with the explicit external paths in [README](README.md). Generation
requires fresh outputs and a clean frozen source. The geometry process has 3 GiB
address-space and 180 s CPU limits; its source worker separately has 2 GiB/60 s
CPU and a supervising wall timeout. Solver limits remain eight seconds. These are
offline experiments; no frame/device budget or full traffic-routing claim.

New 720p before/after pictures use identical regional camera and presentation.
Blender/bpy 5.2.2/Eevee consumes shared camera verification, original triangle and
supplied-normal import, and agreed AgX/daylight from #291. Material colours remain
procedural inspection choices. No ground is flattened or added for display.
One local 1920×1080 after picture uses the same camera for detail checking.
All images stay local; only master publishes integrated Places.

Final clean-run hashes, measured work/allocations/copies/RSS and the actual opened
image decision are recorded with the completed delivery. Missing measurements
remain unproven; geometric validity is independent of visual approval.
