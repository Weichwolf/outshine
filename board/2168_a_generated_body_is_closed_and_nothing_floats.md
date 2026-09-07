Type: bug
State: open
Area: generators, render
Tags: measured
Depends: nothing

# A generated body is CLOSED, nothing floats, and the oracle names an OUTSIDE

**Benchmark** -- Unreal: a mesh that fails its build checks (open edges, degenerate faces, flipped
normals) is reported at import and Nanite refuses to build it. RAGE: the exporter runs the checks
and a body that fails does not ship. **Neither ships a body with an open edge**, and both catch it
with a check that has an external reference -- the outside of the solid -- rather than the mesh's
agreement with itself.

## Where it stands, measured 2026-09-07, looked at in the frames at 4x

| what | where |
|---|---|
| **Roof and wall do not share the eaves edge.** A dashed row of background pixels runs along the eaves: the two surfaces are built separately and never welded | OldTown ~(200-360, 640-690) and ~(720-815, 612-622), and along nearly every eaves in the frame |
| **Neighbouring roofs interpenetrate.** A gable spears through a roof plane with no valley and no cut | OldTown ~(360, 600), ~(870, 600) |
| **A roof plane stands over a footprint it does not belong to**, cantilevered into air | OldTown ~(230-300, 660-700) |
| **A junction is a HOLE.** Black star-shaped gaps at road junctions in the plan view -- a fan whose normals point down, back-face culled | ZurichPlan ~(580,310), ~(527,269), ~(465,245), ~(430,294) |
| **Holes in the carriageway.** Black single pixels in a row across the surface | OldTown ~(792-822, 655-659); Kaiserberg ~(555-570, 442) |
| **Terrain pierces the carriageway**, and the ribbon ends with a hard edge and no kerb | OldTown ~(742-805, 647-662) |
| **Building blocks stand in mid-air**, detached from any ground | Graz, upper left of the frame |
| **A hole in the terrain**, a dark pocket inside a smooth hill flank | Feldkirch, right of the frame |

**And the gate that should catch the junctions is GREEN.**
`test/outshine/places/ScoreEveryMeshFacesOutward.cpp` reads
`streets: triangles wound against their normals == 0` and passes. It cannot see the defect: a
triangle wound consistently with its OWN normal agrees with itself, which is the grade the corpus
table gives a SNAPSHOT, and a fan whose normals all point DOWN passes that test and renders as a
hole. The case also covers streets only -- roofs and walls have no equivalent.

## The solution

1. The orientation oracle takes an EXTERNAL reference, as CLAUDE.md's craft rule now states: a
   carriageway's face against +up, a body's against the ray from its centroid
2. Two counters that need no reference because they are true of a mesh alone: a face of zero area,
   and an edge that only one triangle owns. The open eaves falls out of the second
3. The same case covers what the BUILDING generator hands over, not only the road
4. Roof and wall share their eaves vertices rather than being built apart
5. One footprint yields one body: gable, roof and wall are meshed together and do not intersect

## What will be true

- [ ] The oracle goes RED on today's tree at the junctions, and names how many
- [ ] Unowned-edge count reaches zero on the building generator, and the dashed eaves is gone in
      the picture, looked at at 4x
- [ ] Negative control: invert one junction fan on purpose and the count rises by exactly that fan
- [ ] The nine surveyed places carry no hole a 4x crop can find

## What will show I was wrong

The counters read zero and the holes stand. Then the junctions are MISSING polygons rather than
inverted ones, and the unowned-edge count is what says so.
