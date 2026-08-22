Type: feature
Area: generators
Tags: scope
Depends: 1118, 1119

**IV.14 Aviation and ports**

---
## Band V — Vehicles
*GTA 5 names the construction: a vehicle is a hull on wheels with suspension, tyre grip and a torque
curve; a human is a capsule whose locomotion the animation leads. The field groups below follow RAGE's
own `handling.meta` division — mass and aero, drivetrain, brakes and steering, traction, suspension,
damage — because it is the published enumeration of what a driveable body needs. **Nothing in this band
exists.** It depends on I.12 in full.*

**Acceptance, shared by every child**: done = a render case exists in `test/khronos/glTF/` for this type, cites its `board:NNNN`, and is within the picture bound (`CLAUDE.md`).

**Cost of the full sweep**: about 858 x 8.17 s of Blender, roughly two hours, so the corpus is built once and cached rather than re-rendered.

**Retention**: after validation both `outshine.raw` and `oracle.raw` are deleted; `oracle.exr` and the two PNGs are kept. About 1.4 MB a case against 25 GB today.


---

## Folded children (2026-08-22)

- [ ] Runway with threshold markings, centre line, touchdown zone *(was 0716)*
- [ ] Taxiway with its centre line and edge lights *(was 0717)*
- [ ] Apron and stands *(was 0718)*
- [ ] Approach lighting *(was 0719)*
- [ ] Windsock *(was 0720)*
- [ ] Control tower *(was 0721)*
- [ ] Helipad marking *(was 0722)*
- [ ] Quay crane *(was 0723)*
- [ ] Container stacks *(was 0724)*
- [ ] Ro-ro ramp *(was 0725)*
- [ ] Marina pontoons *(was 0726)*
