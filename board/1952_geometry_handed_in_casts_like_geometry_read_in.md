Type: bug
State: active
Parent: 1949
Area: door, render
Tags: measured

# Geometry handed in through the door casts a shadow, as geometry read from a file does

`Engine::Stands(const Geometry &)` stands a subject and DRAWS it -- `batches the picture draws`
reports it and `ScoreWhatAClientHandsIn` matches a file-read subject pixel for pixel. It does not
CAST: measured, `batches the shadow casts` is **0** for a subject handed in, where a declared
asset of the same shape casts every batch it draws.

So the door's two producers agree about the colour pass and disagree about the depth the light
sees, which is exactly the asymmetry board:1949 exists to remove.

## What this blocked

board:1951's proving case wanted one receiver and one caster under one key, differing only in
whether something stands over the ground. Handing them in was the natural way to write it and it
measured nothing, because nothing cast.

Written down so the next attempt does not repeat five of them: a hand-written glTF fixture was
refused four times before it loaded, and each refusal was correct.

    interleaved positions and normals, byteStride 24, views spanning only vertices*12
        "POSITION does not decode" -- a strided view must span vertices*stride
    blocks of twelve positions then twelve normals under a declared stride of 24
        the same refusal -- the accessors and the bytes disagreed
    an index accessor over a non-indexed layout
        the same
    `min`/`max` declared by hand as [-12,-12,0]..[12,12,3]
        the wall-only document reaches z=0, so the bounds did not bound its positions

The bounds are MEASURED from the positions now, in the fixture that finally loaded, and that is
the general lesson: a fixture that states a fact about its own bytes states it twice.

## What will be true

- [x] A subject handed in through `Engine::Stands` casts every batch it draws, as a declared
      asset does.
      proof: harness/outshine/door/ScoreWhatHandedGeometryCasts
- [x] Proving case: a subject handed in draws N and casts N, needing no file at all -- the claim
      is about the door's own producer. Negative control: the plan outliving its declaration
      again, and the same subject draws 1 and casts 0.
      proof: harness/outshine/door/ScoreWhatHandedGeometryCasts
