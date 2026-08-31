Type: defect
State: open
Area: world, engine
Tags: determinism, measured, blocker

# The world holds the SAME content twice, and today it does not

**Benchmark** — RAGE's replay plays a drive back frame for frame, which is impossible unless the
world is the same world; Unreal's automation compares screenshots bit for bit and calls a wandering
one a STREAMING bug, which is exactly the diagnosis here. **Both agree, and `CLAUDE.md` makes it an
invariant**: the same declaration renders the same bytes, twice, on this machine.

## Measured, three runs of one place, nothing changed between them

    run 1   94 611 footprints   1 031 831 vertices   83 298 edges on more than two
    run 2   91 664               999 807             79 898
    run 3   91 664               999 807             79 898

    every run    SHOT Kaiserberg 94625c1a

**The picture's digest is identical and the world is not.** Two thousand nine hundred and
forty-seven footprints came and went between the first run and the next two, and the frame could not
tell, because the extra buildings stand outside what the camera draws. The bytes agree; the world
they were drawn from does not.

That is the invariant in its exact wording -- the same DECLARATION renders the same bytes -- passing
while the thing underneath it wanders. A digest over the frame cannot see content the frame does not
reach, so the guard that exists is blind to precisely this.

## What it blocks, which is why it is filed as a blocker

**No geometry number can be a ceiling while the content wanders.** board:2085 wants closed, manifold
and non-intersecting declared as counts that may only fall, and a case was written to hold them --
and withdrawn on this measurement, because a ceiling that moves by four per cent between runs is a
flaky gate, which is worse than no gate. The same applies to every count over the world: buildings,
streets, junctions, welded corners.

It also cost a wrong conclusion on the way, which is worth recording: a real defect was found in the
audit's weld -- it keyed a vertex by a SPATIAL HASH rather than by its coordinate, so two distinct
positions that collided were merged into one -- and the repair was credited with a drop of 1 683
edges that was in fact this variation. **The key defect is real and the repair stands on principle;
the number attached to it was this, and is withdrawn.**

## Where it most likely lives

The count differs by whole footprints, so it is INGESTION rather than meshing: how many tiles were
present when the field was built. `Deferrals` and the tile watermark already exist to make that
answerable, and the first run's larger number suggests a COLD cache reached further, not less far,
which is the opposite of what a naive "some tiles were missing" story predicts and is the part worth
understanding before anything is changed.

## What will be true

- [ ] Two runs of one place hold the same content: same footprints, same vertices, same edges. The
      numbers are printed so a reader can see they were compared rather than assumed
- [ ] The comparison is over a DECLARED extent rather than over whatever arrived, so "the world"
      means the same thing twice by construction
- [ ] Negative control: a run with a deliberately truncated tile set differs, and the check says so.
      A check that passes when the world is smaller proves nothing
- [ ] board:2085's ceilings become declarable, and the case withdrawn here is restored

## What this does NOT cover

The frame. The picture's digest has been stable throughout and is not in question; what is in
question is that a stable digest was read as evidence the world was stable, and it never was.
