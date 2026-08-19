Type: feature
Area: clients
Tags: instrument

**The browser draws itself with the engine it shows**

A test-case browser: choose a suite, see every case the tree declares, select one, watch it rendered
live. **It is the UI engine's first consumer and not a second drawing program** — it declares its own
surface as markup and style, hands that to outshine, and outshine paints it; for a case's picture it
configures outshine from the manifest. Two targets of one renderer, never two renderers.

## The hard test of that sentence

**There is no drawing instruction in the browser**: no shader, no draw call, no pipeline, no colour
that is not in a declaration. That is greppable, so it is a test and not a hope — `test/viewer/` is
read the way `EveryPathCitedInADocumentResolves` reads this file.

**The client owns the window and the surface and hands outshine a pointer to it.** There is no
readback path, because it would be nothing but overhead. Whether that surface is a swapchain image or
a texture of the client's own is the CLIENT's declaration and the library cannot tell — which is what
makes the same binary the browser and the test of the browser.

## What must be true

- [ ] **Every case the tree declares is reachable** — 309 today across three suites under one manifest
  format. A case whose preparation has not run is COUNTED as unprepared, never hidden: a browser that
  shows only what happens to be on disk answers a different question than *what does this tree declare*
- [ ] **A document case is drawn by the same engine that draws the browser's own chrome.** The viewer
  and the viewed are one mechanism, and that is a property held on purpose
- [ ] **Interaction is a hit and a declared action.** The pointer event yields the element and the
  `data-action` it carries; what the action MEANS is the browser's, and no callback is registered with
  the library
- [ ] **The browser's own chrome pays a numbered price beside the case's**, at p50/p95/p99, and the
  frame path allocates nothing

## Where a missing control goes

**A list, a scrolling pane, a splitter, a toolbar, a status line and an input are DECLARATIONS and not
engine concepts.** Where one cannot be spelled in the subset, the subset grows — and the growth is
proven upstream and never by a screenshot: the next WPT family is pinned and fetched through the same
preparer, `css-overflow` and `css-sizing` first because they carry what a case list and a scrolling
pane need. Selection stays derived rather than curated, the harness comes before the capability, and
two counts stand side by side.

## Named outside

A second manifest format · a widget vocabulary in the library · a drawing path in the viewer · Blender
and Cycles. **The browser needs no oracle because it judges nothing.**
