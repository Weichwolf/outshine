Type: bug
Area: harness
Tags: khronos, instrument

**Ninety-two emitted manifests declare no frame fraction, so their cases cannot be scored at all**

`Parity.cpp` requires `expected.subjectFrameFraction` and refuses without it. **The hand-written
manifests carry it and the EMITTED ones never did** -- the authoring helper that produced the corpus
cases at scale has no such field, so every case it wrote fails one check and takes its whole arm down
with it.

## The number, and it is why this is filed rather than fixed in passing

[MEASURED] over the full khronos run of this round, 426 tests over 142 cases:

| | |
|---|---|
| cases red in all three arms | **97** |
| manifests lacking the field | **92** |
| picture bound, over the cases that DID score | 100 within, 17 outside, 1 not-enforced of 118 |

**The two numbers were saying different things and only one of them was about pictures.** `criteria 112
met of 118` and `100 within` are counted over the cases the runner could score; the 97 never reached a
comparison. *A count quoted without the population it was drawn from is the defect `CLAUDE.md` names,
and this is an instance of it in the harness's own reporting.*

## Why the field cannot simply be defaulted

It is the subject's **projected area fraction**, which is a property of the subject and not of the rule
-- `kFramingFill` is 0.6 for every case and the projected fraction is not. The check exists so that a
camera which quietly frames a subject smaller cannot tighten the boundary bound without saying so, and
a default would be exactly the silence it refuses.

## What must be true

- [ ] **Every manifest declares it**, with unit and origin
- [ ] **The value is HARVESTED from the runner's own recomputation**, not written by hand
- [ ] **The authoring helper emits it**, so the next case written at scale carries it by construction
- [ ] **The origin says what it is.** Seeding a declaration from the measurement it is checked against
  makes the check tautological on the round it is seeded, and it is honest only because the camera is
  independently proven to be the framing rule's by
  `test/outshine/unit/gltf/ADerivedCameraIsTheFramingRuleAndNotAQuotation.cpp`. **What this field then
  catches is DRIFT, not present error**, and the note in each manifest says so rather than implying more
