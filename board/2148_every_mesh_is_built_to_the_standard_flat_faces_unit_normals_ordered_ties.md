Type: bug
State: open
Area: generators, base
Tags: owner, determinism, audit

# Every mesh is built to the standard: flat faces with their own vertices, unit normals, counter-clockwise front, and no tie a float sort may break

**Benchmark** -- every mesher in Unreal, RAGE, Filament's tools and glTF's own validator:
a face's vertices carry that face's normal, a crease is two vertices, normals handed to a
renderer are unit length, front faces are counter-clockwise (SDL_GPU's default and this
tree's `kGltfFrontFace`), and a body is a closed manifold (board:2146). **All agree**, and
the audit of 2026-09-05 found this tree's road mesher on the wrong side of every line.

## Where it stands, measured 2026-09-05

```
  RoadMesh.cpp:41      every vertex seeded with normal {0,1,0}
  RoadMesh.cpp:63      face normals ADDED into shared vertices across creases, never
                       normalised; Geometry::setNormals(int, span<const float> unit) states
                       the contract they break -- a top vertex leaves with |n| of 1..3
  RoadMesh.cpp:152     the y seed hand-zeroed for side quads: the defect noticed and patched
                       locally
  RoadMesh.cpp:106,    float sorts on an angle with no tiebreak decide a junction's fan and
  Corridors.cpp:973,   its yield ring; two legs at one bearing (a way drawn twice, common in
  :1062, :1095         OSM) leave the order to the sort
  the picture          near-black (15/255) specks at every junction body where a normal
                       points down; 164 -> 452 pixels under 20/255 at Heidelberg
  BuildingMesh.cpp     flat faces with own vertices, split on crease -- the standard, kept
```

## The solution

- `RoadMesh` builds the way `BuildingMesh` does: a face's vertices are its own, the face
  normal is computed once from a known winding (CCW seen from the outward side) and written
  unit length; the "facing" test that flips a winding at run time goes, because the winding
  is known when the corners are ordered
- every sort whose order reaches geometry sorts on (angle, edge index) or (angle, corner
  index) -- a total order, so equal angles are ordered by declaration and not by the sort
- a case: the road mesher's output through `Geometry` has every normal within 1e-6 of unit
  length and every triangle's winding agreeing with its normal (dot > 0); the negative
  control seeds one vertex with {0,1,0} and the case goes red

## What will be true

- [ ] the case above holds for every reference place and for the junction bodies alone
- [ ] no vertex normal in the tree is accumulated across a crease: `grep "NormalM\[.*\] +="`
      reads 0 outside a per-face build
- [ ] near-black under 20/255 at Heidelberg reads at or below the reference's 164, counted
      by `test/scripts/pixels.py`, and the picture looked at
- [ ] every float-keyed sort feeding geometry carries a tiebreak, listed in the item
