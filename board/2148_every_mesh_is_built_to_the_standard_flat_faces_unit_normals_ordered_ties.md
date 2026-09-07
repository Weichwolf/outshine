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

## Landed 2026-09-05: the door holds the standard, and the road mesher meets it

`Geometry::setNormals` refuses a normal that is not unit length (`kUnitWithin` 1e-3 [SET, a
thousand float roundings]) -- glTF's own rule, at the door where every mesh enters --
and `Geometry::windingAgainstNormals(part)` counts the triangles whose counter-clockwise face
normal opposes their vertex normals, ignoring zero-area faces (`kNoAreaM4` 1e-12). The road
generator publishes that count for the streets part, `TheDoorRefusesANormalThatIsNotUnit`
holds the door with the flipped triangle as its negative control, and
`ScoreEveryMeshFacesOutward` holds the streets at OldTown. What the count found, in order,
each measured before it was touched:

```
  OldTown, streets part, triangles against their normals
  38 970   the junction body's side quads were wound inward (culled, so the sides were
           never drawn and the specks were the tops seen through the missing sides)
     226   the ribbon's kerb walls shared vertices with the top and the underside, so a
           vertical face carried a vertical normal -- the sweep gets four wall vertices per
           station with outward normals
      85   the body's "up" was the cross product of two adjacent corners of ONE gate, a
           sliver; the mesher takes the junction's plane (`RoadPlane`) and puts every corner
           on it
      24   zero-area faces where two corners coincide -- skipped by the mesher and ignored
           by the count
       8   reflex corners where two nearly parallel legs overlap: the ring is trimmed to
           star shape before the fan (SUMO joins parallel legs instead; that is the map's
           business, board:2101)
       0
```

- [x] the case holds the streets at OldTown, the door's case holds the refusal and the
      flipped triangle
- [x] no accumulated normal across a crease in the road mesher; the building mesher never had
      one
- [x] near-black at Heidelberg: 105 pixels under 20/255 against the reference's 164, measured
      2026-09-05 by pixels.py on Heidelberg-e556bec1 and looked at -- the river bank's shadowed
      walls, none under open sky
- [x] every float-keyed sort feeding geometry carries a tiebreak (ByBearing, the corner's
      gate index)

Measured 2026-09-05 against c71f1bb0's pictures: OldTown 0.7 % of pixels, Heidelberg 1.3 %,
Shibuya 1.9 %, CentralPark 1.0 %, Venice 0.01 %, Jura 0.2 %, ZurichPlan 2.8 %, Kaiserberg 0.6 %,
Koehlbrand 1.9 % -- the junction bodies' sides and the ribbons' kerb walls, drawn for the
first time. Looked at Kaiserberg's motorway edge and Koehlbrand's ramps: a thin dark line
along every band's lower edge, the kerb face. It reads (16, 15, 16) in shadow, where a
shadowed wall reads 70 of 165: the kerb gets 1.8 % of its lit value against the wall's
16 %, which is not the standard's business but the lighting of a dark vertical face --
recorded in board:2149. Near-black under 20/255 rises by exactly those faces (Heidelberg
25 -> 97, Kaiserberg 41 -> 294, Shibuya 116 -> 387); pure black 0 everywhere.
