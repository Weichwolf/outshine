Type: feature
Area: render
Tags: oracle, instrument, khronos

**A metric is decided against a recipe that can answer it, and a case declares both**

`board:1135` is decided: **the point-sampling recipe decides coverage, an integrating recipe decides
appearance.** This is what that costs and what it must carry. It is a feature rather than a task because
nothing in the tree states it: today one recipe answers every question, and the requirement that a metric
name the reference able to answer it has no home.

- [ ] **`appearance` is a declared recipe name beside `default`, and the manifest already has the shape.**
  `renders` is a map of named recipes, `prepare.py` takes `--recipe` and repeats, and the runner already
  reads named companions beside `oracle.exr`. **No second cache and no second key** — one more entry in a
  map that exists
- [ ] **THE SAMPLE COUNT IS DERIVED OR IT IS `[SET]` WITH ITS REASON, and 256 is neither today.** It is
  the number one experiment used. What decides it: these scenes are deterministic apart from the pixel
  filter — delta lights, `bounces.max = 0`, emitters — so the only stochastic dimension is the filter
  position, and the right count is **the one at which the beauty product stops moving**. Measure it: a
  sweep at 16 · 64 · 256 · 1024 on `four-texels-per-pixel`, and the count is where the difference falls
  under the case's own instrument floor. A magic 256 in a recipe is a magic number in a cache key
- [ ] **THE COVERAGE MASK KEEPS COMING FROM THE POINT RECIPE**, always, including for the appearance
  comparison. That is what keeps the routed population **the same selection** before and after — a change
  of reference that also changed which pixels are compared would move a number without moving a threshold,
  which is the failure this repository names by name
- [ ] **APPEARANCE IS JUDGED ONLY WHERE THE INTEGRATING RECIPE'S ALPHA IS EXACTLY 1**, and the excluded
  count is published. An integrating oracle antialiases **geometry** as well as texture; our rasteriser
  does not, so every silhouette pixel would carry a difference that is not about the filter under test.
  Alpha 1 is *all samples hit*, which is the interior, which is exactly the domain where the remaining
  difference is texture filtering and shading. **Without this the appearance metric would fail on every
  case with an edge and the failure would say nothing**
- [ ] **WHICH CASES TAKE IT IS DERIVED AND PUBLISHED, NOT JUDGED.** The criterion: **a case takes the
  appearance recipe when any covered pixel selects a mip level above 0.** The reason it is *any* and not a
  fraction is that the picture bound is a **max** — one minifying pixel decides the tail, so a point
  sampling reference is already the wrong reference for that case. [MEASURED] under `board:1130`:
  `simple-texture` **0** such pixels, `normal-tangent` **~920**, `normal-tangent-mirror` **~861**,
  `scifi-helmet` **~5 791** — so the criterion is not hypothetical and it separates the corpus today
- [ ] **The per-case cost is published before it is spent**, as the preparer's `dry-run` already does for
  fetching: renders are ~2 s at one sample on this host, so an appearance recipe is that times the derived
  count, per taking case, once
- [ ] **`four-texels-per-pixel`'s state is stated rather than discovered.** Its alpha predicate reads the
  **point** recipe and passes at **0**; its picture bound reads the **appearance** recipe and is
  **37.863681** while the mip chain stays pinned off, and **5.340** when it is enabled. So the case keeps
  **one** failure, it is `board:1130`'s and not this instrument's, and it disappears on the round that
  makes the chain readable — which is what `board:1130`'s own last paragraph says it is waiting for

**Done when** every metric names the recipe that decides it, a case declares the recipes it needs and the
criterion says which those are, the sample count carries its derivation, and no case fails a coverage
predicate for a reason that is not about coverage.

## A second parameter, found by a feature that needs it: BOUNCES

**This item prices the appearance recipe on its sample count alone, and the sample count is not the only
thing that separates it from the point-sampling one.** [MEASURED] over every render declaration in the
corpus — **61 declarations across 36 cases — `bounces.max` is 0 in every one.**

**A zero-bounce reference carries no indirect light**, and three things in this tree depend on there being
some:

- **`occlusionTexture`** attenuates indirect light and nothing else — the specification's own sentence is
  *"Direct lighting is not affected"* — so at zero bounces **it is unprovable against this oracle at any
  sample count.** That is why `board:0079` gates its impact-46 row behind this item rather than in front
  of it
- **`board:0087`'s emission lowering** was argued on variance: *a Diffuse BSDF at one sample per pixel is
  a Bernoulli draw on the visible sky fraction, which is an ambient-occlusion estimator and not a
  material.* **At the integrating recipe's sample count that objection dissolves**, so the lowering of
  17 cases to emitters is a decision this item can revisit — and it should say whether it does
- **Our own side has no ambient term in the subject path**: `Stage::Subjects` reads `{kNoEdge}` while four
  world stages read `IrradianceBuffer`. **So raising bounces alone would make the oracle right and us
  wrong**, which is a real finding rather than a reason not to raise it

- [ ] **`bounces.max` is declared by the appearance recipe and derived like the sample count** — the
  number at which the beauty product stops moving, measured by the same sweep, not a preference
- [ ] **Raising it is not free and the cost is not the render time.** It changes what the reference *is*,
  so every case taking the appearance recipe is re-judged against a different physics. **The two recipes'
  disagreement is the measurement**, published per case, exactly as this item's 37.880-code
  self-disagreement already is
- [ ] **A case may take the integrating recipe at zero bounces**, and that stays the default: a texture
  filter question needs integration over the pixel and nothing over the hemisphere. **Bounces are declared
  by the cases that need indirect light**, which keeps the cost proportional to the question — the same
  rule `board:1143` applies to quantity passes
