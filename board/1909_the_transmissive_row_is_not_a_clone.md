Type: bug
State: open
Area: render, architecture
Tags: measured, map, review

# A red on the map is red for a reason the lines beside it carry

CLAUDE.md's CURRENT render plan marks `GLASS` red and gives this reason:

> `{Stage::SubjectsTransmissive, ...}` (RenderCatalogue.h:268) is a full clone of
> `{Stage::Subjects, ...}` (:263) -- transmissive draws belong in the one subject stage

**The two rows are not clones and the file says so.** Read at 4e69c633:

| | reads | writes |
|---|---|---|
| `subjects` (:263) | `ShadowAtlas` | `SceneHdr`, `SceneVelocity`, `SceneDepth`, `SceneShadingNormal`, `SceneSurfaceIdentity` |
| `subjectsTransmissive` (:268) | `SceneHdr`, `LinearSampler` | `SceneTransmissive`, `SceneVelocity`, `SceneDepth` |

Different reads, different writes, and the difference is the whole point: a transmissive draw
REFRACTS what is behind it, so it samples `SceneHdr` -- the finished opaque picture -- and cannot
be a batch partition of the pass that produces it. A separate pass reading the opaque result is
what both benchmarks do; Unreal runs translucency after the base pass and samples SceneColor for
exactly this.

What IS true of the pair is that one class encodes both -- `Renderer::EncodeGlass`
(Renderer.cpp:120) drives the same `SubjectDraw`. One stage class, two passes, two targets. That
is composition, not a clone.

`TheMapCitesLinesThatSayWhatItClaims` passes on this row because the quoted TEXT is present at
the line cited. The claim ABOUT the text is what is wrong, and no walk can catch that.

## What will be true

- [ ] The `GLASS` red on CLAUDE.md's render plan states a defect the cited lines carry, or the
      node stops being red. The architecture review owns both maps and this is its correction to
      make (board:1864).
- [ ] board:1867's third box -- "transmissive draws are a batch partition of this one stage, so
      the cloned row disappears" -- rests on the same premise and moves with it.
