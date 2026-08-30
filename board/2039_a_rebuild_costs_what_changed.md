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

## HOW THIS IS MEASURED, so the next reading is the same reading

`test/outshine/places` is the instrument. It stands a real place, streams it, draws it and writes
the picture to `build/places/<Name>.png`, printing what each phase cost beside what the world
holds -- so every number here has a picture next to it that can be looked at.

    sh test/run.sh outshine/places          all six, which is what the gate does
    $NEST/outshine-places-RenderShibuya     ONE place, which is what a measurement wants

`$NEST` is `${TMPDIR}/outshine-tests.<checkout hash>`, where `run.sh` builds. Run a single place
directly when measuring: the other five cost minutes and answer a different question. **Always
under a wall clock** -- `perl -e 'alarm 200; exec @ARGV' <binary>` -- because a world that does not
settle waits out the patience rather than failing.

The lines that matter are `THE TIME IS NOT THE PICTURE` (standing, waiting, and the per-frame draw)
and the `rebuild:` block. A run that reports tiles standing BARE is a reading about the STREAMING
and not about the place, and the case refuses it rather than scoring it.

MEASURED on Shibuya, warm cache, and this reproduces the reading below to the megabyte:

    waiting for the world                  22260 ms      the target is under 10 000
    the frame                              22.65 ms      the target is 16.7 or better
    the last rebuild                       18837 ms
      of that, buildings streets water     11940 ms
      walking it into the proxy             6428 ms
      the device taking the streams         4330 ms
      resolving its surface                  632 ms
    megabytes carried                       2468 MB
    what it holds     88 tiles over 7 levels, 9 433 960 triangles, 8 789 636 of them buildings,
                      529 798 footprints, 79 674 streets

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

**RE-MEASURED, AND THE FIRST READING WAS THE WRONG ONE.** "2 rebuilds" reads like a world built
twice; instrumenting the TRIGGER says otherwise:

    rebuild: the eye walked into another tile      0 yes/no
    rebuild: tiles resident when it did          112 tiles
    rebuild: and resident the time before          0 tiles

Both rebuilds come from `grew`, not from movement, and the last one took the world from 0 to 112
tiles -- that is the ONE real build. The other is the empty one at stand-up. **A standing place
builds its world once, and this item buys it nothing.** `preload` calls `Grounds` once at settle
rather than per arrival, which board:1941 already fixed.

So the cost this item names is real but it is the DRIVE's, not the place's: the eye crossing into
another tile sets `elsewhere` and re-meshes the whole ring, which is a 19 s stall in a world that
is supposed to be moving. **The places cannot see it and there is no instrument here that can.**
Writing the optimisation before the instrument would be optimising against a guess, which is what
this page refuses everywhere else.

- [ ] An instrument that MOVES: a camera walking a city until it crosses tile borders, reporting
      the stall at each crossing. `apps/bench` drives and is the place for it -- and a rate is
      BOUNDED there rather than turned into a tick
- [ ] Only then the cut below, and each line of it quotes the number it beat

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
