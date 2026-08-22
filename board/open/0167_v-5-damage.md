Type: feature
Area: generators
Tags: scope
Depends: 1118, 1119

**V.5 Damage**

Every item below is its own task.

**Acceptance, shared by every child**: done = a render case exists in `test/khronos/glTF/` for this type, cites its `board:NNNN`, and is within the picture bound (`CLAUDE.md`).

**Cost of the full sweep**: about 858 x 8.17 s of Blender, roughly two hours, so the corpus is built once and cached rather than re-rendered.

**Retention**: after validation both `outshine.raw` and `oracle.raw` are deleted; `oracle.exr` and the two PNGs are kept. About 1.4 MB a case against 25 GB today.


---

## Folded children (2026-08-22)

- [ ] Collision damage multiplier and body deformation *(was 1063)*
- [ ] Panel detachment *(was 1064)*
- [ ] Glass cracking and shattering *(was 1065)*
- [ ] Engine damage, smoke, fire *(was 1066)*
- [ ] Fuel or battery leak *(was 1067)*
- [ ] Light breakage *(was 1068)*
- [ ] Deformation reflected in the collision shape, not only in the mesh *(was 1069)*
