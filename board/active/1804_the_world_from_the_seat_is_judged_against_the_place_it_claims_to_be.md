Type: task
Parent: 1573
Area: generators, render
Tags: driver, picture

# The world from the seat is judged against the place it claims to be

`board:1573`'s M1, and the owner's own words put it first in importance:

> *"was fehlt und besonders wichtig ist, ist die grafik der umwelt. strassen, häuser,
> vegetation. wir wollen alle outshine lib fähigkeiten zeigen. driver ist ein showcase."*

`Forest`, `Buildings`, `Water`, `Infrastructure` and `Ribbon` all exist and all are green in
the class diagram -- as ARCHITECTURE. **Nothing in the tree has ever judged how the world
LOOKS from the driver's seat.** The render corpus judges single subjects against Cycles; the
drive suites judge physics and frame cost. A still from a seat on a real road, judged against
what that road actually looks like, has never been taken.

That is the gap: every piece is present and no one has asked whether the assembled picture is
worth driving through.

## What will be true

- [ ] A still from the driver's seat on three declared routes, each carrying road, buildings
      and vegetation in the same frame.
- [ ] Each still is judged against a PHOTOGRAPH of that place rather than against itself --
      the comparison is the point, and a picture that only agrees with its own previous
      version measures nothing.
- [ ] What is missing is NAMED per still: no kerb, no road markings, untextured facades, no
      street furniture, whatever it is. A showcase's gaps are the work list.
- [ ] Proving test: the three stills, taken by the runner, with the named gaps as its output.
      Negative control: a generator disabled -> its absence appears in the named gaps rather
      than in a picture nobody looks at.
