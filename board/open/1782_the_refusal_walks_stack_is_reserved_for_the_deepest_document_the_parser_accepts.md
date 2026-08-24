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
