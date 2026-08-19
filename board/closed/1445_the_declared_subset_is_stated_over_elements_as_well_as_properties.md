Type: bug
Area: harness
Tags: instrument, scope

**The declared subset is stated over elements as well as properties, so its count names the
population it decides**

The subset was written down as a list of CSS properties and selectors, and the corpus's second count
was read off it. That count decides which cases the engine CLAIMS, and a declaration reaches past
this engine in a way no property list can see: an element whose size comes from a resource nobody
loaded.

## What the first run measured, and why the number was about something else

[MEASURED] the first corpus run reported **10 cases inside the subset of 137**, and six of the ten
were about `<img>`, `<input>` or `<embed>`. Every one of them failed with a dimension of **zero** —
`<img> width is 0.000000, the document states 8000.000000` — because an image this engine never
fetched has no intrinsic size to lay out with. Two more produced NEGATIVE dimensions,
`width is -80.000000` and `height is -40.000000`, which is a separate defect and carries its own item.

`CLAUDE.md` names this failure and its four faces; this is the one called **input set too wide**. The
number was right — those cases really did miss their stated layout — and it was about a population
the claim never covered.

## What is true now

- [x] The vocabulary is an **allowlist and not a blocklist**, in `ElementIsInTheSubset`. A blocklist
  cannot be finished: an element nobody has thought of is laid out as an ordinary box and reported as
  held, where the allowlist answers *outside* and stays true when upstream grows a tag
- [x] Two lists rather than one, because they fail differently — a flow element missing from the first
  is laid out **wrongly**, a metadata element missing from the second is laid out **at all**
- [x] A document the markup reader REFUSES is outside the subset rather than a failed case. The refusal
  is the library's contract with a consumer writing a declaration; upstream's corpus is not that
  consumer, and `</embed>` is a markup question that may not become a layout number
- [x] The reader **names** what it dropped, and the case checks that the list is not empty — a counter
  that went up with nothing beside it is a case quietly dropped, which is the one thing the second
  count exists to make visible

## Three more the same run found, all of them in the reader rather than the layout

| what | [MEASURED] | why it is not a capability gap |
|---|---|---|
| a comment inside a declaration block was read as CSS | `/*`, `*/`, `spacing`, `things`, `to`, `for` counted as properties we lack | comments were stripped at the top level only, and a block is where the comments are |
| a vendor prefix counted against the subset | `-webkit-align-self` in **198** declarations, always beside the standard property it prefixes | a prefixed property is by construction not a standard one, so there is nothing here to be missing |
| a pseudo-element hid inside a class name | `.item::first-letter` parsed as a class named `item::first-letter`, matched nothing, counted as nothing | only the TAG part validated its characters; a rule that matches nothing looks exactly like a rule that did not apply |

## The count after the repair, over the same 138 cases

| | before | after |
|---|---|---|
| inside the declared subset | 10 of 137 | **27 of 138** |
| layout held | 0 | **4** |
| red | 10 | 23 |

**The intermediate state is the finding, not the end state.** With the element vocabulary in and the
shorthands still missing, the run read **0 inside of 138 and 138 PASS** — the suite got greener by
attempting less, which is precisely the fixed point the two counts exist to expose, and it was visible
in one line.

## Comments

The intermediate counts in the table above were taken through a stale binary in at least one run --
see `board:1446`, which this round found while chasing a repair that appeared to change nothing. The
numbers here were re-taken afterwards.

`flex` was the single largest gap and it was invisible until the noise cleared: **161** declarations
of `flex:` were counted as a property this engine does not hold, under 198 counts of a vendor prefix
that means nothing. Expanding `flex`, `flex-flow`, `background` and `border` is what took the inside
count from 0 to 27. `font` is still outside and is named rather than half-expanded.
