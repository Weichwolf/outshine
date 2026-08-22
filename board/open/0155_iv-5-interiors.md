Type: feature
Area: generators
Tags: scope
Depends: 1118, 1119

**IV.5 Interiors**

Every item below is its own task.

**Acceptance, shared by every child**: done = a render case exists in `test/khronos/glTF/` for this type, cites its `board:NNNN`, and is within the picture bound (`CLAUDE.md`).

**Cost of the full sweep**: about 858 x 8.17 s of Blender, roughly two hours, so the corpus is built once and cached rather than re-rendered.

**Retention**: after validation both `outshine.raw` and `oracle.raw` are deleted; `oracle.exr` and the two PNGs are kept. About 1.4 MB a case against 25 GB today.


---

## Folded children (2026-08-22)

- [ ] A dark room box behind the glass, so a window is not a hole into the world *(was 0819)*
- [ ] Interior wall plane at a declared depth, lit only by what comes through the window *(was 0820)*
- [ ] Lit interior at night with a per-room duty cycle *(was 0821)*
- [ ] Curtain or blind plane *(was 0822)*
- [ ] Enterable ground-floor shop *(was 0823)*
- [ ] Enterable dwelling: hall, room, stair *(was 0824)*
- [ ] Stair core and lift shaft as geometry *(was 0825)*
- [ ] Floor plan generated from the footprint and the storey count *(was 0826)*
- [ ] Furniture as declared bodies *(was 0827)*
- [ ] Interior lighting as a light list contribution *(was 0828)*
- [ ] Portal or occlusion boundary at a door, so an interior does not cost the exterior *(was 0829)*
- [ ] Basement and cellar *(was 0830)*
- [ ] Loft space under a pitched roof *(was 0831)*
- [ ] GTA 5's hand-modelled interiors *(was 0832)*
