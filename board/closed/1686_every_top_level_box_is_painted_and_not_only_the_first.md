Type: bug
Area: ui
Tags: paint

**Every top-level box is painted, and not only the first**

`Layout::Build` places every element child of the document root with `parentBox = -1`
(`src/ui/Layout.cpp:1044-1047`), so a markup fragment with two top-level elements yields two
parentless boxes. `Painting::Build` walks only box 0 (`src/ui/Paint.cpp:174`,
`painter.Walk(0, …)`) — every later top-level box silently produces no quad.

Proven:

    <div style="width:50px;height:20px;background:red"></div>
    <div style="width:50px;height:20px;background:blue"></div>

→ `boxes=2 quads=1`, the blue box vanishes without a word. `Layout::Hit` meanwhile iterates
ALL boxes, so the pointer answers for a box the painter never drew — two truths about the
same tree. A surface declared through `Live::Compose` carries exactly this shape when the
author writes a fragment instead of a single wrapper; the house rule is a loud failure or a
correct picture, never a silent drop.

Demanded: Paint walks every parentless box in order (or Layout roots them under one box, in
ONE place), and a unit case in `test/unit/ui/` with two top-level elements asserting both
backgrounds appear.

---

Closed: the painter walks EVERY rootless box in box order -- a fragment of two top-level
divs paints two quads, and what Hit answers for is what the eye sees. Negative-controlled:
with Walk(0) restored the new proof (the second div's quad at its own place) goes red.
