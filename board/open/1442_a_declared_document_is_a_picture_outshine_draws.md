Type: feature
Area: render
Tags: scope

**A declared document is a picture outshine draws**

A consumer declares an interface as markup and style; the engine lays it out and draws it. **The engine
spells the verbs -- measure, wrap, place, clip, paint -- and the document spells the nouns**, which is
the same decomposition every other part of this tree already obeys.

**IT IS A FLEXBOX SUBSET AND NOT A BROWSER, AND THAT IS THE WHOLE OF THE DESIGN.** *HTML light* is the
best-known tar pit in this field: the light version grows a float, then a table, then specificity
arithmetic, then a font fallback chain, and two years later it is a browser that has never been asked to
hold a frame. So the subset is DECLARED here, in both directions, and the corpus is selected from the
declaration rather than the declaration grown from whatever the corpus happened to contain.

## What the engine does

| | |
|---|---|
| **document** | elements with `id`, `class` and inline `style`; text nodes; `<img>` pointing at a texture the consumer supplies |
| **box model** | `width` `height` `min-*` `max-*` `padding` `border-width` `margin`, `box-sizing: content-box` (**CSS's own default, because the corpus assumes it**) and `border-box` |
| **display** | `block`, `flex`, `none` |
| **flex** | `flex-direction: row \| column`, `justify-content`, `align-items`, `align-self`, `flex-grow`, `flex-shrink`, `flex-basis`, `gap`, `flex-wrap: nowrap` |
| **sizing** | `px`, `%`, `auto`, `em`, `rem` |
| **position** | `static`, `relative` |
| **paint** | `background-color`, `border-color`, `border-radius`, `opacity`, `color`, `overflow: hidden` |
| **text** | one family at a time, `font-size`, `line-height`, `text-align: left \| center \| right`, `white-space: normal \| pre`, wrapping at spaces |
| **cascade** | selectors are a comma-separated list of compound selectors -- a tag name, `.class` and `#id` in any combination -- with the descendant combinator, ordered by **CSS specificity** and then by document order, with inline `style` above all of it; inheritance for `color`, `font-*`, `line-height`, `text-align` and nothing else |
| **a declared user-agent sheet** | `body { margin: 8px }`, `p { margin: 1em 0 }`, `div { display: block }` and the rest of the handful the corpus depends on -- [MEASURED] a WPT layout test states `data-offset-x="8"`, which IS `body`'s margin, so a subset without the sheet fails every such case by eight pixels and would look like a layout defect |

## What it does not do, and every line is a refusal with a reason

| | |
|---|---|
| **floats, tables, grid, multi-column** | four layout algorithms a game interface has never needed, and each is as large as flex |
| **inline layout beyond a text run** | no `inline-block`, no `vertical-align`, no inline boxes with their own margins -- the line box is where a browser's complexity lives |
| **`position: absolute`, `z-index`** | wanted, and **deferred rather than refused**: an overlay needs it and flex cannot express it, so it is a second stage with its own item |
| **the rest of the selector algebra** | `>`, `+`, `~`, attribute selectors, `:hover` and every other pseudo-class, `*`. **Specificity itself is IN, and that is a correction the corpus forced**: [MEASURED] `css/css-flexbox/align-content-horiz-001a.html` writes `div.flexbox` and `auto-margins-001.html` writes `#circles, #circles div`, so a three-tier order would have mis-ranked the corpus's own rules on the first file read. Specificity is ten lines of arithmetic; guessing at it is a wrong picture |
| **transforms, shadows, filters, gradients, animations, transitions** | paint effects, each one a pass; the engine already has a place for a pass and it is the render plan |
| **bidi, hyphenation, ligatures, shaping, font fallback** | the text problem in full, and it is a field of its own |
| **scripting, events, forms, links** | a document is data here; what a click means is the consumer's |
| **`calc()`, viewport units, `min()` `max()` `clamp()`** | an expression language inside a value, and every one of them is a parser |

## How it is judged, and there is no oracle in it

**The corpus is WPT's CSS reftests**, pinned like every other upstream in this tree. A reftest is a pair
of declarations written differently that **must render pixel-identically** -- and both sides are rendered
by this engine, so there is no reference renderer, no tolerance, no perceptual bound and no Blender. The
comparison is bitwise or it is nothing.

**A `match` pair alone is a fixed point a renderer that draws NOTHING satisfies**, so the suite is not
built out of them alone:

- **`mismatch` pairs**, which must differ -- the half an empty renderer cannot fake
- **anchored cases**, few and ours, stating where a box actually lands: *this rectangle covers
  (100, 50) to (300, 70)*. Without them the suite measures self-consistency and calls it correctness

**The selection is DERIVED, not curated**: a WPT test is in the corpus when every element, property and
value it uses is in the table above -- which the harness can answer because it has the parser. **Two
counts are published side by side**: how many selected pairs hold, and how much of the suite the subset
reaches. Neither stands for the other, which is the same rule the picture corpus already lives under.

## Comments

`Ahem` is the font WPT uses for exactly this reason -- every glyph is a solid block of known metrics, so
a layout test is not a font test. It is a handful of rectangles to implement and it is what makes
pixel-identity a statement about layout rather than about rasterisation.

## Comments

**Three layers stand and are measured against upstream's own corpus** (`board:1443`, `board:1444`):
the markup reader, the stylesheet reader and the layout. `wpt: subset 14 inside of 138, layout 5
held, 9 red of 14` — and the first number moved 10 → 0 → 27 → 22 → 14 across this round as the
subset statement was completed, every step in the direction of claiming less.

**What the corpus found in the engine, each one a class rather than a case:**

| repair | [MEASURED] |
|---|---|
| `flex: <number>` expands to `<number> 1 0%` | a bare `0` is a number with no unit and `Resolve` answers absent, so the basis fell through to the item's declared height — `flex: 1` on a 5 px item took 5 px of a 300 px column instead of 135 |
| the flex container lays out **lines** | one line cannot spell `align-content` at all; `flexbox-lines-must-be-stretched-by-default` held the moment lines existed |
| `head, title, link, meta, style, script` draw nothing | `<title>` was laid out as two lines of 19.2 px and pushed `<body>` from y = 8 to y = 46.4, which read as an `align-content` defect in three cases |
| a run that collapses to nothing but spaces is removed | CSS deletes collapsible whitespace between block boxes rather than giving it a line box |
| centring an overfull line starts before the container | the corpus states a **negative** offset for it; flooring the slack at zero reads as tidy and is a different layout |
| every part of a compound selector is checked | `.item::first-letter` parsed as a class *named* `item::first-letter`, matched nothing, and counted as nothing |
| a comment inside a declaration block is not CSS | its own delimiters and the words out of its prose were counted as properties this engine lacks |
| `flex`, `flex-flow`, `background` and `border` expand | `flex:` alone was **161** declarations read as a gap |

## The paint layer's shape, so the next round starts from a design rather than a blank file

**One verb and no taxonomy**: *draw this rectangle, with that patch of that image, at that tint.* A
button, a name, a health bar and a debug histogram are the same three numbers, which is what keeps a
content vocabulary out of the renderer.

- `OverlayQuad` — `LeftPx TopPx WidthPx HeightPx`, `U0 V0 U1 V1`, `Tint[4]`. **A quad whose atlas
  rectangle has no area draws from the tint alone**, so a solid panel needs no white texel to point at
- **the atlas is the consumer's image and the engine only holds it** — a font is an asset, and who
  makes an asset is not the engine's business
- it **`Contributes`** and does not `Write`: a plan that declares no overlay is exactly the plan it
  was, so this costs a picture nothing that does not ask for it
- `[SET]` **8192 quads**, refused beyond with the overage named — a silently truncated interface draws
  a picture nobody declared. Eight thousand rectangles is a full screen of eight-pixel glyphs several
  times over
- `SetAtlas` and `SetQuads` upload **outside** the pass, the shape `SetMesh` already has, so the frame
  path binds and draws and touches no allocator

*A header carrying this interface with no implementation behind it was written and then removed
rather than committed: half-built is worse than not built, and the design is worth more here than a
file nothing compiles.*
