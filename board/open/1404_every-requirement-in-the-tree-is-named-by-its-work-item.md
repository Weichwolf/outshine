Type: feature
Area: harness
Tags: instrument

**Every requirement in the tree is named by its work item**

The tree carries **two** requirement schemes and only one of them has a home.

`board:0042` has a file whose directory is its state, an id derived from the tree, three views that
cannot disagree — the file, the code, the log — and an invariant that a closed item is cited under
`test/`. **`I.26.10` has none of that.** [MEASURED] over `src/` and `test/` there are 25 distinct ids
of that shape, `I.26` alone at 201 sites, and the scheme is **defined nowhere in this repository**.
Board items cite it as *§ I.26.9's placement rule* and *§ I.26.15* — sections of a document that was
folded into `CLAUDE.md` and the board and no longer exists.

**So an `I.26.x` is an identifier with nothing behind it**: nothing says whether it exists, nothing
says what it requires, and nothing can check that a test claiming it proves anything.

## And it collides with the C++ rule index, which is a separate cost

`I.26` is simultaneously this tree's render-case covenant and a real Core Guideline — *For stable
library ABI, consider the Pimpl idiom*. `I.20` and `I.28` are used here as requirements and are not
guidelines at all: the source skips `I.13` to `I.22`. **A reader cannot tell the two schemes apart
from the text, and neither can a checker.** [OWNER] the rule index is a prompt for writing code rather
than a citation apparatus, so the collision costs nothing in evidence -- but it costs a reader the
moment they wonder which of the two an `I.26` in a `Covers(...)` line means.

## What must be true

- [ ] **Every `Covers(...)` names a `board:` id**, so the requirement it claims has a file, a state and
  an invariant behind it
- [ ] **Each surviving `I.x` requirement becomes a work item** carrying what it required, so nothing
  is lost in the move — *the board may be extended and may not be shortened*
- [ ] **The sweep is a tool and not 492 edits**, and the tool is the thing that ends the kind
- [ ] **A `Covers(...)` naming an id that does not resolve is red**, which is the citation invariant
  the board already has, one namespace over

## Comments

**This is a good problem to have and it is the shape of a project that outgrew its first document.**
The `I.x` scheme was a specification's section numbers; that specification became this file and this
board, which are better in every way except that their predecessor's ids stayed behind in the code.
**Every one of those 492 sites is a test that says which requirement it proves** -- which is more than
most trees have, and is exactly why it is worth giving them a home that can answer back.
