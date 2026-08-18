Type: bug
Area: corpus
Tags: khronos, instrument

**Ninety-three manifests declare a camera the framing rule does not derive**

`ADerivedCameraIsTheFramingRuleAndNotAQuotation` exists so that no case can be made to pass by moving
its viewpoint. [MEASURED] over the corpus as it now stands, it reports:

| the check | cases failing |
|---|---|
| the derived eye IS the declared eye | **93** |
| the declared aim IS the bounds' centre | 33 |
| the declared clip contains the rule's | 33 |
| the declared frame fraction is the rule's | 16 |
| the declared camera is the rule's answer or a metre away, never fitted near it | 12 |

## The cause is named and it is mine

The helper that authored these manifests computes a subject's bounds from **the eight corners of each
primitive's local AABB, transformed** -- which is a bound on the bound, and always at least as loose as
the AABB of the transformed vertices the engine itself computes. A looser radius moves the camera back,
so the declared eye sits behind the rule's by a hair. *It is a small number and it is the wrong kind of
small: the camera is what the picture is a function of, and a declaration that no rule derives is a
second determination of the viewpoint.*

## Why it cannot be repaired by editing the manifests alone

**The camera is in the render key.** Every corrected camera is a new oracle render, so this is one full
preparation pass -- which is also why it belongs with `board:1360`'s `kFramingFill` change rather than
before it: two viewpoint changes measured separately would cost two passes to learn one thing.

## What must be true

- [ ] **Every non-`exact` camera is HARVESTED from the rule's own output**, the way the frame fraction
  now is, and never recomputed by a second implementation
- [ ] **The authoring helper stops computing bounds at all** -- a helper that can disagree with the
  engine about a subject's extent is a second implementation of the thing under test
- [ ] **`kFramingFill` 0.6 -> 0.9 lands in the same pass** (`board:1360`), because both move every camera
- [ ] **`exact` cases keep their cameras**, read from `acceptanceClass` rather than from a list
- [ ] **The before-and-after quotes the same population**, and says how many cases changed verdict

## Comments

**This is what the test is FOR and it took a corpus of 151 cases to make it audible.** With twenty
cases the same defect was five reds that read as noise; at scale it is a ranked list with one cause.
