Type: bug
Area: clients
Tags: dead code, closure integrity
Regresses: 1681

# The window seed at ReadInto is dead, and the closure said otherwise

1681's point 3 was closed with: "the base's window seed at ReadInto stays deliberately,
because 'the frame is the consumer's window unless declared' is the base's contract". The
adjudication is mechanically false:

- src/clients/Engine.cpp:171 `out.Render.Frame = S_->Frame;` runs BEFORE
  `ReadScenario(text…, out, …)`.
- ScenarioRead.cpp:226 — ReadScenario's first statement is `into = Scenario();`, which
  resets `out` entire, Frame included, before any attribute is read. The seed never
  survives to the reader's keep-the-standing-value defaults; an undeclared widthPx reads
  onto 0, not onto the window.
- The contract the closure invokes is real, but it is enforced ONLY at Declare
  (Engine.cpp:122-125, `WidthPx > 0 ? declared : S_->Frame`), exactly as 1681's original
  point 3 said.

Demanded: delete Engine.cpp:171 (the fallback at Declare is the one truth), and correct
1681's record — a closure resting on a false mechanical claim is worse than an open bug,
because the next reader trusts it.

---

Closed, and the record set straight: the seed WAS dead code -- ReadScenario resets before
any reader could see it, so the 1681 note's "stays deliberately" described a mechanism that
did not exist. The line is deleted; the window-default contract lives where it always
actually ran, in Declare's fallback. A closure note that misstates the mechanism is worse
than the bug it closes -- on the record here.
