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

## The paint layer, the pointer, pages and a real face are built and measured

**`src/ui/Paint.{h,cpp}`** turns a laid-out declaration into rectangles and still names no device.
The background covers the border box with four edges over it — four widths are four declarations and
one rectangle behind the background cannot spell a thick left side. A glyph is a rectangle and a
space is not; **Ahem needs no atlas at all**, which is why the measurement face also paints. The
initial clip is the TARGET and never the root box, or a page that overflows erases itself. A quad its
clip excludes entirely is not emitted, which is what makes a page of a long document cost the page.

**`src/ui/Pointer.{h,cpp}`** answers what was touched: the element, the nearest declared
`data-action`, which element declared it, and where inside the box the point fell. **There is no
callback registered with the engine and that is the design** — a library that invoked the consumer's
function would decide when the consumer runs, on which thread, inside which frame; a stage signals
readiness and never asks. A box clipped away is not under the pointer, because the viewer cannot see
it and that is the one answer a pointer must never give.

**Pages cut at line boundaries.** A line that would cross the bottom begins the next page and the page
before it ends short. A line taller than the page still gets one, overflows it and is **counted** — a
break that cannot advance is a loop with no bound, and dropping the line is the other way to make it
terminate.

**A real face fits through the interface, and Ahem could not have proved it.** Ahem is monospace, so
every claim held with it is also true of an engine that divides a width by one advance and calls it a
measurement. `Glyph::AdvancePx` moves the pen per glyph, the wrap walks until the room runs out, and
a word wider than the line is placed WHOLE — taking one character at a time is a cut inside a word
spelled once per glyph. [MEASURED] with a proportional face `mmmm` at 20 px measures **80 px** where
the monospace reading answers 40, and the second glyph of `im` begins at **5** rather than at 10.

## What it costs, with its population and its domain

[MEASURED] 400 frames of a HUD **re-read from source every frame** — 69 boxes, 284 quads, 1280×720:

| | p50 | p95 | p99 | max |
|---|---:|---:|---:|---:|
| before the per-glyph walk | 0.0944 | 0.1315 | 0.1573 | 0.2235 |
| after it | **0.1105** | 0.1507 | **0.1520** | 0.1815 |

**The domain is stated with the number**: reading, cascading, measuring, placing and turning boxes
into rectangles. Uploading and drawing them is the renderer's and is not in this figure. Against a
`[SET]` tenth of 16.67 ms, p99 uses **9.1 %** of that share — **0.91 % of the whole frame**. The
per-glyph walk cost **+0.016 ms at p50** and bought proportional text; that is the trade, priced.

*The re-read-every-frame population is the pessimistic case on purpose. A consumer rebuilds when
something changed, so a real HUD pays less than this — but a budget checked against the cheap case is
not checked.*

## The path reaches a pixel: `Stage::Overlay`

**It CONTRIBUTES and does not write**, so the picture exists without it — which is what makes a HUD a
declaration rather than a requirement, and why it costs a plan that does not ask for it nothing. It
sits **after** the display transfer: a consumer's colour is display-referred, and a panel put through
the transfer would come out a colour nobody declared.

**`Resource::OverlayAtlas` is `Given`, like a sampler.** The consumer's image, which the engine holds
and never makes. One white texel until it is handed something, so *no atlas* is a value and not a
branch, and a binding is never left empty — which is a validation error on some drivers and a black
interface on others.

**One draw for the whole interface.** The six corners are arithmetic and the rectangle is data, so
every quad is an instance; the clip is a per-quad **discard** rather than a scissor, because a scissor
is pass-level state and honouring per-rectangle clips with one would mean as many passes as clips.
The buffer is allocated once at the bound and reused, so a HUD whose length varies allocates in the
first frames of a run and on no frame after that.

**The renderer takes `OverlayQuad` and not `Ui::Quad`, and that is the layering rather than an
inconvenience.** No content noun has a spelling in the renderer; a box, a glyph and a page are the
UI's vocabulary. The consumer translates — a loop it already owns — and either end stays replaceable.

### What the device answered, and every number is the declaration's own

[MEASURED] `test/outshine/shader/TheOverlayPutsEachRectangleWhereItWasDeclared.cpp`, 33 of 33:

| claim | what came back |
|---|---|
| a rectangle lands where it was declared | its first and last texel are the declared colour, and one pixel outside each edge is the clear |
| the clip is per rectangle, in one draw | half of a 40×40 quad past a 20×20 clip never reaches a texel |
| the blend is premultiplied | one half-opaque white over black reads **128**; a second over it reaches **191**. A straight-alpha blend gives the same 128 and darkens the seam, so the second number is the one that separates them |
| the radius reaches the pixel | the middle is filled and the corner texel is not |
| the bound refuses | a list three past it comes back with *3 past the bound* in the refusal |

*The two-quad number is the claim that costs something to make: a single translucent quad cannot tell
a premultiplied blend from a straight one, and a suite that only checked it would have passed either.*

## What is still open on this item

- the client-side translation from `Ui::Quad` to `OverlayQuad` — ten lines, and it is the client's by
  design, so it lands with the first client that draws an interface
- `position: absolute`, deferred and carrying its own item
- nine red corpus cases, all genuine flexbox depth
