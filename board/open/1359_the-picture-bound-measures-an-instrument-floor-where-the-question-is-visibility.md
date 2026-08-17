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
