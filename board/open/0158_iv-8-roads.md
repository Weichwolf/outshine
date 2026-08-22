Type: feature
Area: generators
Tags: scope
Depends: 1118, 1119

**IV.8 Roads**

Every item below is its own task.

**Acceptance, shared by every child**: done = a render case exists in `test/khronos/glTF/` for this type, cites its `board:NNNN`, and is within the picture bound (`CLAUDE.md`).

**Cost of the full sweep**: about 858 x 8.17 s of Blender, roughly two hours, so the corpus is built once and cached rather than re-rendered.

**Retention**: after validation both `outshine.raw` and `oracle.raw` are deleted; `oracle.exr` and the two PNGs are kept. About 1.4 MB a case against 25 GB today.


---

## Folded children (2026-08-22)

- [ ] Ways carried as centrelines with a declared half-width per kind (`world/StreetField`) *(was 0925)*
- [ ] Seventeen street kinds classified: motorway, trunk, primary, secondary, tertiary, unclassified, residential, living street, pedestrian, service, track, path, footway, cycleway, steps, bridleway, busway *(was 0926)*
- [ ] Rail kinds classified: rail, light rail, tram, narrow gauge, subway, monorail, funicular *(was 0927)*
- [ ] Aeroway kinds classified: runway, taxiway, apron, helipad *(was 0928)*
- [ ] Street polygons as areas rather than ribbons *(was 0929)*
- [ ] Way surface as a class-grid colour under the terrain shader *(was 0930)*
- [ ] Point query: what is made here, and how wide (`generators/Infrastructure`) *(was 0931)*
- [ ] A road drawn as its own geometry rather than as a colour on the terrain *(was 0932)*
- [ ] Carriageway with camber and superelevation on a curve *(was 0933)*
- [ ] Lane subdivision from the width, with the lane count stated *(was 0934)*
- [ ] Hard shoulder and verge *(was 0935)*
- [ ] Kerb with an upstand, dropped at a crossing, with a corner radius *(was 0936)*
- [ ] Gutter channel and drainage grate *(was 0937)*
- [ ] Manhole and inspection cover *(was 0938)*
- [ ] Junction geometry: the corner fillet, the flared mouth, the island *(was 0939)*
- [ ] Roundabout with its island and apron *(was 0940)*
- [ ] Motorway interchange ramps as geometry *(was 0941)*
- [ ] Level difference between carriageway, verge and field *(was 0942)*
- [ ] Cutting and embankment along a road *(was 0943)*
- [ ] Surface by class: asphalt, concrete, setts, gravel, unpaved, and the served `surface` attribute already carries it *(was 0944)*
- [ ] Tracktype as a surface gradient on a farm track *(was 0945)*
- [ ] Wheel-track polish bands and a darker centre strip *(was 0946)*
- [ ] Patches, joints, crack sealing *(was 0947)*
- [ ] Potholes and edge break-up *(was 0948)*
- [ ] Road markings: centre line, lane line, edge line, stop line, give-way triangles, arrows, zebra, box junction, hatching, chevrons, cycle lane, bus lane, parking bay, painted speed limit *(was 0949)*
- [ ] Reflective studs *(was 0950)*
- [ ] Tactile paving at a crossing *(was 0951)*
- [ ] Wet road reflectance, and it doubles the apparent light at night *(was 0952)*
- [ ] The reference's road tool is a decal along a spline *(was 0953)*
