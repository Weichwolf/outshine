Type: feature
Area: generators
Tags: scope
Depends: 1118, 1119

**IV.1 Data prerequisites**

Every item below is its own task.

**Acceptance, shared by every child**: done = a render case exists in `test/khronos/glTF/` for this type, cites its `board:NNNN`, and is within the picture bound (`CLAUDE.md`).

**Cost of the full sweep**: about 858 x 8.17 s of Blender, roughly two hours, so the corpus is built once and cached rather than re-rendered.

**Retention**: after validation both `outshine.raw` and `oracle.raw` are deleted; `oracle.exr` and the two PNGs are kept. About 1.4 MB a case against 25 GB today.


---

## Folded children (2026-08-22)

- [ ] Building footprints from the served vector tiles, kept as rings with an index rather than re-parsed *(was 0631)*
- [ ] A `height` attribute where the provider carries one *(was 0632)*
- [ ] The provider's 5.0 m fill detected and named rather than trusted (`kFillHeightM`, 1634 of the Hameln tile) *(was 0633)*
- [ ] Base elevation per footprint from the ring's own lowest corner *(was 0634)*
- [ ] One base per building *(was 0635)*
- [ ] `building:levels` *(was 0636)*
- [ ] `roof:shape`, `roof:levels`, `roof:material`, `building:material`, `building:colour` *(was 0638)*
- [ ] Address and house number *(was 0639)*
- [ ] Storey count inferred from height when no level count is served, with the inference stated *(was 0640)*
- [ ] Building age or period inferred, which the epoch dial needs *(was 0641)*
