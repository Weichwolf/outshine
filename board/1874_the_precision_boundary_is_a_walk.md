Type: task
State: open
Area: test
Tags: precision, claims, render

# A float that holds a world position is refused by the gate

CLAUDE.md: *"Precision has ONE boundary and it is the camera — `Anchor - Eye` in `double`, the
model-view-projection product in `double`, and the cast to `float` only at the uniform push
(src/render/stages/SubjectDraw.cpp:841,846,854). A `float` that ever holds a world position is
a defect; a `double` that reaches a shader is a different one."*

**Nothing walks it.** The rule names its own three lines and no case reads them. The invariant
is the one that decides whether a car a thousand kilometres from the origin has a visible
wheelbase, and it is kept by memory.

Two walks, both cheap, both trivially checkable by eye:

- the cast to `float` from a world quantity happens at the uniform push and nowhere else —
  every `(float)` and `static_cast<float>` in `src/render/` whose argument names an anchor, an
  eye or a model matrix is at one of the declared sites;
- no signature in `src/` takes a world position as `float` — a `float lat`, `float lon`,
  `float eastM/northM` or `float x, y, z` naming a position rather than an offset is a defect,
  and an offset must say so in its name.

## What will be true

- [ ] The declared cast sites are read from the code, not from a constant in the claim.
- [ ] The claim publishes how many casts it judged, so a walk that saw a corner says so.
- [ ] Negative control: a `float` anchor in a fresh `src/render` header turns it red.
