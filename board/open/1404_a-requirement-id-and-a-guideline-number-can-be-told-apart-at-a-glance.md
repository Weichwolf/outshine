Type: feature
Area: harness
Tags: instrument

**A requirement id and a guideline number can be told apart at a glance**

The C++ rule index at the foot of `CLAUDE.md` exists so that a rule number and its content are never
apart: `ES.9` stood in two agent definitions as the enumeration rule for a long time, and it is *avoid
ALL_CAPS names* -- the enumeration rule is `Enum.2`. **The index makes the cheap half reliable and
leaves the expensive half to a fetch**, which is the right division and is worth keeping.

**It shares a namespace with this tree's own requirement ids, and that is the same confusion wearing
the other face.** [MEASURED] over `src/` and `test/`:

| cited | what it is here | what it is in the guidelines |
|---|---|---|
| **`I.26`**, 201 sites plus `I.26.5`, `I.26.10`, `I.26.12`, `I.26.14` … | this tree's render-case covenant -- *a render test is a directory* | **a real rule**: *For stable library ABI, consider the Pimpl idiom* |
| **`I.20`**, 3 sites | a requirement about documents citing files | **not a rule at all** -- the source skips `I.13` to `I.22` |
| **`I.28`**, 1 site | a manifest's declared requirement | **not a rule at all** |
| `I.22`, `I.23`, `I.27`, `Enum.2`, `F.20` and 34 others | genuine guideline citations | genuine |

**So a reader cannot tell from the text which of the two a number means, and neither can a checker.**
Every one of the 39 guideline-shaped citations resolves except the two that were never guidelines --
and those two resolve to nothing precisely because they belong to the other scheme.

## What must be true

- [ ] **A requirement id is spelled so that no guideline number can be mistaken for it**, and the
  choice is one character rather than a convention -- the board's own `board:0042` is the precedent
- [ ] **A guideline citation resolves to a line in the index**, checked from the tree the way
  `board:` markers already are. It is the same invariant one namespace over
- [ ] **The sweep is mechanical and is a tool rather than 492 edits**, which is what *build the thing
  that ends the kind* means here

## Comments

**This is a good problem to have**: it exists because the tree cites its requirements at all, and most
trees do not. The two schemes have simply grown into each other, and separating them makes both
checkable -- the guideline half against the index, the requirement half against the covenant it names.
