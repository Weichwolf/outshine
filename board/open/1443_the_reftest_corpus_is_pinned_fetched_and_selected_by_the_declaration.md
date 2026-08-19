Type: task
Parent: 1442
Area: corpus
Tags: scope

**The reftest corpus is pinned, fetched and selected by the declaration**

WPT's CSS reftests, at one commit, obtained by `test/harness/shared/corpus/prepare.py` -- the same
preparer, the same digest discipline and the same fetch cache the glTF corpus uses, because a second way
to obtain an upstream is a second thing that can drift.

**THE SELECTION IS DERIVED AND NOT A LIST.** A pair is in the corpus when every element, property and
value either of its two files uses appears in `board:1442`'s table. That is a question the parser
answers, so the corpus is a FUNCTION of the declaration -- widen the subset and cases appear; narrow it
and they leave, and nobody edits a list either time.

- [ ] the pin is a commit and a digest per file, like every other upstream here
- [ ] the selection is computed from the declaration and its result is published, so *how much of the
      suite the subset reaches* is a number rather than an impression
- [ ] a pair whose two files disagree about which properties they use is **in** if both are inside the
      subset and **out** otherwise, and the reason is recorded per pair
- [ ] `Ahem` is fetched with the corpus, because a layout test that depends on a system font is a font
      test wearing a layout test's name

## What this must not do

**It must not select by directory.** `css/css-flexbox/` contains tests for `position: absolute` inside a
flex container, and a directory that looks like the subject is not the subject. The declaration decides.
