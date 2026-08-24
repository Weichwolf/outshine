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

- [x] **A world reads the species directory**, so a forest can be mixed
- [x] **A refusal names the file it could not read.** Standing a world up printed
      `species_unreadable path=src/assets/world/species why=` -- an empty reason, because the
      directory open failed with nothing to say about it
- [ ] **The vegetation table decides which species stand where**, which is what it already carries
      104 OSM rules and 14 class rows for

## Comments

**Found by standing a world up for the first time** -- pointing `Species` at the directory refused,
and pointing it at `beech.json` opened. A world of nothing but beech is a picture nobody would accept
and every check still passes, which is why this is a bug and not a missing feature.

---

## Repaid, boxes 1 and 2, and the DEFECT itself (2026-08-24)

The item's title is the shape, and the shape is now honoured in all three places it was
broken -- reader, generator and holder:

| | before | after |
|---|---|---|
| `ReadSpecies` | `fopen` on ONE path, `bool` verdict, no reason | file OR directory; a directory answers every `.json` it holds, sorted; every refusal names the path |
| `Forest` | `Stem Stem_` -- one species | `std::vector<Stem> Stems_`, `kMostSpecies = 64`; the species is drawn from the region's own seed stream (a fourth stream per cell) |
| `Sim` | `Generators::TreeSpecies Species_` | `std::vector<Generators::TreeSpecies> Species_` |

**Measured:**

| | |
|---|---|
| species the tree carries | **31** `.json` under `src/assets/world/species/` |
| species the reader returns | **31** (was 1) |
| a stand of three declared stems | 198 922 trees in **3** distinct height bands (was 1) |

The refusal that filed this item -- `species_unreadable path=src/assets/world/species why=`
with an EMPTY reason -- now reads:

```
the species file 'src/assets/world/species/nosuchtree.json' does not read: it could not be opened
```

- **Proving tests**: `test/unit/clients/AWorldReadsTheSpeciesThatGrowInIt` (the directory, the
  single file, the missing file, the directory with no `.json`, the empty path -- each refusal
  naming its path) and the mixed-stand arm of
  `test/unit/generators/SameRegionSamePlacement` (three stems, three height bands, and the
  same ground growing the same wood twice).
- **Negative controls**, both run: the directory branch disabled -> `REFUSED the species file
  'src/assets/world/species' does not read`, the reader claim red; the species draw pinned to
  `Stems_.front()` -> **1** height band, the mixed-wood claim red. Both reverted.
- Which species stands where is a PURE function of `(region seed, cell index)`, so the wood
  is reproducible: the test grows the same ground twice and compares every tree.

## Still open: box 3

**The vegetation table does not yet choose.** `VegetationTemplates::Row` carries colours and
`Rule` carries road geometry; neither names a species, so there is nothing for a class row to
select FROM. Box 3 is a feature -- a species list per ground class -- and it needs the table
to gain a column before the forest can read it. Left open with that named reason rather than
ticked because the other two landed.
