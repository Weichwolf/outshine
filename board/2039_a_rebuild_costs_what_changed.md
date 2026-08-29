Type: feature
State: open
Area: engine, world
Tags: measured

# A rebuild costs what changed

**Benchmark** — Unreal: a streamed level's primitives are added to and removed from the scene one
at a time, and `FGPUScene` uploads only the instances whose transform or payload moved; nothing
re-uploads a whole world because one cell arrived. RAGE: the same, with the streaming module
adding and evicting drawables per map sector while the rest of the scene stands untouched.
**They agree**, so the matter is closed: a change costs what it changed.

MEASURED on Shibuya, one place, one load:

    times the world was built WHOLE            2 rebuilds
    the last rebuild took                  19239 ms
    of that, buildings, streets and water  12309 ms
    and the device taking the streams       5504 ms
    megabytes carried                       2468 MB

A guard already stops a rebuild that would change NOTHING -- `World.EverLaid && !elsewhere &&
!grew` -- and that guard is why a standing camera pays nothing per frame. What it does not do is
make the rebuild PROPORTIONAL: one tile arriving re-meshes every resident tile, re-packs every
stream and re-uploads 2468 MB.

**THIS IS THE LAST OPEN PIECE OF THE FORM GOAL**, and the three before it are done: no `Gltf::` in
`src/render/`, the device layout declared once in `RunsOf` with a `static_assert`, and every
generator writing that layout already -- `Geometry::Held::Piece` holds float per channel per part,
which is what the device binds, and `SubjectStream` writes it into SDL's mapping with no buffer of
ours in between.

WHAT IT WILL TAKE, and it is a DESIGN rather than a tweak:

- A part per TILE rather than one part per kind over all tiles, so a tile that changed is a part
  that changed and the rest are untouched.
- Device buffers that can take a REGION rather than a whole stream. `SubjectResidency::Cross`
  sizes and uploads a stream entire; a partial upload needs the buffer to outlive the change and
  the copy pass to name an offset. SDL3 already carries that -- `SDL_UploadToGPUBuffer` takes a
  `SDL_GPUBufferRegion` -- so this is our bookkeeping, not a driver limit.
- An eviction rule, because a ring that only grows is a leak with a view.

- [ ] A tile arriving re-meshes that tile and no other, measured as parts rebuilt over parts standing
- [ ] The upload moves the bytes of the parts that changed, measured against the 2468 MB whole
- [ ] A tile leaving the ring frees what it held, and the count of resident parts is bounded
      proof: the corpora cannot see this -- it is a rate and a residency, so it is BOUNDED by
      `apps/bench` and stated here, never turned into a green tick
