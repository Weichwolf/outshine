Type: bug
Area: clients
Tags: bug

**The scenario reader refuses what it does not know**

`ReadScenario` walks the elements it knows and **ignores everything else in silence**. A scenario that
spells `<generatorz>`, or writes `radiusMeters` where the reader wants `radiusM`, loads without a word
and does nothing -- and the author sees a black frame with no sentence to grep for.

**This is the defect class `board:1483` swept the engine for, in code written the same day.** Two
refusals exist in the whole reader and both are about the root element.

## What must be true

- [ ] **An element the reader does not know is a refusal naming it and its parent**, so a typo costs a
  sentence rather than an afternoon
- [ ] **An attribute the reader does not know is a refusal naming it**, for the same reason: a
  misspelled attribute silently takes the default, which is the worse half of this defect because the
  scenario still runs
- [ ] **A required attribute that is absent is a refusal naming what it belongs to** -- an `<instance>`
  with no `of` names no kind, and reading it as the empty string is how a scenario declares nothing
- [ ] **The known set is derived from ONE table per element**, so adding a row to the declaration adds
  it to the grammar and nothing is written twice

## What this may not do

**It may not refuse an element a LATER version of the engine adds.** That is what `Layers` and
`Identity.Version` are for, and a scenario declaring a version this engine does not know is its own
refusal with its own sentence.
