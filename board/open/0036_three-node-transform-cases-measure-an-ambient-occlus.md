Type: bug
Area: render
Tags: oracle, perf, instrument

**Three node-transform cases measure an ambient-occlusion estimator at one sample and report the answer as a placement**

`test/render/coverage/trs-hierarchy/manifest.json`, `matrix-node/`, `sphere/` — the material block,
`"kind": "diffuse"`, with `samples: 1` and `bounces.diffuse: 0`. Measured at `124504a`:
**`trs-hierarchy` vs `matrix-node` differ by 5 899 px, of which 5 896 are colour-only**, in the contact
regions where their three cubes touch; `sphere` differs by 62, of which **60** are the same thing from
shading-normal self-occlusion.

**The mechanism, and it is exactly diagnosable rather than inferred.** The oracle's departures from
`ρ·L` are **binary — `ρ·L` or exactly 0, never between**. At 1 spp with `diffuse_bounces = 0` a pixel
takes one cosine-weighted direction; it escapes to the environment and returns `ρ·L`, or it meets
geometry and returns 0. The pixel is a **Bernoulli draw whose mean is the visible sky fraction**, so
these cases carry an ambient-occlusion integral that § I.26.13's four reductions do not remove and that
no seed makes deterministic.

Each manifest states the material is not read — *"Nothing in a coverage comparison reads it; it is here
so the picture a person opens is not black"* — and the comparison reads it. That is the defect: a case
whose declaration and whose acceptance disagree about what it measures.

**The harmless explanations, sought.** *It is noise and belongs under a tolerance* — no: 5 896 px is a
contact **region**, not a boundary, and § I.26.13's own rule is that a non-reducing oracle is lowered and
never accommodated. *It is a difference between our renderer and Cycles* — no: `trs-hierarchy` and
`matrix-node` are compared **against each other**, and the same subject placed two ways cannot differ in
its geometry; the 5 896 are two independent draws of one estimator. *The three colour-free pixels are the
real finding* — yes, and they are the only part of this measurement that is about node transforms.

**Right:** the material becomes `emission` (`board/` § I.26.13), one colour per node where a
case has more than one, geometry untouched. **Not** separating the cubes — that repairs the oracle by
changing the subject, and it cannot repair `sphere` at all. **Fixed when** two renders at two seeds are
bit-identical for these three cases, which is a stronger statement than the pixel count falling.
