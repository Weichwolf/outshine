Type: task
Parent: 1442
Area: corpus
Tags: scope

**The reftest corpus is pinned, fetched and selected by the declaration**

WPT's CSS reftests, at one commit, obtained by `test/harness/shared/corpus/prepare.py` -- the same
preparer, the same digest discipline and the same fetch cache the glTF corpus uses, because a second way
to obtain an upstream is a second thing that can drift.

**ONE MANIFEST FORMAT FOR EVERYTHING THAT RENDERS.** A reftest pair is a case directory with a
`manifest.json` like every other case in this tree -- its subject is a document instead of a glTF and its
criterion is `reftest` instead of `numeric`, and nothing else about it is special. That is what lets the
browser under `test/viewer/` list a UI case beside a Khronos one without a line of special handling, and
what stops a second corpus growing a second shape.

**THE SELECTION IS DERIVED AND NOT A LIST.** A pair is in the corpus when every element, property and
value either of its two files uses appears in `board:1442`'s table. That is a question the parser
answers, so the corpus is a FUNCTION of the declaration -- widen the subset and cases appear; narrow it
and they leave, and nobody edits a list either time.

- [ ] the pin is a commit and a digest per file, like every other upstream here
- [ ] the selection is computed from the declaration and its result is published, so *how much of the
      suite the subset reaches* is a number rather than an impression
- [ ] a pair whose two files disagree about which properties they use is **in** if both are inside the
      subset and **out** otherwise, and the reason is recorded per pair
- [ ] the manifest gains a subject kind `document` and a criterion kind `reftest`, declared in
      `test/harness/shared/corpus/manifest-schema.json` where every other key of a manifest is declared
- [ ] `Ahem` is fetched with the corpus, because a layout test that depends on a system font is a font
      test wearing a layout test's name

## What this must not do

**It must not select by directory.** `css/css-flexbox/` contains tests for `position: absolute` inside a
flex container, and a directory that looks like the subject is not the subject. The declaration decides.

## Where a pair's declaration lives, and the two rules that pull against each other

**A case is a directory with a manifest** (`board:0083`), and **every artefact goes to the system temp
directory, never into the tree**. A WPT pair is upstream's file and ours only by selection, so the two
rules point opposite ways and the choice has to be made rather than discovered.

**TAKEN: a manifest per pair, in the tree, WRITTEN BY THE PREPARER AND IDEMPOTENT.** `test/wpt/css/` then
holds what `test/khronos/glTF/` holds -- one directory per case, one thin manifest each, naming the pin
and the two files and nothing else. What it buys is everything this tree does per case: `run.sh` finds
them with the same `find`, the browser lists them with the same walk, and **a pair can carry its own
declared reduction** -- which is the mechanism by which this corpus will record what the subset cannot
reach, exactly as the picture corpus does.

**REFUSED: deriving the directories into the prepared tree instead.** It keeps generated files out of
the tree and it was tempting for that alone -- but a case that exists only after preparation cannot be
cited, cannot be read, and above all cannot carry a reduction, and a corpus whose refusals have nowhere
to live is a corpus that will grow a list somewhere else instead.

**A manifest is a DECLARATION and not an artefact**, which is why this does not breach the second rule:
nothing built is committed, and the fetched files, the rendered pictures and every comparison stay in the
temp tree where the glTF corpus keeps its own.

*The generation is a subcommand of the preparer and re-runnable: widen the subset in `board:1442` and the
directories that appear are the ones the declaration now admits.*
