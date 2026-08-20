Type: task
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
