Type: bug
Area: corpus
Tags: oracle, instrument

**The oracle's importer reads a whole rate, so a finer grid asks for more frames**

A case that needs an instant a whole frame rate cannot reach declares MORE FRAMES, which is this
corpus's own currency. The scene's rate stays a whole number because that is what the oracle's glTF
importer reads.

## What was tried and why it looked right

Blender states a frame rate as a PAIR -- `fps` whole and `fps_base` dividing it -- so a declared 0.25
looks like `fps = 1, fps_base = 4.0`, and the generator's animation groups key over spans no whole rate
can sample inside. The schema was widened to a number, the preparer built the pair, and 23 refused
models started preparing.

## Why it is wrong, measured twice

**Blender's glTF importer converts a sampler's seconds with `scene.render.fps` and never divides by
`fps_base`.** Every key therefore lands at the wrong instant.

| what it broke | how it said so |
|---|---|
| `AnimatedMorphCube`, a case that had passed for months | *the importer's first key at 0.0012839989503845572 and the same key derived from the file is 0.0* |
| the derived camera's own bounds walk | one pose reported at EVERY instant, because every key sat outside the interval being sampled |

**The second is the one that cost a round.** `board:1467` was filed against the near plane and its three
cases were narrowed through `view_layer.update()`, the depsgraph, the frame conversion and the pose
semantics -- and the cause was that the keys were never where the walk looked. *A wrong clock does not
announce itself; it makes everything downstream look almost right.*

## What it is

`set_frame_grid` refuses a fractional rate by name and says what to do instead. `fps` is an integer in
the schema again, and its note carries this reason rather than the one it had. The generator's importer
derives **one frame a second and as many frames as it takes to reach the middle of the keyed span** --
so a model keyed from 2 s to 6 s declares four frames and is decided at 0 s and 3 s.

## Comments

The pair is not wrong in general and Blender uses it for NTSC rates; what is wrong is assuming an
importer honours it. **A rational rate the oracle cannot read is a declaration this corpus may not
make**, and the schema now says so where somebody choosing a grid will read it.
