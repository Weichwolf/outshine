Type: feature
Area: render
Tags: scope

**A positioned box is offset from where the flow put it**

`position: relative` with `top`, `right`, `bottom` and `left`, and then `absolute` against the nearest
positioned ancestor. **It is deferred and not refused** — `board:1442` says so in its own subset, and
this is the item it points at.

## What must be true

- [ ] **A relatively positioned box is laid out where the flow put it and then MOVED**, so the space it
  occupied stays occupied. That is the whole of `relative`, and getting it the other way round is a
  layout that quietly reflows everything around it
- [ ] **An absolutely positioned box is out of flow** and is sized and placed against its nearest
  positioned ancestor's padding box — which the layout already publishes as `Box::Positioned`, because
  the corpus harness needed it to read an offset the way `offsetLeft` does
- [ ] **A stacking order comes with it or the picture is undefined.** Painter's order is the whole of
  depth today; an out-of-flow box is the first thing that can be *behind* something it comes after, so
  `z-index` is part of this item and not a later one
- [ ] The corpus decides it: `position:absolute` is a declared boundary today, on 12 cases of the CSS
  suite, and closing this item is what turns those from reduced into attempted

## Why it was deferred rather than built

Out-of-flow boxes are the first thing in this subset that breaks *the list is the depth*: everything
else paints in tree order and cannot be behind anything. A game's interface can be built without it —
a HUD, a quest log, a book page and a terminal are flow and flexbox — so it is the one capability whose
absence costs a declaration a shape rather than a possibility.
