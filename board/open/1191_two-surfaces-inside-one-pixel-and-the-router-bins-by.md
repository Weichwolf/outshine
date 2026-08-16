Type: bug
Area: render
Tags: oracle, khronos, instrument

**Two surfaces inside one pixel, and the router bins by identity rather than by depth**

Both `KHR_materials_variants` cases fail the picture bound on **exactly 2 pixels each — the same two**.
[MEASURED] over 29 972 covered pixels:

| second surface within, along the view ray | pixels |
|---|---|
| 100 µm | **91** |
| 10 µm | **10** |
| 1 µm | **2** — and they are the two failures |

**The two: 1.95 µm and 0.113 µm.** The second is **below one ulp of camera-relative f32 at 1.19 m**, and
**eight of the ten closest pairs are resolved identically by both sides** — so this is not a systematic
depth error, it is a tie that occasionally falls the other way.

## Why the router cannot see it, and it is by construction rather than by omission

**`Routing.h` bins a pixel by the oracle's OBJECT and MATERIAL index passes. Here both surfaces are one
object and one material.** The router has no predicate that can separate them, and `board:1144` gave it
identity precisely because identity was the missing half then.

**The generalisation is the finding: the router bins by WHAT covers a pixel and never by WHERE ALONG THE
RAY.** Two coincident surfaces of one material are one identity and two depths, and a rasteriser
answering *which fragment won* against a path tracer answering *which intersection came first* will
disagree at a spacing no declaration controls.

**It is the same family as `board:1185`** — a verdict decided by a quantity the instrument cannot
resolve — **and it is a different discriminator**: that one is coverage at an edge, this is depth within a
pixel. Filed separately because the repairs differ, and cited together because the family is what makes
both worth fixing rather than tolerating.

## The two failures are not the same kind and must not get one answer

- [ ] **0.113 µm is below the arithmetic's own resolution** — at 1.19 m in camera-relative f32 there is no
  correct answer, and **scoring it in either direction manufactures a verdict**. This is `board:1185`'s
  ruling applied to depth: **a difference beneath the instrument's floor is routed and counted, never
  scored**
- [ ] **1.95 µm is ≈16 ulps and is resolvable in principle** — so it is a real disagreement, and **nobody
  can currently say whether OUR answer is the wrong one.** The developer inferred which surface each side
  drew **from the sampled colour, not from a depth buffer**, which is an inference and was reported as
  one
- [ ] **The instrument that would settle it does not exist**: a per-pixel depth from both sides at the
  same point. **Ours is readable — `SceneDepth` is an attachment and `ReadDepth` exists — and the
  oracle's is not: `depth` was dropped from `QUANTITY_PASSES` as *the router's WEAKER implementation; index
  is exact and no test reads depth today*.** That reason was correct when identity was the question and it
  is not the question here

## What would be right instead

- [ ] **The router gains depth as a third discriminator**, after coverage and identity: *both sides agree
  this pixel is covered · they agree which surface · they disagree which of two coincident surfaces* is a
  fourth class and it has no bin today
- [ ] **The oracle's depth pass returns, declared per case** as `board:1143` requires, so it is paid for by
  the cases that need it rather than by all 37
- [ ] **The population is published before any verdict moves.** 91 · 10 · 2 is the shape of this defect on
  one asset; **whether other cases carry the same tail is unmeasured**, and a repair scored on one case is
  the *population too small* face this tree names
- [ ] **What must NOT happen is a tolerance.** Widening the bound until two pixels stop failing is a number
  fitted to a case, and the two pixels are not the same defect anyway — one is unresolvable and one is a
  real disagreement

**Done when** a pixel whose two candidate surfaces lie within the depth instrument's own resolution is
routed and counted rather than scored, a disagreement above that resolution is attributable to one side by
a depth measurement rather than by inference, and the population is measured across the corpus rather than
on the asset that surfaced it.
