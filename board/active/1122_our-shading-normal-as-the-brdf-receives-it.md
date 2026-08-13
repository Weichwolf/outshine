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

**There is no single value to read back, and that is the finding rather than an obstacle.** Measured
across the fifteen fragment entry points: the **lit** arm passes `facing(in.n, front)` inline at **6**
call sites, the **mapped** arm passes `mappedNormal(...)` at **1**, and the emissive entry points compute
no normal at all. None is stored. So *the normal the BRDF received* is two expressions over seven sites,
and `SubjectDraw.cpp:529` is the mapped arm only — the lit arm is the more common path.

**The repair is `board:1121`'s shape one level in: the normal becomes a named local at each arm**, bound
once, passed to `shadeRow` and written to the attachment from that same local — so the two are the same
value by construction rather than by inspection. **Recomputing it in the fragment to write it out is the
reconstruction this item forbids**, and with two expressions in play it is likelier wrong than right.

**Two points settled while sizing.** The `Contributes` entry goes on `Subjects` **alone** — only its
shader can write it, and a stage contributing a target its shader does not write *is* the `board:1121`
defect; pass merging requires identical `Contributes` sets, so this is safe by construction. And **the
colour index is spliced, never fixed**: attachment order follows `Contributes` through the prune, so the
normal is `color(2)` when velocity is attached and `color(1)` when it is pruned. Capacity is fine —
`kMaxEdges` is 8 and the geometry rows use 3.

**Its verification reads the corpus's `normal` channel**, which is already there: the channel trim landed
in an earlier commit and `QUANTITY_PASSES` carries the three. Two rounds carried *still outstanding* for
it after it was committed — a claim about the tree that nobody re-checked, which is the same shape as a
catalogue row outliving its implementation.

**Done when** all three legs are published per pixel — the file's declared `NORMAL`, Cycles mapped into
glTF metres, and ours at the shading point — and the branch is named: engine fix if Cycles matches the
file and we do not; *reduce the oracle* or *patch the asset* if we match the file and Cycles does not.
