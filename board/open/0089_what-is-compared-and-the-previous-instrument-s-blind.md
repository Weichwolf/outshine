Type: feature
Area: render
Tags: instrument

**What is compared, and the previous instrument's blindness was structural rather than a bug**

- [ ] **The whole image, every pixel, no mask.** `worst_disagreement_px` is a distance from a coverage boundary, so **interior noise can never reach it** — the instrument was blind to what the owner saw *by construction*, not by defect. A picture claim needs an instrument whose domain is the picture
- [ ] **RGBA for the coverage count; RGB for the perceptual tail** — *split 2026-08-13, see the collision ruling below.* Straight alpha is on both sides and the channel is load-bearing **as coverage**: an empty render scores **50** differing pixels under RGB-only against **46 151** under RGBA, because the hole was 46 101 px wide and black-on-black in colour. *And the consequence is deliberate — our alpha is `covered(sceneDepth)`, which the bug tasks in `board/` records as wrong for a blended surface over nothing, so including alpha makes a filed defect finally score. An instrument reaching a known defect is the instrument working*
