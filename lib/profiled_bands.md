# Profiled facade bands

`outshine_ref.buildings.profiled_bands` sweeps a small, explicit architectural
cross-section around a polygon boundary. It supplies geometry, not a building
recipe or entrance planner. The source polygon is immutable, finite local ENU XY
metres. Exterior and courtyard rings are oriented consistently; edge IDs follow
that oriented exterior, then its holes. They do not depend on the selected edges.

```python
from outshine_ref.buildings.profiled_bands import BandPlan, band_surface

profile = ((0., 0.), (.08, 0.), (.2, .12), (.2, .3), (0., .3))
plan = BandPlan(source_polygon, profile, base_m=12.)
mesh = band_surface(plan, edges=(0, 1), max_triangles=4096)
```

The profile is an immutable tuple of `(outward_depth_m, height_m)` points. It
begins at `(0,0)`, rises monotonically around a simple positive-depth outer
surface, ends on the wall at positive height, and closes along that wall. A
horizontal step is allowed. At most 512 boundary edges and 32 profile points;
finite base height and miter ratio in `[1,16]` (default 4). Opposed normals,
degenerate/reversed edges, excessive miters and offset topology changes fail
explicitly. The maximum offset is compared with GEOS's mitred polygon buffer.
This is a bounded, topology-preserving offset, not a general wavefront solver.

Adjacent outward unit normals `a,b` give the miter vector `(a+b)/(1+a·b)`.
Its projection onto either edge normal is one. Every profile depth therefore
has the stated perpendicular distance to each original wall. The maximum
offset must keep every source edge forward and the same exterior/hole topology.
Along a fixed combinatorial offset, earlier depths interpolate the same edge
planes. `edge_footprints(plan)` returns conservative XY quadrilaterals for the
caller's neighbor/reservation checks. It does not itself select obstacles.

`edges=None` selects every boundary edge; otherwise use unique bounded integer
IDs in a tuple. Output order is canonical. A missing adjacent edge produces a
triangulated profile cap on the miter plane. Thus isolated edges and joined runs
remain closed solids. Hard triangle normals and planar orthonormal metric UV
charts accompany caller-owned `vertices`, uint32 `faces`, `normals`, `uv`, and
per-face uint32 `edge_ids`. Material/source/frame/visibility/asset ownership is
the caller's. The original wall-contact backs are included; the source wall
remains its own component. Output triangle capacity is checked before sweep
expansion. Empty selection emits empty arrays, after bounded plan validation.

`method="scalar"` and `"batch"` emit identical arrays. Batch broadcasts all edge
placements once; scalar materializes each edge separately then assembles them.
This comparison covers placement work, not the whole geometry algorithm:
offset validation, normals, hard-face expansion and caps are shared work.
Actual wall-clock, output bytes and traced peak must be measured separately;
fewer placement arrays alone do not prove lower whole-process peak or device time.

The current city experiment derives plinth, selected first/upper-storey belts,
and a stepped cornice from the existing `FacadeRepeat` gaps. It preserves
source walls, opening axes, roofs, room controls and attributes. Neighbor source
footprints with overlapping heights reject intersecting edge supports before
emission. Partial runs are capped. Special families, clipped sources, collapsed
offsets and incomplete boundary frames retain the original representation.
Context landuse polygons are not building obstacles; the mixed city-input
control verifies identical geometry with and without that context layer.
Transport reservations and physical entrance routes are not yet consumed by
this new trim path. Source solids are unchanged; outward trim extents are
procedural additions, not measured OSM footprints.

The experiment determines demand from facade metadata before reading mesh
buffers. A conservative displacement sphere bounds the projected distance from
the original wall: with camera-space source position `q`, nearest positive
depth `z`, camera movement radius `m`, and maximum miter displacement `r`, the
error is at most `f*r*(max|q|+m)/(z*(z-r))`, where `z` includes movement. Plane
crossing keeps detail. Frustum tests expand by `m+r`; no source-wall backface
test discards the band's potentially visible upper/lower surfaces. This bound
only controls positional displacement. The painted far wall is not an admitted
equivalent for radiance, shadows, night lighting or changing LOD transitions.

Independent controls cover oriented watertight solids, planar metric UVs,
actual horizontal mesh slices and analytic/GEOS-integrated
rectangle/concave/courtyard volumes after rigid XY
transforms, every nonempty rectangular edge subset, stable edge IDs, equal
scalar/batch output, empty products, capacity-before-expansion, invalid profiles,
sharp corners and collapsing courtyards. Actual source consumers additionally
check original arrays/attributes, window-free heights, far no-buffer access,
height-aware neighbor exclusions, independent projection/movement displacements,
mixed landuse inputs and truthful atomic fallback reporting.

Primary references: local SIGGRAPH 2006 *Procedural Modeling of Buildings*,
PDF page 3/Figure 4 (absolute ledge and repeated floors), and SIGGRAPH 2003
*Instant Architecture*, PDF page 2/Figures 2–3 (coherent partitioning), visually
inspected. Local Esri Candler `Base/Mid/Upper/Top/Ledge/Cornice` rules demonstrate
facade hierarchy and asset trimming. Existing pinned Ghidra extrusion dispatchers
inform the primitive boundary; they do not establish this profile algorithm or
full SDK geometry equivalence. No vendor code, profile mesh or texture is copied.
