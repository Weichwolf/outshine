Type: task
State: open
Parent: 1480
Area: render
Tags: scope

**A view is one of several, and time runs at its rate**

`Views` are read and carried and the renderer has one camera. **A first-person view, an aimed view that
slows time, a third-person view, a map view and a terminal view are one mechanism** -- a view follows
something, sits at an offset, has a field and a time scale.

## What must be true

- [ ] **A scenario declares 0 or 1..N views and exactly one is active**, and which is answerable
- [ ] **A view FOLLOWS an instance**, so the camera is not a thing the client drives frame by frame
- [ ] **`timeScale` scales the CLOCK and not the frame** -- `CLAUDE.md`: *if pace decides the result,
  the coupling is a bug*, so a slowed view still lands its frames and the world advances less
- [ ] **The active view is the audio listener**, which is `board:1486`'s ear and needs no second
  declaration
- [ ] **Switching a view costs no stand-up**, because a game switches on every aimed shot

---

Progress -- four of five boxes stand in src/scenario/ViewBook: 1..N declared views with
refusals (duplicate id = a coin toss, follows-nothing, a person the engine does not
declare, timeScale <= 0); EXACTLY ONE active, answerable, seeded from the player's declared
starting view (an undeclared start refuses naming it); Take() is one index so switching
costs no stand-up; ClockScale() is the active view's timeScale, to be multiplied into the
world's ADVANCE (never the frame); ListensFrom() hands the audio listener from the same
seat, no second declaration. Proving test:
unit/scenario/AViewIsOneOfSeveralAndTimeRunsAtItsRate.cpp. Remaining: wiring ClockScale
into Engine::Advance and the renderer camera following the instance -- the integration
residue that lands with the camera work.
