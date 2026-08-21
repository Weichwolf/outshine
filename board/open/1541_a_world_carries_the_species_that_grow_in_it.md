Type: bug
Area: clients
Tags: bug

**A world carries the species that grow in it, and that is 0 or 1..N**

`Clients::ReadSpecies(const char *path, Generators::TreeSpecies *out)` opens **one file** with
`fopen` and parses it. `Sim::Assets::Species` is therefore a single species, while
`src/assets/world/species/` holds **32** -- ash, beech, birch, blackthorn, box, chestnut, dog rose,
dogwood, elder and the rest.

**This is the shape `CLAUDE.md` names outright**: *a shape is 0 or 1..N; code that assumes exactly one
of something is a defect waiting for the second*. The second is already committed, thirty-one times
over.

## What must be true

- [ ] **A world reads the species directory**, so a forest can be mixed
- [ ] **A refusal names the file it could not read.** Standing a world up printed
      `species_unreadable path=src/assets/world/species why=` -- an empty reason, because the
      directory open failed with nothing to say about it
- [ ] **The vegetation table decides which species stand where**, which is what it already carries
      104 OSM rules and 14 class rows for

## Comments

**Found by standing a world up for the first time** -- pointing `Species` at the directory refused,
and pointing it at `beech.json` opened. A world of nothing but beech is a picture nobody would accept
and every check still passes, which is why this is a bug and not a missing feature.
