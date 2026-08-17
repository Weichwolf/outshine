Type: issue
Area: render
Tags: oracle, instrument, scope

**The picture bound measures an instrument floor where the question is visibility**

**The owner: the case thresholds are too strict, and should be adequate for a game engine.** That is a
scope instruction and it is legitimate, but only in one of its two possible readings, and the difference
is the whole of this issue.

| reading | verdict |
|---|---|
| **widen a bound because a case fails it** | **stays refused.** That is a number fitted to a case, and `CLAUDE.md` forbids it for a reason this board has already paid for. A suite whose thresholds follow its failures measures only itself |
| **derive the bound from the PURPOSE, declared in advance, applied to every case alike** | **this is the instruction, and it is right.** The purpose is a game engine at 720p60, not a path-tracer clone. A threshold that comes from an *instrument's floor* answers a different question from one that comes from *what a viewer can see* |

## Where it bites, with the number

[MEASURED] `SpecularTest` fails `worst_disagreement_px` at **0.17261918 px against a bound of 0.005 px**.

**0.005 px is one five-hundredth of a pixel.** `CLAUDE.md` names it for what it is — *a 0.005 px
instrument floor* — the resolution below which the comparison cannot tell two silhouettes apart. **It was
never a claim about what is visible**, and a silhouette 0.17 px off is a silhouette nobody has ever seen
move. *The number is correct and it is answering the wrong question, which is precisely the "domain too
narrow" face of the failure this tree already names.*

## The two thresholds, and only one of them is really in question

| metric | today | what it is |
|---|---|---|
| `worst_disagreement_px` | **0.005 px** | the **geometric** bound, on pixels the two sides disagree about the identity of. This is the instrument floor standing in for a visibility threshold |
| `picture_max_delta_code` | **6.4354338 codes** of 255 | the **perceptual tail**, on pixels both sides agree are covered. ~2.5 % of the range, and it is already derived from named terms rather than set |

**The second is not obviously too strict and the first plainly is.** A recommendation with a basis rather
than a preference: **half a pixel**. A silhouette displaced by less than half a pixel cannot change which
pixel a centre-sampling rasteriser calls covered, so below that the two sides disagree about a boundary
neither of them draws differently. *That is a threshold derived from the raster, not from taste.*

## Three conditions, or this becomes the thing it is replacing

- [ ] **Declared before it is measured against, never after.** The new bound is written down, then the
  corpus is re-run, and whatever it says it says. A threshold chosen while looking at which cases would
  flip is a fit wearing a derivation's clothes
- [ ] **Uniform over every case.** Per-case relaxation is disqualification by another name, and
  disqualification is already the ladder's last rung with its own procedure
- [ ] **The instrument floor keeps being REPORTED.** It stops being a verdict and does not stop being a
  number: a case whose geometry agrees to 0.005 px and one that agrees to 0.4 px are different facts, and
  a bound that swallowed the difference would hide a regression inside its own tolerance

## What this does not touch

**The Khronos criteria.** They count features and they are not a tolerance — a criterion is met or it is
not, and nothing here makes one easier to meet.

## Comments

**Filed rather than applied, because the number is the owner's.** The engineering half — reading a
threshold from a declaration instead of a constant, and re-running the corpus — is a task once the number
exists. *What must not happen is this arriving as a quiet edit to a constant in a round that was about
something else.*

## DECIDED, and the first thing the measurement did was refute this item's own headline

**The owner delegated the number: be reasonable, and it must be achievable.** So the population was
measured before anything was chosen, and it settled two of the three questions without an opinion.

### The geometric bound is NOT too strict, and naming it was a mistake

[MEASURED] `worst_disagreement_px` over all 44 cases:

| | |
|---|---|
| exactly 0 | **25 cases** |
| 4.03e-05 … 0.00198 px | **17 cases** — every one inside the 0.005 px bound with 2.5x to spare |
| **the gap** | **87x** |
| above it | **2 cases**: `SpecularTest` 0.17261918, `PointLightIntensityTest` **0.61810204** |

**42 of 44 meet it comfortably and the two that do not are over half a pixel out.** 0.618 px is a
silhouette a person can see move. *This item nominated that bound on the strength of one case's number
and no distribution; the distribution says the bound is right and those two cases are wrong.*

### There is no single perceptual threshold either, because the bound is PER CASE

[MEASURED]: `SpecularTest`'s `picture_max_delta_code` bound is **6.4354338** codes and
`BoxVertexColors`'s is **0.000668135** — four orders apart, because each is derived from the terms that
case's own picture carries. **"Too strict" cannot be answered by moving a number; it is a question about
the derivation.** And `board:1195` has already found one term genuinely missing.

### So the decision is about SHAPE, and it is two changes

**1 · A PERCEPTUAL FLOOR OF 1.0 CODE on every case's bound.** [DERIVED] the comparison is expressed in
codes of the case's declared 8-bit transfer, and one code is that transfer's quantisation step. **Below
its own quantum, "the two pictures differ" is not a representable claim** — no display the picture is
judged on can show it. So the bound becomes `max(the derived terms, 1.0 code)`. *Not 2, which would be a
comfort margin, and comfort is exactly what a derivation may not contain.*

**2 · THE VERDICT MOVES FROM THE MAXIMUM TO THE 99.99th PERCENTILE, and the maximum keeps being
reported.** [DERIVED] the metric today is a max over 1280 x 720 x 3 = **2 764 800 channels**, so it is a
claim about the single worst channel while the finish line is a claim about a *picture*.
`board:1136` measured the consequence directly: **four cases exceed their bound on fewer than ten
channels in 2.6 million.** At 1e-4 the disagreeing set is under 0.01 % of the frame. **The maximum stays
on the report** — a case agreeing to one code and a case agreeing to 200 on nine channels are different
facts, and a rule that hid the difference would conceal a regression inside its own tolerance.

### What does NOT move

**The Khronos criteria** — a feature is implemented or it is not, and no tolerance touches that. **The
geometric bound**, refuted above. **The ladder** — disqualification stays the last rung, per
`(case, metric)`.

### The condition this decision carries

- [ ] **It is declared here, BEFORE the corpus is re-run**, and whatever the re-run says it says. **A
  percentile chosen while watching which cases would flip is a fit in a derivation's clothes**, so the
  count it buys is a measurement owed rather than a benefit claimed. *This item does not predict it.*

**Closed on its own answer, which is what an issue does.** The implementation and its before-and-after
are `board:1361`.
