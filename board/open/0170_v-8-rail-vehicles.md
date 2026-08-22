Type: feature
Area: generators
Tags: scope
Depends: 1118, 1119

**V.8 Rail vehicles**

Every item below is its own task.

**Acceptance, shared by every child**: done = a render case exists in `test/khronos/glTF/` for this type, cites its `board:NNNN`, and is within the picture bound (`CLAUDE.md`).

**Cost of the full sweep**: about 858 x 8.17 s of Blender, roughly two hours, so the corpus is built once and cached rather than re-rendered.

**Retention**: after validation both `outshine.raw` and `oracle.raw` are deleted; `oracle.exr` and the two PNGs are kept. About 1.4 MB a case against 25 GB today.


---

## Folded children (2026-08-22)

- [ ] Constraint to the rail rather than free contact *(was 1100)*
- [ ] Bogies, and a long body articulating over them *(was 1101)*
- [ ] Coupling between units *(was 1102)*
- [ ] Pantograph contact with the catenary *(was 1103)*
- [ ] Doors, and a stop at a platform *(was 1104)*
- [ ] Freight wagon types: flat, hopper, tank, container *(was 1105)*
- [ ] Tram in a road surface, sharing it with traffic *(was 1106)*
