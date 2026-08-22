Type: feature
Area: generators
Tags: scope
Depends: 1118, 1119

**V.4 Drivetrain, brakes, controls**

Every item below is its own task.

**Acceptance, shared by every child**: done = a render case exists in `test/khronos/glTF/` for this type, cites its `board:NNNN`, and is within the picture bound (`CLAUDE.md`).

**Cost of the full sweep**: about 858 x 8.17 s of Blender, roughly two hours, so the corpus is built once and cached rather than re-rendered.

**Retention**: after validation both `outshine.raw` and `oracle.raw` are deleted; `oracle.exr` and the two PNGs are kept. About 1.4 MB a case against 25 GB today.


---

## Folded children (2026-08-22)

- [ ] Drive bias: front, rear, all *(was 1052)*
- [ ] Gear count and ratios, final drive *(was 1053)*
- [ ] Drive force and drive inertia *(was 1054)*
- [ ] Clutch engage and shift rates *(was 1055)*
- [ ] Top speed limiter *(was 1056)*
- [ ] Reverse *(was 1057)*
- [ ] Electric drive with a single ratio and instant torque *(was 1058)*
- [ ] Brake force, brake bias, handbrake *(was 1059)*
- [ ] Anti-lock behaviour *(was 1060)*
- [ ] Throttle, brake, steer as the only inputs a brain or a player reaches *(was 1061)*
- [ ] Cruise and speed limiter *(was 1062)*
