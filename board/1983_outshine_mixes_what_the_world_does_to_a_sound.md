Type: feature
State: open
Parent: 1982
Area: audio
Progress: audio

# outshine mixes what the world does to a sound

**Benchmark** — Unreal: `USoundAttenuation` declares the distance curve, the spatialisation
method, air absorption, occlusion (by trace) and focus; reverb comes from `AAudioVolume` and
submix effects; Doppler is a component setting on the source. RAGE: environment groups carry
occlusion and reverb per space, and `audSound` carries Doppler. **Both agree** — what the WORLD
does to a sound is DECLARED data, never code per sound, and it is the engine that applies it.

The engine already holds what this needs and reaches none of it: a `BusGraph` with routing and a
distance falloff, a store that knows where every entity is, bodies with velocities, and ground and
structures to occlude and reflect against.

- [ ] distance attenuation follows the DECLARED model, not one hardcoded curve
- [ ] Doppler from the relative velocity of source and listener, and the shift is a measurement
      against the closed form rather than a feeling
- [ ] occlusion: a source behind geometry is quieter and duller, by a ray the world answers
- [ ] early reflections off the nearest surfaces, and the room they imply
- [ ] the mix is handed to the client the way a frame is -- outshine fills a buffer, the client
      owns the device, because the client owns the process
