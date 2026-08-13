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

**The third leg is built and validates, but does not yet adjudicate.**

| case | ours vs file p50 | Cycles vs file p50 |
|---|---|---|
| `normal-tangent-mirror` | **0.3177°** | **0.3183°** |
| `normal-tangent` | **0.3178°** | **0.3179°** |

Against the derived **0.3174°** — four significant figures, on both legs, independently. With
`ours vs Cycles p50 = 0.00099°` the construction is validated end to end: **three legs, two frames, one
named residual**, and that term is now arrived at three ways — derived from bit depth, measured on
Cycles, measured on ours.

**Two bounded steps remain, and both are defects in the third leg rather than findings about the first
two.**

**1 · The rasteriser is wrong for multi-part subjects.** `RasteriseDeclaredNormals` walks
`geometry.Indices()` as one flat list where `Attribution.h` uses `part.FirstIndex` per part.
`water-bottle` and `boom-box` return p50 ≈ 103° and 96° — and **both legs return the same wrong number**
(103.2169 against 103.2388), which is the signature of the third leg being broken rather than either
other. `normal-tangent-mirror` is `meshes=1 primitives=1`, which is why it is right there and only there.

**2 · Percentiles cannot answer *which is closer*.** At p95 both legs diverge from the file — 61.8° ours,
48.1° Cycles — and **that divergence is the normal map doing its job**, since the file's leg is the
geometric normal the map perturbs. Reading `61.8 > 48.1` as evidence would be two percentiles over
different pixel populations. **The branch needs a per-pixel statistic over the disagreeing minority**:
for each pixel where ours and Cycles differ, which is nearer the declaration.

**Until step 1 lands, the third leg cannot speak to the open tension at all** — `water-bottle` is one of
the multi-part cases it gets wrong, so *a normal-mapped asset agreeing at p95 0.064° while the
tangent-free column was worst* stays open rather than being resolved by a number that is measuring the
wrong population.

**Step 1 refuted its own diagnosis and found the real cause.** Per-part offsets changed nothing to four
decimals — all three assets are **single-part**, so multi-part was never it. The defect was a depth
ordering written as *the engine's reversed-Z, greater is nearer* against `src/gltf/Camera.h:7`, which
states the opposite in its own comment: *NDC z in [−1, +1] with −1 at the near plane… not this engine's
depth convention.* **The far surface was kept** — on a closed body, the back, whose normal points away.
The diagnosis **predicted which cases would be affected**: `normal-tangent-mirror` is an open grid with
no back to pick and was untouched; every closed body was wrong.

**They do not land on 0.3174°, and where they land is the finding.**

| case | ours vs file | Cycles vs file | apart |
|---|---|---|---|
| `normal-tangent-mirror` | 0.3177° | 0.3183° | 0.001° |
| `normal-tangent` | 0.3178° | 0.3179° | 0.001° |
| `water-bottle` | 0.9566° | 0.9530° | **0.004°** |
| `boom-box` | 0.5259° | 0.5282° | 0.002° |
| `corset` | 3.9743° | 3.9430° | 0.031° |
| `lantern` | 2.4663° | 2.4731° | 0.007° |

The two open grids land on the term because there the map's flat region dominates and the texture's
quantisation is the whole residual. **The four closed bodies sit at 0.53°–3.97°: the normal map's genuine
perturbation over a curved, coarsely tessellated surface, which 0.3174° never covered.**

**And those four answer their own branch with a fourth outcome: neither side is wrong.** Both legs sit at
the *same* distance from the declaration, agreeing with each other to within 0.004° — the offset is the
perturbation both apply identically, not a defect in either.

**The disagreement that started this is untouched by the repair**, which is the right outcome: repairing
the adjudicator did not move the thing being adjudicated. `ours vs Cycles p95 = 9.4786°` on the two
tangent assets stands.

**One step remains: the per-pixel *closer to the file* statistic over the disagreeing set, with its
population size published.** A verdict over 200 pixels and one over 200 000 are different claims.

**Step 2 is built and its number does not decide the question, because the population was chosen wrong.**

| | `normal-tangent-mirror` | `normal-tangent` |
|---|---|---|
| disputed pixels (legs differ > 0.001° floor) | 147 669 | 120 931 |
| file nearer **ours** | 45 674 (30.9 %) | 44 151 (36.5 %) |
| file nearer **Cycles** | 101 995 (69.1 %) | 76 780 (63.5 %) |
| median margin | **−0.00247°** | **−0.00148°** |

**The count says Cycles 2:1. The margin says the contest is at the floor.** A median of 0.0015°–0.0025°
is **130× below the 0.3174° texture term** every leg was validated against, and only ~2× above the
0.001° threshold the population was selected by. The question is about the **9.48° p95 tail**, and a 2:1
lean at a 0.002° median is *a verdict over a population that mostly agrees, quoted about the population
that does not.*

**The selection should have used the signal threshold, not the floor.** One line: restrict the disputed
set to `ours vs Cycles > 0.4°` — above the texture residual, so every admitted pixel carries signal —
publish the size, and the branch falls out.

**That is the last step. Everything behind it is built and validated**: the attachment · the shader
locals binding what the BRDF receives · the readback · the zero-vector exclusion predicate · both frame
maps · the adjudicating third leg with its depth convention corrected · and the disputed statistic
itself, which needs a threshold change rather than a rewrite.

**The branch is named: Cycles matches the file, we do not.** Selecting at the **signal** threshold —
0.3174° rounded up to 0.4°, the term both legs were validated against, past which a disagreement cannot
be the texture's quantisation:

| case | disputed | file nearer ours | file nearer Cycles |
|---|---|---|---|
| `normal-tangent-mirror` | 39 029 px | **0** | **39 029 (100 %)** |
| `normal-tangent` | 21 036 px | **0** | **21 036 (100 %)** |

**Unanimous over 60 065 pixels, and unanimity is the expected shape rather than a suspicious one**: a
systematic shading-normal defect means that wherever the legs differ by more than the asset can express,
ours is the one that is wrong. A per-pixel coin-flip would have read as noise; this reads as a mechanism.

**Contrast with the floor-selected population it replaces** — 147 669 px at 69 % and a median margin of
0.0025°. Same data, same code, different selection: one is a verdict, the other was a population that
mostly agreed being quoted about a tail it did not contain.

**This item is done.** The repair is `board:1126`, which this supersedes into rather than absorbing —
naming a branch and taking it are different claims with different evidence.
