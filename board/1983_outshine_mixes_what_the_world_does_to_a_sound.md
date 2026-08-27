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

- [x] distance attenuation follows the DECLARED model, not one hardcoded curve -- all three Web
      Audio models, each against the formula that defines it.
      proof: outshine/audio
- [x] Doppler from the relative velocity of source and listener, measured against the classical
      ratio at exactly c/10 so that a sign error lands visibly on 9/10 -- which is what it caught.
      proof: outshine/audio
- [x] occlusion: a source behind geometry is quieter AND duller, by a declared gain and a declared
      cutoff -- Unreal's USoundAttenuation carries exactly that pair -- and the ray that measures
      it is `TriangleBvh::Occludes`, which the shadow path already had.
      proof: outshine/audio
- [x] a BUS carries the room, and a source SENDS to it -- Schroeder combs and allpasses, with the
      feedback inverted from the DECLARED RT60. proof: outshine/audio
- [ ] ray-traced early reflections, which both engines keep OUTSIDE the base engine -- which is the shape both engines use:
      Unreal's AAudioVolume plus a reverb submix effect, RAGE's environment groups per space.
      Reflection is a property of the SPACE and not a computation per source, and ray-traced
      early reflections are a PLUGIN in Unreal (Steam Audio, Resonance) rather than the base
      engine -- so they are a later item and not this one
- [ ] the mix is handed to the client the way a frame is -- outshine fills a buffer, the client
      owns the device, because the client owns the process
