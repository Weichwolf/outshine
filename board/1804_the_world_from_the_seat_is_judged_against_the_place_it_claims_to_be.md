Type: task
State: open
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

- [x] A still from the driver's seat on three declared routes, each carrying road, buildings
      and vegetation in the same frame.
- [ ] Each still is judged against a PHOTOGRAPH of that place rather than against itself --
      the comparison is the point, and a picture that only agrees with its own previous
      version measures nothing.
- [x] What is missing is NAMED per still: no kerb, no road markings, untextured facades, no
      street furniture, whatever it is. A showcase's gaps are the work list.
- [ ] Proving test: the three stills, taken by the runner, with the named gaps as its output.
      Negative control: a generator disabled -> its absence appears in the named gaps rather
      than in a picture nobody looks at.

## The stills exist, and what they show is the finding (2026-08-24)

`apps/driver/stills` gained the budget arm of `board:1778` -- it was killed at 120 s having
written nothing -- and now writes what it reaches:

```
SPENT the budget at 113.9 km of 753.6 after 60.9 s, with 6 of 12 stills written
NOT JUDGED the 9 stations beyond 113.9 km -- the budget was spent there
```

Three places on the Munich--Hamburg route, first person and third:

| what the picture holds | what it does not |
|---|---|
| the F31, well modelled: glass, mirrors, plate, badge, tail lights | any building |
| a road ribbon, correctly cut and filled into the terrain | any vegetation -- no tree, no grass, no hedge |
| a sky gradient | any ground texture; one flat green from verge to horizon |
| a shadow under the car | any road marking, kerb, edge line or verge treatment |
| | any sun disc, cloud or weather |
| | visible terrain relief in these three places -- **whether that is the data or the draw is not answerable from a still on the Munich gravel plain, and needs a place with relief** |

**The picture is correct for what is declared.** `f31.scenario` has no `<world>` element: no
sphere, no ground, no sky, no clock, no weather. The green plane and the grey ribbon are built
by the case itself, in its own C++.

That turns this item from an app backlog entry into the architecture finding filed as
**board:1805**: six green nodes compose the world in the CURRENT diagram and no consumer in the
tree walks the path.

## Parked, with the reason named

The owner's standing direction: *"nur an driver arbeiten, wenn die outshine architektur keine
offenen tasks hat und die claude.md diagramme ist = soll zeigen"* -- and *"du darfst aber
driver nutzen um die architekturarbeit voranzutreiben"*.

This item did exactly the second. Its remaining box -- judging a still against a photograph of
the place -- is product work and waits behind `board:1805`, because judging a picture against a
photograph is pointless while the picture is built by the client rather than composed by the
engine.
