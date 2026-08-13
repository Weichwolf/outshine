Type: task
Area: render
Parent: 0104
Depends: 1121
Tags: oracle, instrument

**Our shading normal, as the BRDF receives it**

The third leg of the normal comparison. Cycles' Normal pass and the file's own `NORMAL` accessor are
both readable; ours is not, so the disagreement is inferred from where the highlight landed rather than
measured.

Established: the specular centroid is displaced in **20 of 20** cells of `normal-tangent-mirror`, mean
**2.33 px** on a 33.4 px dome against a 7–9 px core, and the shift **rotates with the cell including in
the tangent-free Geometry column** — so it tracks the local normal and is not one global light-direction
error. The implied disagreement is **4.2°–10.3°**, which is structural rather than precision. Fresnel and
multiscatter GGX are both refuted by a pre-registered discriminator: the residual changes sign inside the
disc at the highlight, and the rim is flat.

The frame map is validated — Blender `(x,y,z)` → glTF `(x, z, −y)`, unit length preserved to **one f32
ulp**, residual tilt **0.3174°** derived from the normal texture's own 8-bit quantisation, which is
13×–32× below the effect.

**The value must come from the shading point, not a reconstruction.** It is the return of
`mappedNormal(...)` passed inline at `src/render/stages/SubjectDraw.cpp:529` and never stored; a
recomputed normal can agree with Cycles perfectly while the shader uses something else. Reading it needs
a fourth colour attachment, which is why this depends on the plan pruning.

**Done when** all three legs are published per pixel — the file's declared `NORMAL`, Cycles mapped into
glTF metres, and ours at the shading point — and the branch is named: engine fix if Cycles matches the
file and we do not; *reduce the oracle* or *patch the asset* if we match the file and Cycles does not.
