Type: feature
Area: render
Tags: scope

**A declared surface is an asset that renders into its own texture**

**Beside the glTF loader, a surface loader.** `src/gltf/` reads a declaration into a subject; `src/ui/`
reads one into boxes and quads. Two content surfaces, one renderer — which is what `CLAUDE.md` already
says about the content surface being *one interface*, applied a second time.

**THE TEXTURE IS WHAT MAKES THE THREE USES ONE PATH.** A surface renders into its own texture, and what
happens to that texture afterwards is a second declaration the consumer makes: composited over the
frame for a HUD or a menu, or bound as a material's base colour for a screen on a wall, a book in a
hand, a terminal in a corridor. **A screen, a book and a HUD are not three renderers; they are one
renderer with three targets, and the client chooses which.**

*Today's overlay is welded to `FrameTex`, which is the weaker shape: it can only be a HUD. This item
is what unwelds it.*

## What must be true

- [ ] **The surface's texture is a `Resource` the plan declares**, and the overlay stage CONTRIBUTES to
  it. Whether it then reaches the frame is a composite the consumer declares, and whether it reaches a
  material is a binding the consumer makes. **A second render path beside the compiled plan is the one
  thing this may not become**
- [ ] **A surface is an asset and not a document with a lifetime.** Pinned, digested and cached in the
  same content store a glTF uses. **No navigation, no fetch, no timer of its own** — every effect it
  can have goes through a native the client registered, which is what `board:1448` already holds
- [ ] **Every growing number names its bound.** Ten screens in a world are ten layouts and ten passes a
  frame if nothing says otherwise, so a surface declares its **size** and is re-drawn on **change**
  rather than per frame. That is the one real risk in this design and it is written down before it is
  built
- [ ] **The interaction path is the one that exists**: a pointer in surface coordinates, a hit, the
  action the element declared, and a call into a native the client owns. **A surface on a wall needs a
  ray-to-surface coordinate first**, and that is the consumer's — the library takes a point

## What this is not

It is not a browser. **The subset is the contract** (`board:1442`), it is written down in both
directions, and it is measured against upstream's own corpus — 162 cases, every one held or reduced at
a boundary this engine declared. Calling the format HTML says how a page is SPELLED and never what the
engine promises to do with it.

## Comments

The owner proposed this shape and it is a better one than the tree has: the current overlay can only be
a HUD because it contributes straight to `FrameTex`. The three sharpenings above are mine — the plan
resource, the no-lifetime rule, and the refresh bound — and the third is the one that would otherwise
be discovered as a frame-time regression rather than as a decision.
