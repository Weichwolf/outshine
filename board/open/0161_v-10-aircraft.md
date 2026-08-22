Type: feature
Area: generators
Tags: scope
Depends: 1118, 1119

**V.10 Aircraft**

Every item below is its own task.

**Acceptance, shared by every child**: done = a render case exists in `test/khronos/glTF/` for this type, cites its `board:NNNN`, and is within the picture bound (`CLAUDE.md`).

**Cost of the full sweep**: about 858 x 8.17 s of Blender, roughly two hours, so the corpus is built once and cached rather than re-rendered.

**Retention**: after validation both `outshine.raw` and `oracle.raw` are deleted; `oracle.exr` and the two PNGs are kept. About 1.4 MB a case against 25 GB today.


---

## Folded children (2026-08-22)

- [ ] Lift and drag as coefficients over angle of attack, not a table lookup *(was 0984)*
- [ ] Stall and its recovery *(was 0985)*
- [ ] Control surfaces: elevator, aileron, rudder, flaps, airbrake *(was 0986)*
- [ ] Propeller or turbofan thrust *(was 0987)*
- [ ] Landing gear with suspension and a ground handling model *(was 0988)*
- [ ] Ground effect *(was 0989)*
- [ ] Rotor thrust, cyclic and collective, and the torque a tail rotor answers *(was 0990)*
- [ ] Autorotation *(was 0991)*
- [ ] Classes: light aircraft, airliner, cargo, glider, helicopter, drone *(was 0992)*
- [ ] Drone as the everyday post-scarcity aircraft, and it is the one the camera can follow anywhere *(was 0993)*
- [ ] Wind field from the weather provider driving all of the above *(was 0994)*
