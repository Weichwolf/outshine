Type: feature
Area: generators
Tags: scope
Depends: 1118, 1119

**IV.2 Mass and footprint**

Every item below is its own task.

**Acceptance, shared by every child**: done = a render case exists in `test/khronos/glTF/` for this type, cites its `board:NNNN`, and is within the picture bound (`CLAUDE.md`).

**Cost of the full sweep**: about 858 x 8.17 s of Blender, roughly two hours, so the corpus is built once and cached rather than re-rendered.

**Retention**: after validation both `outshine.raw` and `oracle.raw` are deleted; `oracle.exr` and the two PNGs are kept. About 1.4 MB a case against 25 GB today.


---

## Folded children (2026-08-22)

- [ ] Footprint extruded to a prism *(was 0727)*
- [ ] A building's LOD ladder on the one cluster DAG, with the same model-space error the vegetation ladder uses *(was 0728)*
- [ ] A far rung that drops openings, trim and roof furniture and keeps mass and roof plane, because at 320×180 those are what a silhouette is made of *(was 0729)*
- [ ] A block of buildings merged into one draw at the far rungs, the way the reference merges vegetation per cell *(was 0730)*
- [ ] An impostor rung for a distant block, its error anchored on the atlas cell texel like every other impostor *(was 0731)*
- [ ] Wall vertices carrying a façade coordinate *(was 0732)*
- [ ] Multi-part mass: a main block plus a lower wing, rather than one prism per ring *(was 0733)*
- [ ] Courtyard buildings as several rings resolved as one structure *(was 0734)*
- [ ] Terrace: a row of prisms sharing walls, recognised as a row *(was 0735)*
- [ ] Setback on an upper storey *(was 0736)*
- [ ] Overhang and cantilever *(was 0737)*
- [ ] Building on a slope: a stepped base rather than a floating or buried plinth *(was 0738)*
- [ ] Plinth and base course as a distinct band *(was 0739)*
- [ ] Party wall exposed above a lower neighbour *(was 0740)*
- [ ] Attached garage, porch, conservatory, extension *(was 0741)*
- [ ] Building contact body for physics *(was 0742)*
