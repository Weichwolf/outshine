Type: feature
Area: generators
Tags: scope
Depends: 1118, 1119

**V.2 Mass, hull and aerodynamics**

Every item below is its own task.

**Acceptance, shared by every child**: done = a render case exists in `test/khronos/glTF/` for this type, cites its `board:NNNN`, and is within the picture bound (`CLAUDE.md`).

**Cost of the full sweep**: about 858 x 8.17 s of Blender, roughly two hours, so the corpus is built once and cached rather than re-rendered.

**Retention**: after validation both `outshine.raw` and `oracle.raw` are deleted; `oracle.exr` and the two PNGs are kept. About 1.4 MB a case against 25 GB today.


---

## Folded children (2026-08-22)

- [ ] Mass, centre-of-mass offset, inertia multiplier *(was 1024)*
- [ ] Hull as a collision shape distinct from the drawn mesh *(was 1025)*
- [ ] Drag coefficient and frontal area *(was 1026)*
- [ ] Downforce *(was 1027)*
- [ ] Submersion depth at which the engine cuts *(was 1028)*
- [ ] Buoyancy volume and centre, so a car sinks and a boat does not *(was 1029)*
- [ ] Body panels as sub-bodies: doors, bonnet, boot, hatch, with hinges and limits *(was 1030)*
- [ ] Glass panes as breakable elements *(was 1031)*
- [ ] Number plate *(was 1032)*
- [ ] Paint as a material row: base colour, clear coat, metallic flake, and it needs no texture *(was 1033)*
- [ ] Livery and decals *(was 1034)*
- [ ] Dirt accumulation as a function of use and weather *(was 1035)*
- [ ] Rust and wear *(was 1036)*
