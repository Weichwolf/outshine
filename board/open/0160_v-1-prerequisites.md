Type: feature
Area: generators
Tags: scope
Depends: 1118, 1119

**V.1 Prerequisites**

Every item below is its own task.

**Acceptance, shared by every child**: done = a render case exists in `test/khronos/glTF/` for this type, cites its `board:NNNN`, and is within the picture bound (`CLAUDE.md`).

**Cost of the full sweep**: about 858 x 8.17 s of Blender, roughly two hours, so the corpus is built once and cached rather than re-rendered.

**Retention**: after validation both `outshine.raw` and `oracle.raw` are deleted; `oracle.exr` and the two PNGs are kept. About 1.4 MB a case against 25 GB today.


---

## Folded children (2026-08-22)

- [ ] Rigid-body dynamics (I.12) *(was 0977)*
- [ ] Declared body format carrying segments, joints, contacts, force sources and medium *(was 0978)*
- [ ] Vehicle as one declaration in that format, not a second format *(was 0979)*
- [ ] Vehicle prototype and instances, as vegetation already is *(was 0980)*
- [ ] Vehicle LOD ladder on the same one ladder *(was 0981)*
- [ ] Vehicle spawned by an actor spawner sharing the region key *(was 0982)*
- [ ] Vehicle occupancy claimed against the same sink *(was 0983)*
