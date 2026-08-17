Type: issue
Area: render
Tags: oracle, instrument, scope
Supersedes: 1359

**The owner set the tolerance at 99 percent, and the percentile I derived was two orders tighter**

**Two corrections, and the first is mine to make.**

## The reduction was never struck, and I read it into a sentence that did not carry it

`board:1171` records the owner's finish line as *all 148 models green on both counts*, and this board
then concluded that **a declared reduction had stopped being an acceptable end state**. **The owner did
not say that.** *Green on both counts* is a statement about the counts; it says nothing about how a case
whose oracle cannot decide it is accounted for. **The reduction stands**, exactly as `CLAUDE.md` has it:
inside the bound, or carrying a declared reduction naming why the oracle cannot decide the case.

*This is the second time in this session that a conclusion was drawn from a sentence rather than from
what the sentence said, and both times the correction cost less than the round the error would have.*

## The number: 99 percent, and mine was p99.99

`board:1359` decided the verdict metric moves from the maximum to the **99.99th percentile**, derived
from the disagreeing set being under 0.01 % of the frame. **The owner's own tolerance is that the
pictures need only look 99 % alike.** That is two orders looser and it is the owner's to set, so
**p99 replaces p99.99** and `board:1361` implements that instead.

**The rest of `board:1359` is untouched**: the 1.0-code perceptual floor and its derivation stand, the
geometric bound stays where the distribution put it, and the three conditions -- declared before it is
measured against, uniform over every case, the maximum still reported -- stand unchanged.

## And it does NOT rescue the shading arm, measured before saying so

[MEASURED] over the covered pixels of the three sphere cases, in display codes on the declared transfer:

| case | p50 | p95 | **p99** | max | **pixels over 1 code** |
|---|---|---|---|---|---|
| `shaded-sphere` (grey, roughness 0.5) | 0.34 | 5.07 | **9.40** | 48.28 | **28.2 %** |
| `shaded-sphere-black` (specular only) | 0.15 | 0.41 | **0.58** | 6.80 | **0.197 %** |
| `shaded-sphere-smooth` (roughness 0) | 0.48 | 12.96 | **34.53** | 105.71 | **36.4 %** |

**The specular path meets 99 % comfortably** -- 99.8 % of its pixels agree to within one code. **The
diffuse term does not: 28 % of pixels differ by more than a code**, which is not a tail but most of the
picture.

**So `board:1363`'s finding survives a looser tolerance intact.** The oracle evaluates a different
diffuse BRDF from the one glTF Appendix B specifies, and no percentile hides a disagreement that broad.
*Stated with the measurement because the tempting reading of "99 % is enough" is that the shading
question goes away, and it does not.*

**Closed on its own answer.** `board:1361` carries p99 instead of p99.99; nothing else moves.
