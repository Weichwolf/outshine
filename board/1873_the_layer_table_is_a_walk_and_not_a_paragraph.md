Type: task
State: open
Area: test
Tags: layering, claims
Parent: 1611

# The layer table refuses in the gate, not in a review

CLAUDE.md's *"layer may not spell"* table is five prohibitions and **no claim walks any of
them**. Every breach it has ever caught was caught by a human reading code, which is the
construction the tree refuses everywhere else.

Census, measured 2026-08-25 at a07fff68 — the table is nearly clean, which is what makes the
walk cheap and the red meaningful:

| rule | breaches at HEAD |
|---|---|
| any of `src/` may not spell Earth · Moon · a planet's name or numbers | `const double a = 6378137.0, e2 = 6.69437999014e-3;` (src/core/Geodesy.h:10) · `constexpr double kWgs84A = 6378137.0;` (src/data/Wgs84.h:8) · `Moon,` (src/render/plan/RenderCatalogue.h:79) |
| Ground may not spell camera · frustum · clock · LOD level · device · sun | 73 sites, all in `src/ground/World.{h,cpp}` — 59 in the body, 14 in the header |
| generator may not spell camera · neighbour part · draw list · device | 0 |
| compositor may not spell device · pipeline · texture · shader · pass | 0 |
| renderer may not spell any content noun | 7 in `src/render/plan/RenderCatalogue.h`, 1 each in `SubjectDraw.cpp` and `ShaderFile.cpp` |

`World` is the node CLAUDE.md already paints red for exactly this reason, so the claim starts
RED and is declared in `EXPECT_FAIL` with its count. That is the point: the gate then turns the
reviewer's verdict into a number, and goes red the day `World` is repaired with the declaration
still standing — a repair cannot land quietly.

The render-plan row is a QUESTION before it is a breach: TARGET draws `SUN`, `MOON` and `STARS`
as stages while the table forbids `src/` to spell Moon at all. One of the two is wrong and the
claim must not be written until it is decided.

## What will be true

- [ ] One claim walks all five rules from a table it reads, never a list repeated per layer.
- [ ] Its vocabulary is DECLARED per layer and its census is published per rule.
- [ ] It is declared in `EXPECT_FAIL` at its measured count, and the count is the work list.
- [ ] Negative control: spelling `Frustum` in a fresh `src/generators` header turns it red.
