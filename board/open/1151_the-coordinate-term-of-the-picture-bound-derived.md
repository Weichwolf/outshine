Type: task
Parent: 0089
Area: render
Tags: oracle, instrument, khronos

**The coordinate term of the picture bound, derived**

`board:1133` is decided: **derive the term first and let the number land where it lands.** The deliverable
is a derivation, not a code change — the code change is one constant and is worthless without it.

**Parented to `board:0089` because that feature owns the bound** (`test/render/PictureBound.h` cites it,
and the scorer's own words are *the bound is the sum of the terms this case's own path puts in it*). A
term of the bound is a child of the feature that states the bound, and inventing a feature to hold one
derivation would be a headline over a paragraph.

**WHAT THE DERIVATION MUST CONTAIN TO BE CHECKABLE BY SOMEONE WHO DID NOT WRITE IT.** Each of these is a
place a derivation can be wrong while reading as rigour, and each has bitten this tree already:

- [ ] **The quantity, named in the field's vocabulary and bounded, not estimated.** *The interpolated
  texture coordinate's error at a fragment* — say whether that is the error of the barycentric
  interpolation, of the perspective divide, of the uv attribute's own quantisation, or of all three, and
  which dominates. A term that silently sums three mechanisms cannot be checked against any one of them
- [ ] **Every input with its origin and unit** — `[SET]`, derived or measured — and the arithmetic written
  out, not the result. This tree's existing weight term is `255 × 12.92 × 2⁻⁹ = 6.43476562`, and it is
  checkable **because every factor is visible**: the code range, the sRGB slope near zero, half a quantum
  of an 8-bit weight grid measured on the device
- [ ] **The propagation from a uv error to a CODE error, stated as a function of the texture**, because it
  is not a constant: on a smooth texture a sub-texel coordinate error moves the tap by the local gradient,
  and on a hard step it moves it by a full step. **The bound is per case and the texture's own maximum
  step is the case's input** — a single scalar added to every case would be the *invariance too broad*
  failure, and it would widen the bound for cases whose textures cannot express the error at all
- [ ] **The instrument that measures it independently of the case it is meant to explain.** A term derived
  from `texture-coordinate-test` and then validated on `texture-coordinate-test` is a frame fitted to a
  number. The shader suite already holds the precedent —
  `TheSamplerSnapsSubTexelWeightsToTheDeclaredCount` measured the weight grid on the device with no asset
  in the path, and the coordinate wants the same treatment
- [ ] **The population the term is claimed over, and the cases where it must be ZERO** — an untextured
  case has no coordinate, so a bound that grew there would be a tolerance granted for a mechanism that
  cannot occur
- [ ] **The predicted number, written down BEFORE the case is re-scored**, and the verdict either way

**THE OUTCOME THAT IS A SUCCESS, STATED IN ADVANCE.** The observed excess at the worst channel is a full
weight quantum, `10.295625` codes, against the existing half-quantum term of `6.43476562` — a ratio of
exactly **1.6**. If the derived coordinate term comes out **below `3.86` codes**, `texture-coordinate-test`
**still fails**, and the finding is that **our coordinate is genuinely wrong**. That is the method
working. The temptation this option carries is to keep deriving until the case passes, and a derivation
that arrives at exactly the gap it needed to close is evidence against itself.

**Done when** the derivation exists as a written product with every factor visible, the term is measured
by an instrument that does not use the case it explains, the number is published before the re-score, and
`texture-coordinate-test`'s verdict is stated with its cause either way.
