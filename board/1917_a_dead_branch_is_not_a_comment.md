Type: bug
State: open
Area: engine
Tags: measured, house-rule

# A dead branch is not a place to keep a comment

    src/engine/Engine.cpp:286   if (false) {
    src/engine/Engine.cpp:287     Error = "a drive stands, and the ground is APPENDED to the driven
                                  vehicle's own glTF, so it inherits that placement and that model
                                  scale. Measured: the ring reaches the draw list at 517 batches
                                  ... -- a count is not a picture ..."
    src/engine/Engine.cpp:295     return false;
    src/engine/Engine.cpp:296   }

Ten lines the compiler removes, carrying six lines of prose about a measurement taken two commits
ago and a mechanism the tree no longer has. CLAUDE.md's rule is absolute -- `src/` carries no
explanatory prose, no derivation, no board number -- and `TheSourceCarriesNoCommentary` walks for
`//` and `/*`, so a paragraph inside a string literal inside a branch that cannot be taken passes
it. The walk is not wrong; the code is evading it.

`-Wall -Werror` does not object either, because `if (false)` is a statement and not an unused
variable. Nothing in the tree refuses this.

The prose belongs where CLAUDE.md puts it -- the board item and the commit -- and board:1890
already carries the same measurement in more detail. Delete the branch.

## What will be true

- [ ] No branch in `src/`, `include/`, `apps/` or `tools/` is nailed shut by a literal, and the
      history of what was tried lives in git.
- [ ] Proving case: the commentary walk refuses a constant-false branch as well as a `//`.
      Negative control: the branch restored, and the walk goes red.
