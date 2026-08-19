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
| **cascade** | inline `style`, then `#id` rules, then `.class` rules, in that order; inheritance for `color`, `font-*`, `line-height`, `text-align` and nothing else |
| **a declared user-agent sheet** | `body { margin: 8px }`, `p { margin: 1em 0 }`, `div { display: block }` and the rest of the handful the corpus depends on -- [MEASURED] a WPT layout test states `data-offset-x="8"`, which IS `body`'s margin, so a subset without the sheet fails every such case by eight pixels and would look like a layout defect |

## What it does not do, and every line is a refusal with a reason

| | |
|---|---|
| **floats, tables, grid, multi-column** | four layout algorithms a game interface has never needed, and each is as large as flex |
| **inline layout beyond a text run** | no `inline-block`, no `vertical-align`, no inline boxes with their own margins -- the line box is where a browser's complexity lives |
| **`position: absolute`, `z-index`** | wanted, and **deferred rather than refused**: an overlay needs it and flex cannot express it, so it is a second stage with its own item |
| **specificity arithmetic** | the three-tier order above answers every rule a consumer of this engine writes, and the selector algebra is what makes a stylesheet unpredictable |
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
