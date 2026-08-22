Type: feature
Area: generators
Tags: scope
Depends: 1118, 1119

**V.3 Wheels, suspension, tyres**

Every item below is its own task.

**Acceptance, shared by every child**: done = a render case exists in `test/khronos/glTF/` for this type, cites its `board:NNNN`, and is within the picture bound (`CLAUDE.md`).

**Cost of the full sweep**: about 858 x 8.17 s of Blender, roughly two hours, so the corpus is built once and cached rather than re-rendered.

**Retention**: after validation both `outshine.raw` and `oracle.raw` are deleted; `oracle.exr` and the two PNGs are kept. About 1.4 MB a case against 25 GB today.


---

## Folded children (2026-08-22)

- [ ] Wheel as a body with a hub, a rim and a tyre *(was 1037)*
- [ ] Wheel raycast or shape cast against the drawn terrain *(was 1038)*
- [ ] Suspension: spring force, compression damping, rebound damping, upper and lower travel limits, raise *(was 1039)*
- [ ] Anti-roll bar *(was 1040)*
- [ ] Roll centre heights, front and rear *(was 1041)*
- [ ] Suspension bias front to rear *(was 1042)*
- [ ] Tyre longitudinal and lateral force curves, maximum and minimum *(was 1043)*
- [ ] Traction spring delta and low-speed loss *(was 1044)*
- [ ] Camber stiffness *(was 1045)*
- [ ] Traction bias front to rear *(was 1046)*
- [ ] Surface grip multiplier per contact material *(was 1047)*
- [ ] Tyre deformation and burst, with the rim then running on the road *(was 1048)*
- [ ] Wheel spin, lock-up and the marks they leave *(was 1049)*
- [ ] Steering geometry: lock angle, Ackermann, self-centring *(was 1050)*
- [ ] Ride height change under load *(was 1051)*
