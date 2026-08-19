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

**TAKEN: a manifest per pair, in the tree, WRITTEN BY THE PREPARER AND IDEMPOTENT.** A directory named
for the project whose tests these are -- wpt, under it css -- then holds what `test/khronos/glTF/` holds -- one directory per case, one thin manifest each, naming the pin
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

## The corpus was read before it was planned, and it says three things

**WPT AT `550efd4a2f1f14877d424b5668fbb6e63d5f6165`**, 2026-08-19. `css/css-flexbox` alone holds 994
files, 212 of them references.

**1. THERE ARE TWO FAMILIES AND THE SECOND IS THE STRONGER ONE FOR LAYOUT.** Besides the reftests there
is a family carrying its assertions IN THE MARKUP: `data-expected-width`, `data-expected-height`,
`data-offset-x`, `data-offset-y` on the elements themselves. [MEASURED] `align-content-horiz-001a.html`
states every box's size and position that way. **Those are the anchored cases this task said it would
have to invent -- and upstream already wrote them.** They need no painting, no font rasterisation and no
pixel comparison: a layout engine can be judged by them the day it exists, and it can be RED against them
before a single quad is drawn.

**2. A REFTEST'S REFERENCE IS DELIBERATELY WRITTEN WITH OTHER FEATURES, which is what makes the family
expensive here.** [MEASURED] `auto-margins-001.html` is a flexbox test whose reference renders the same
picture with a `<table>`; the test itself uses `calc()` and `writing-mode: vertical-rl`. **A pair is in
the subset only if BOTH files are**, and a reference chosen to be independent of the feature under test
will reach for exactly the features a subset leaves out. So the reftest family will select thin, and
that is a property of the family rather than a fault of the subset.

**3. THE TESTS DEPEND ON A UA STYLESHEET AND SAY SO IN THEIR NUMBERS.** `data-offset-x="8"` is `body`'s
own 8 px margin; `<p>` carries `1em 0`. **A subset with no default sheet fails every one of these by
eight pixels**, so the sheet is part of the declaration and not a detail -- `board:1442` gains it.

## What follows for this task

- [ ] the corpus carries **two case kinds**, both declarative and both in the one manifest format:
      `layout-assertions` -- one document, numbers from upstream -- and `reftest` -- two documents,
      bitwise
- [ ] the `layout-assertions` family is fetched and selected FIRST, because it is what a layout engine
      can be measured by while it is being built
- [ ] the fetch follows a test's own `<link rel=stylesheet>` and `<script src>` references, because
      `support/flexbox.css` and `/fonts/ahem.css` are part of the case and not of the browser

## Comments

The layout-assertion family is in: **138 cases of 1328 tests** under `css/css-flexbox` at the pin,
derived by asking every file whether it states its own layout and never by a list somebody wrote.

**The enumeration had to be taken twice, and the first one was wrong by more than half.** GitHub's
contents endpoint caps a directory listing at 1000 entries and says so nowhere in the payload — it
reported **799** html files where the git-tree endpoint, which publishes `truncated`, reports
**1766**. The generator refuses a truncated tree for exactly this reason.

**`mismatch` is all but absent from this directory: 1 of 765 reftests.** So the empty-renderer
fixed point cannot be defeated here by pairing alone, and the anchor is the layout-assertion family
itself — an absolute offset in the viewport is a claim no pair of identical blank pages can satisfy.
A `mismatch` slice has to come from elsewhere in `css/`, and that is worth its own measurement
before the reftest arm is built.

**The manifest is one format with two variants.** `outshine/ui-case-manifest` carries no `blender`,
no `scene` and no `renders`, and the root object discriminates on `schema` — so a document case
cannot silently omit an oracle field a render case needs, and a render case cannot carry a Blender
version for a case Blender never sees. 283 manifests across both suites validate.

**`year` left the licence's required set.** Every web-platform-tests file names a licence and not one
names a year; the alternative was writing a year the source does not carry into the field that exists
to record where a claim was made.

## Two more families, and what they measured is that this instrument has reached its edge

`css-overflow` and `css-sizing` were fetched at the same pin, because two families measured at two
commits are two corpora. [MEASURED]:

| family | tests at the pin | state their own layout | inside the subset |
|---|---:|---:|---:|
| `css-flexbox` | 1328 | 138 | 17 |
| `css-overflow` | 385 | **5** | 0 |
| `css-sizing` | 317 | 19 | 0 |

**`css-overflow` is judged by pictures and not by numbers**: 380 of its 385 tests state no layout of
their own at all. **Of the 24 cases the two families do contribute, 18 are script-driven** — the
document assigns its own geometry in `window.onload` — and the rest reach `float` (10) and the `font`
shorthand (6).

**So the next capability is the reftest arm and not another layout-assertion family.** That is the
opposite of what the round expected to find, and it is what the numbers say: the instrument that reads
`data-expected-*` has taken what upstream states that way, and everything past it is a pair of
pictures. `board:1444` is where that goes, and its own measurement — one `mismatch` in 765 reftests of
`css-flexbox` — says the pairs have to be drawn from `css/` more widely than one directory.
