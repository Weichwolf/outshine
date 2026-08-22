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

- [x] **An element the reader does not know is a refusal naming it and its parent**, so a typo costs a
  sentence rather than an afternoon
- [x] **An attribute the reader does not know is a refusal naming it**, for the same reason: a
  misspelled attribute silently takes the default, which is the worse half of this defect because the
  scenario still runs
- [ ] **A required attribute that is absent is a refusal naming what it belongs to** -- an `<instance>`
  with no `of` names no kind, and reading it as the empty string is how a scenario declares nothing.
  *Not built: the grammar table declares which attributes are PERMITTED and not which are REQUIRED,
  and the two are different columns. Left open rather than half-done, because a required-set guessed
  from what the reader happens to dereference would go stale the first time a field gained a default.*
- [x] **The known set is derived from ONE table per element**, so adding a row to the declaration adds
  it to the grammar and nothing is written twice

## What this may not do

**It may not refuse an element a LATER version of the engine adds.** That is what `Layers` and
`Identity.Version` are for, and a scenario declaring a version this engine does not know is its own
refusal with its own sentence.

## What it does now, and the sentence is the point

**One table of 55 rows -- `path`, the children it may carry, the attributes it may carry -- walked
before a single field is read.** The grammar and the reader are one declaration: adding a row to the
scenario adds it to both, and the check is `Grammatical(root, "scenario", error)` in one line.

[MEASURED] four ways to misspell a scenario, and every refusal names what would have been right:

```
<generatorz/>        -> <scenario> carries a <generatorz>, and the children it may carry are:
                        world render lighting providers generators compositors assets ...
radiusMeters="9"     -> <world> carries the attribute 'radiusMeters', and the ones it may carry
                        are: lat lon radiusM windDeg windMs cloudCover
<world><provider/>   -> <world> carries a <provider>, and the children it may carry are: none
<scenario naem="t"/> -> <scenario> carries the attribute 'naem', and the ones it may carry are:
                        name version epoch decay
```

**A refusal that lists the alternatives is a different tool from one that says no**, and it costs one
string that was already in the table.

## What proves it

**`test/render/outshine/client/AClientRunsAScenarioInFourLines.cpp`** -- the four typos above are
refused and each refusal is checked to quote the name it could not place.


---

Closed -- the parked fourth demand is built without the staleness the parking feared: the
grammar table gained a REQUIRED column (fourth field, default empty, aggregate rows untouched
where nothing is required), populated by one rule -- the attribute that names what the element
IS (of, kind, name, uri, asset, document, what, do, path), never the ones a default serves.
Twenty rows carry one identity attribute each; Grammatical refuses an absent or empty one
with "<instance> declares no 'of', and without it the element names nothing". The real
scenarios (f31, four-lines) pass unchanged -- the population matched what they already spell.
Proving test: test/render/outshine/client/AClientRunsAScenarioInFourLines.cpp, six refusal
cases each quoting the name it could not place. 129/129 warm.
