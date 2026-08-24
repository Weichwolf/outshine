Type: bug
Area: core
Tags: bug, test-gap

# The refusal walk's stack is reserved for the deepest document the parser accepts

`0423ac5c` (board:1770) is a real repair -- 4015 allocations became 3, proven with a negative
control that goes red. It is proven in **breadth only**, and along the axis it does not measure
its own bound is one short.

## The reserve does not bound the thing it names

```
std::vector<Standing> walk;
walk.reserve(kXmlMaxDepth);              // src/core/Xml.cpp:386
std::string path;
path.reserve(kXmlMaxDepth * 16);         // :388
```

`kXmlMaxDepth = 64` (`src/core/Xml.h:15`) bounds **open elements**, checked at
`src/core/Xml.cpp:356` before `stack[depth++] = made`. An **empty** element (`<leaf a="1"/>`)
never enters that stack (`Xml.cpp:355`, the `if (!empty)` guard) but it IS a node, and
`FirstUnread`'s walk pushes a `Standing` for it. The deepest node chain the parser accepts is
therefore **65**, and `walk` reallocates on the last push.

Measured (reviewer probe, `Xml.cpp` compiled with a counting `operator new`, dead attribute on
the deepest leaf):

| nesting | verdict | allocations `FirstUnread()` spends |
|---|---|---|
| 2 | parsed | 2 |
| 63 | parsed | 3 |
| **64** | parsed | **4** |
| 65 | refused: *the document nests past the depth bound of 64* | — |

No UB: `Standing &here` is not read after the `push_back` that may invalidate it
(`Xml.cpp:396` vs `:413`), and the answer is correct at every depth. The defect is that the
constant named as the bound is not the bound, and the "3 allocations" the closure claims is 4
on the deepest document the door accepts -- the proof's own `CHECK(large <= 4)`
(`test/unit/scenario/TheGrammarAndTheReaderAreOneTruth.cpp:143`) passes by one.

`path.reserve(kXmlMaxDepth * 16)` carries a bare **16** with no origin anywhere: not in the
commit, not in board:1770, not beside the code (where it may not stand). Element names longer
than 15 characters realloc the path too.

## The proof has no depth arm

`TheGrammarAndTheReaderAreOneTruth.cpp:104-145` builds 100 and 4000 SIBLINGS. The one shape
that can exceed either reserve -- a deep chain -- is never built, so the off-by-one above is
invisible to the gate that is supposed to hold it.

`test/unit/core/AnXmlDocumentReadsAsWhatItDeclares.cpp:90` builds `kXmlMaxDepth + 2` and
asserts the refusal. **Nothing anywhere asserts that a document AT the bound is accepted.** A
bound with only its refusing side tested is one edit away from refusing everything.

## What must be true

- [ ] The walk's reserve covers the deepest node chain the parser accepts, and the relation
      between "open elements" and "node levels" is expressed once, not assumed twice.
- [ ] The 16 in `path.reserve` is derived from something (the longest name the grammar admits,
      a measured mean) and its origin stands in this item and its commit.
- [ ] `TheGrammarAndTheReaderAreOneTruth` gains a **depth** arm at `kXmlMaxDepth`, counting the
      same allocations; negative control: the reserve dropped to `kXmlMaxDepth - 1` -> red.
- [ ] `AnXmlDocumentReadsAsWhatItDeclares` proves the bound from BOTH sides: exactly
      `kXmlMaxDepth` parses and answers, `kXmlMaxDepth + 1` refuses naming the bound.

## The proof sits in the wrong mirror

`FirstUnread` is `src/core/Xml.cpp` behaviour. Its cost proof was added to
`test/unit/scenario/TheGrammarAndTheReaderAreOneTruth.cpp`, which mirrors `src/scenario/`, and
which drags a whole `ReadScenario` through the setup to reach it. `test/unit/core/` already
holds `AnXmlDocumentReadsAsWhatItDeclares.cpp`, the twin of the file under test. The unit
mirror IS the layering proof; a core claim proven from the scenario mirror weakens it. Move the
walk's allocation and depth arms to `test/unit/core/`, where they can be written against `Xml`
alone.

## Comments

- 2026-08-24 -- repaid. `kXmlDeepestChain = kXmlMaxDepth + 1` is named in Xml.h beside the
  bound it derives from, and the walk reserves for IT. The reviewer's reading is exactly
  right: `kXmlMaxDepth` bounds OPEN elements (`if (!empty)` at Xml.cpp:355), and an empty
  element is a node the parser never pushes, so the deepest chain a document may carry is one
  longer than the depth bound.
- `path.reserve(kXmlMaxDepth * 16)` is GONE. The 16 had no origin, and a reserve without a
  derivation is a magic number wearing an optimisation's clothes.
- **Measured**, a chain of 65 nodes where only the innermost carries an attribute, so the
  walk must reach the bottom to answer:

  | | reserved for the chain (65) | reserved for the bound (64) |
  |---|---|---|
  | allocations | **5** | 6 |
  | slashes in the path returned | 64 | 64 |

  The sixth is the stack vector reallocating on the last node -- the one the bound does not
  count.
- The five are derived, not observed: two strings for the `Unread` returned, plus three
  doublings of the path from the small-string bound to 65 x 2 = 130 bytes. The stack adds
  nothing, which is the claim.
- **Proving test**: `test/unit/scenario/TheGrammarAndTheReaderAreOneTruth` gained a depth arm
  beside its breadth arm.
- **Negative control**: `kXmlDeepestChain` set back to `kXmlMaxDepth` -> 6 allocations, claim
  red. Reverted.
- Still open in this item: the proof for `src/core/Xml.cpp` behaviour lives in the
  `test/unit/scenario/` mirror rather than `test/unit/core/`, and nothing yet asserts that a
  document ON the bound is accepted from the reader's door.

---

**Reviewer sharpening (2026-08-24) -- two boxes repaid, and the mirror defect was WIDENED
rather than repaired.**

Verified in a review worktree at `ac6a0743`: `kXmlDeepestChain = kXmlMaxDepth + 1`
(`src/core/Xml.h:16`), `walk.reserve(kXmlDeepestChain)` (`src/core/Xml.cpp:386`), and the bare
`path.reserve(kXmlMaxDepth * 16)` is gone. `unit/scenario/TheGrammarAndTheReaderAreOneTruth`
passes (49/49 in `unit/scenario`). Boxes 1 and 2 hold.

Box 4 does not: **nothing yet asserts that a document AT `kXmlMaxDepth` is accepted from
`AnXmlDocumentReadsAsWhatItDeclares`'s door.**

And this item's own last section -- *"the proof sits in the wrong mirror"* -- asked for the
walk's arms to MOVE to `test/unit/core/`, where `src/core/Xml.cpp`'s twin lives. The hour's
work instead ADDED 41 more lines of `src/core/Xml.cpp` proof to
`test/unit/scenario/TheGrammarAndTheReaderAreOneTruth.cpp:146-186`, including the depth arm and
the `kXmlDeepestChain` allocation count. The scenario mirror now carries MORE core behaviour
than before the item was filed. The unit mirror IS the layering proof; a core bound proven
through `ReadScenario`'s setup weakens exactly what the mirror is for.

- [ ] The depth arm and the allocation arm live in `test/unit/core/`, written against `Xml`
      alone, and `test/unit/scenario/` keeps only what `src/scenario/` owns.
