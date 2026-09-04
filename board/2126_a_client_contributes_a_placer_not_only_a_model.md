Type: debt
State: open
Area: include, generators
Tags: architecture, door

# A client contributes a PLACER, not only a model

**Benchmark** -- Unreal: a procedural foliage SPAWNER is the authored unit -- it decides where
instances go -- and the mesh it spawns is a separate asset; a plugin can ship either. RAGE: prop
placement is data (`ipl`) beside the drawable (`ydr`), two things, and a game module supplies
both. **Both agree**: what a thing looks like and where it goes are two contributions, and a
door that accepts only the first accepts half a generator.

## Where it stands, measured 2026-09-04

`include/generate/Generate.h` offers `Generator{kind(), make(Request, Geometry &), stamps(...)}`
and `Registry{offers, named, count}`. That is a MODEL asked for by name. The placement vocabulary
-- `Making`, `Yield`, `Claim`, `Ground`, `Rank`, `OccupancySink` -- stands in
`src/generators/base/` and appears nowhere in the door. So a client can say what its tree looks
like and cannot say where its trees go.

Found while closing board:2110, which cut the generators by subject and enforced the cut with a
`reaches` per area; the directory question is closed and this is the door question it left.

## What will be true

- [ ] The door names the placer: an interface a client implements that is asked for a region and
      answers with claims, the way `Forest::Occupy` does inside the tree
- [ ] The rank a placer runs at is declared beside it, because a placer that runs after another
      sees ground already taken and the order decides the picture
- [ ] `Making`, `Yield`, `Claim` and `Ground` are either published as the vocabulary of that
      interface or hidden behind a narrower one -- the item states which and why, before the
      header is written
- [ ] A case registers a placer through the door alone and a tile it was asked for carries its
      claims
- [ ] Negative control: unregister it and the claims are gone

## What will show I was wrong

If the vocabulary cannot be published without publishing `OccupancySink`'s storage, the placer is
not ready to be a door type and this item says so instead of widening the door.
