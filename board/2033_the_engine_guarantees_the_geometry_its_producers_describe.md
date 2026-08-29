Type: feature
State: open
Area: geo
Tags: benchmark, target, measured

# The ENGINE guarantees the geometry, and a producer can only DESCRIBE it

**Benchmark** — Unreal: a producer fills an `FMeshDescription` — vertices, edges, polygons, and
per-vertex-instance attributes — and `FStaticMeshBuilder` does the welding, the tangent basis, the
render-vertex split and the validation. A polygon mesh with a non-manifold edge FAILS THE BUILD; no
importer is trusted to get it right, and glTF, FBX, Datasmith and a procedural tool all go through
the same door. RAGE: the exporter validates and refuses, and every asset in the game passed it.
**They agree completely**, so the matter is closed and only the shape of it here is mine.

**Taking that.** Whether a glTF is read, a terrain tile is meshed, an OSM structure is grown or a
plant is placed, the producer SAYS what the shape is and the engine answers for what it becomes.

## Why, measured rather than argued

Rothenburg, from the buildings alone:

    2 781 105   corners emitted
      975 372   identical in POSITION AND NORMAL -- pure duplication, 35 per cent
    1 805 733   distinct, which is what a vertex buffer should hold
      494 944   distinct POSITIONS, which is what the topology should hold
       58 395   edges on one triangle, so holes
       20 630   edges on more than two, so not a surface

Every producer in this tree emits a flat triangle SOUP. Nothing relates one corner to another, so
closedness is not a property anything HAS — it is reconstructed afterwards by a walk that lives in a
test. Five builders wrote their own polyline along one footprint edge and nothing could notice they
disagreed until that walk welded them and counted. **With one description shared by all of them,
none of the defects this session closed could have existed.**

## The shape of it

A `MeshBuilder` in `src/base/`, below the generator tier, that a producer cannot go around because
`StructureMesher::Mesh` takes one instead of a `std::vector<float>&`.

    what a producer may say          what it may NOT do
    ----------------------------     -------------------------------------------
    at(E, N, Z) -> Position          push a float
    loop(span<Position>, outward)    choose its own winding
    quad(a, b, c, d)                 emit triangles at all
    attributes per face-corner       delete, drop, or refuse a face of its own

Six things the engine then owns, once:

1. **SNAPPING, not welding.** `at()` quantises to a millimetre and returns a handle. Two corners
   meant to be one corner ARE one handle, by construction. Welding repairs a drift that snapping
   never allows to start.
2. **The topology.** Positions and faces, half-edges derived. A shared edge is one object.
3. **Triangulation.** A producer states a LOOP; the engine cuts it. Two producers meeting on one
   boundary state the same loop and cannot disagree about how it is subdivided.
4. **Winding.** A producer states which side is outward; the engine orders the corners. A face
   cannot be inside out.
5. **The render-vertex split.** Positions welded for topology, render vertices split where a normal
   or a UV differs. That is the 35 per cent above, recovered once and for everybody.
6. **Reduction, never deletion.** Where a face is too small to carry a pixel the engine collapses an
   edge under an error bound and the shell stays closed. Nothing is ever discarded — dropping a
   triangle takes three edges out of the walk and can hide the hole beside it, which is measured in
   board:2031 and cost this tree a false "whole" reading.

**And the engine REFUSES.** `Closed()` is answered by the builder about itself, at build time, with
the producer named. A hole stops being a number a test finds later and becomes a refusal on the day.

## The measurements that would show I am wrong

1. **The corner count.** 2 781 105 emitted today against 1 805 733 distinct. If the split is done
   once by the engine the vertex buffer holds the smaller number and the index buffer the larger; if
   the emitted count does not fall by about a third, the split is not happening
2. **The negative control is a producer that tries to lie.** A test producer that states an open
   loop, a face with two corners in one place, or two faces on one edge with the same winding — the
   build must REFUSE each, by name. If any of them gets through, the guarantee is a wish
3. **Cost, as a BOUND and not a tick**: `apps/bench`. A hash lookup per corner is not free and the
   frame budget is what it is. Quoted in whatever item spends it
4. **Every producer, not one.** glTF, terrain, OSM and vegetation all through it. If any keeps a
   private path to a float buffer, the guarantee has a hole in exactly the shape of that path
