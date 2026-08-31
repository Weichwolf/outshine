Type: defect
State: active
Area: test, process
Tags: measured, guard

# The map and the guard that reads it agree on what a map is FOR

**Benchmark** — Unreal's `CONTRIBUTING` and module docs cite paths that exist and CI checks the
links; RAGE's internal docs are generated from the tree. **Both agree** that a document about a tree
argues FROM the tree. This tree built the same guard and then changed the document out from under
it.

## Measured

`TheMapCitesLinesThatSayWhatItClaims` reads `CLAUDE.md`, collects every backticked span that looks
like a path into this tree, and asserts two things: that there are at least twenty, and that every
one of them resolves. Today:

    backtick spans in CLAUDE.md            66
    of them path-shaped                     7
    that resolve                            6   board/  build/  compile_commands.json
                                                include/  src/  src/client/
    CHECK(paths >= 20)                     RED

**The second assertion passes and the first fails**, and that is the whole finding: every path the
map names is in the tree. It names six.

## Which half is right

`CLAUDE.md` now opens with **"This page is the AIM and never the state"** and says in as many words
that a sentence describing today would be a lie within a week, so there is none. A document that has
deliberately stopped describing the tree will deliberately stop citing it. **The count is therefore
pinning a STYLE the document has left on purpose, and `CLAUDE.md`'s own exception applies**: a check
that pins a spelling rather than a property is mis-specified and the CHECK changes.

**The position: the count goes, the resolution stays.** "Every path it names exists" is the property
worth guarding and it is cheap to keep true. "It names at least twenty" was a proxy for
"it argues from the tree", and the tree's own page has since decided that arguing from the tree is
`board/` and `git log`'s job rather than the map's.

This is written down rather than acted on, because a guard that was set deliberately is not one to
loosen on a passing measurement -- the owner set the bar and the owner rewrote the page, and which
of the two wins is a decision rather than a repair.

## What will be true

- [ ] The bar is DECIDED: either the count goes and the resolution stays, or `CLAUDE.md` regains the
      citations and the count stands. Not both, and not neither
- [ ] Negative control: a path added to `CLAUDE.md` that does not exist makes the claim go red. That
      half works today and must keep working whichever way the count is decided

## What this does NOT cover

Whether the map is TRUE. A path can resolve while the sentence around it is wrong, and no walk can
tell. That is what `board/` and the commits are for.
