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

## Comments

**The mechanism is built and its first numbers are meaningless, which is the guard working.** Attachment,
shader locals, readback, comparison and exclusion predicate all landed and are green. The p50 angles came
out **179.45°** on `normal-tangent-mirror`, **107.32°** on `water-bottle`, **100.76°** on `boom-box` — and
**a single sign error cannot produce all three.** Offset varying per case is the signature of a
**placement transform**: our leg is in the anchor-relative world frame the studio places the subject in,
Cycles' is in the glTF frame.

**The frame map was validated on one leg only.** *Blender `(x,y,z)` → glTF `(x, z, −y)`, unit length to
one f32 ulp, 0.3174° residual* was quoted in every brief on this item as though it covered both sides. It
covered **Cycles**. Our own leg's frame was never checked, and the rule this item was written under —
*a frame error and a shading-normal defect look identical* — caught a frame error.

**Negating anything to make the angles small would be fitting a frame to a number.** The repair is to map
our leg through the subject's own `EcefFromGltf` placement: a transform that exists in the tree and is
derivable rather than guessable.

**The exclusion predicate is verified by the cases that have nothing to compare.** `coverage/sphere` and
`coverage/cube` are wholly emissive: **0 pixels compared, 46 134 and 97 465 excluded by zero length**, not
by an angular threshold. Without it they would have reported a clean 90° over 143 599 pixels, which would
have read as a finding.

**Remaining:** our leg through the placement, then re-read the three legs and name the branch. Everything
else is done and verified.

**Our leg is mapped and validated the way the oracle's was.** The map is the inverse of the permutation
the upload already applies — `GltfStudio::EcefFromGltf` writes `ecef = (gltf.y, gltf.x, −gltf.z)`, so the
inverse is `gltf = (ecef.y, ecef.x, −ecef.z)`: a signed permutation of determinant +1 with **no free
parameter to tune**, derived from the code that created the discrepancy rather than fitted to the result.
**What says it is right is that p50 lands at 0.00099°** — three orders below the 0.3174° texture residual
and at the numeric floor. *A fitted frame does not produce a floor; it produces a smaller average.*

**The effect is measured rather than inferred**, and two independent routes agree: p95 **9.48°** measured
directly, against 4.2°–10.3° predicted from a 2.33 px centroid displacement on a 33.4 px dome. **Quote the
measurement from here, not the inference.**

| case | p50 | p95 | max |
|---|---|---|---|
| `normal-tangent-mirror` | 0.00099° | **9.48°** | 22.96° |
| `normal-tangent` | 0.00099° | 5.02° | 22.82° |
| `water-bottle` | 0.0129° | **0.064°** | 72.50° |
| `boom-box` | 0.0124° | 0.27° | 83.01° |

**The obvious reading is refuted: `water-bottle` carries a normal map and agrees at p95 0.064°**, so this
is not *normal maps in general*. It is concentrated on the two assets Khronos built for tangent
handedness — while the earlier per-column measurement found the **tangent-free** Geometry column the
worst. **Those two facts do not yet resolve into one mechanism**, and naming a branch from two legs would
be picking the louder defect.

**Remaining, and it is one step: rasterise the file's declared `NORMAL` as the third leg.** Ours and
Cycles now agree at the floor over most of the surface; **which is right where they differ is what the
file decides**, and that leg exists only in aggregate today.
