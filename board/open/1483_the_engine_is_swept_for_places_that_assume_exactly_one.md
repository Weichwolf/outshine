Type: task
Parent: 1382
Area: core
Tags: scope

**The engine is swept for places that assume exactly one**

`board:1482` found two by asking the format what it permits and the tree what it spells, and both were
SILENT -- a wrong answer that looks right, which is the worst shape a shortfall takes. This is the rest
of that sweep.

**The rule**: *a shape is 0 or 1..N; code that assumes exactly one of something is a defect waiting for
the second.* **The test is not whether a bound exists** -- a bound somebody chose on purpose is what
`CLAUDE.md` asks for -- **it is whether reaching it is NAMED**.

| | | |
|---|---|---|
| **named refusal** | the bound is in the sentence | correct, and most of this tree |
| **published shortfall** | the count and what it dropped are answered | correct where the picture survives |
| **silent truncation** | neither | **the defect class** |

## What must be true

- [ ] **Every place that reads a numbered attribute set reads all of them or refuses by name** --
  `JOINTS_n` is done, `TEXCOORD_n` refuses, `COLOR_n` has no further semantics; the sweep must state
  which others exist rather than assume these are all
- [ ] **Every `[0]` on a container the format allows N of is either glTF's own rule or a defect**, and
  which one is written down per site. [MEASURED] there are 11 such sites in `src/`, and the ones read
  so far are legitimate: `Primitives[0].Targets` is glTF stating every primitive of a mesh agrees, and
  `Deck[0..2]` is a three-deck cloud model
- [ ] **Every declared bound refuses or publishes when reached**, and the ones already checked are
  `kMaxSubjectLights` 16 · `kMaterialSlots` 1 · `kGroundSlots` 12 · `kMaxColourAttachments` 8 ·
  `kUvSets` 2 · `kTagSlots` 32 -- the last one was the defect and is fixed
- [ ] **A scene is 0 or 1..N**: glTF permits several scenes and a default; a reader that takes the
  first would be this class
- [ ] **A camera is 0 or 1..N**, and the same question
- [ ] **The sweep's result is a TABLE**, one row per site, so the next round reads what was decided
  rather than re-deriving it

## What this may not do

**It may not add a bound to make a site look considered.** The defect is silence, not the number: a
site that reads exactly one because the format states exactly one is correct and gets a row saying so.
