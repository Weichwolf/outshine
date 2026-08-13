Type: bug
Area: corpus
Tags: oracle, perf, instrument

**Five camera manifests aim at a point their own stated derivation does not produce — **Band 2****

`test/render/coverage/{cube,index-widths,sphere,matrix-node,trs-hierarchy}/manifest.json`,
`scene.camera.lookAtM`. Each states its derivation as *"the framing rule of `board/`
I.26.10 applied to this subject's own bounds"*, and § I.26.10 aims at the bounds' centre. The declared
aim is not that point.

| case | subject bounds centre | declared `lookAtM` | offset |
|---|---|---|---|
| `cube` · `index-widths` | origin (`halfExtentM 1.0`) | `(0.00186938763, 0.000549409433, −0.00301839697)` | **3.5927e-3 m** |
| `sphere` | origin (`radiusM 1.0`) | the same triple | the same |
| `matrix-node` · `trs-hierarchy` | not the origin — a nested chain | `(1.06417013, 0.625490847, −0.00269665869)` | the same tail on `z` |

**Measured structure, and it is what makes this a defect rather than a rounding artefact**: the offset is
a **pure image-plane displacement** — its dot product with Forward is exactly 0 — in the **same direction
in the camera basis** across all eight cases that carried it (`0.809724 · Right + 0.586811 · Up` under
the declared roll), with world magnitude proportional to the subject's distance, so **the pixel value is
identical to nine digits: 0.435660418 px**. A quantity that is constant in pixels across subjects at
different scales was applied in pixels, once, by something.

**Nothing has been found that produces it.** It first appears **hand-written at `c5275c1`**, a commit that
added no camera-generating script, and none has existed since. It is not a float32 round trip of the
centre.

**The harmless explanations, sought.** *It is the framing rule's own output* — no: the rule aims at the
bounds' centre and these are not it, and the three cases constructed at `8f0ecce` carry either the centre
or an aim § I.26.14 derives. *It is too small to matter* — 0.4357 px is **87× the oracle's 0.005 px filter
half-width** and these are coverage cases whose acceptance is a sub-pixel distance to an edge. *It is
harmless because it is consistent* — consistency is what makes it a rule somebody applied, which is
exactly the thing that must have an origin.

**Note the instrument, because it decides how this is found again**: `grep` for `0.435660418` over the
tree returns **nothing**. The number is not a literal anywhere; it is a **derived** property of the
declared `lookAtM`, so only computing it from the manifests finds it. That is the same lesson as
`board/` § I.25.1's *a grep proves a string is absent, never that a capability is*, reaching
a number instead of a feature.

**Right:** the aim is the bounds' centre, as the derivation says, or the offset carries a derivation of
its own — `derived`, `measured` or `[SET]` per `CLAUDE.md`. **Fixed when** every `lookAtM` in the suite
either equals its subject's bounds centre or names why it does not. **Decides it:** recomputing the aim
from the declared bounds and refusing a mismatch, in the runner that already recomputes the margin.

*Not a defect, and recorded here so the same investigation is not run twice: the clip range's origin
**was** found. `blender --factory-startup` reports `clip_start = 0.10000000149011612` and
`clip_end = 100.0` — Blender's factory camera, the same source those manifests already cite for the lens.*
