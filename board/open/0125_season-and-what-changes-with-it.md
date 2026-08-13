Type: feature
Area: generators
Tags: perf, instrument

**II.10 Season, and what changes with it**

- [ ] Day-of-year reaching anything at all — no `season` in the tree
- [ ] Leaf-on / leaf-off state per species with its own phenology
- [ ] Autumn colour per species, with the sequence right (ash early and dull, beech copper, larch late gold)
- [ ] Leaf fall and a litter layer that thickens
- [ ] Bare-crown silhouette with branch structure legible — the crown geometry already exists, so this is cheap
- [ ] Spring flush with a lighter, yellower leaf
- [ ] Snow lying on branches, roofs and the ground with a slope mask
- [ ] Crop calendar: sown, green, eared, ripe, harvested, stubble, ploughed
- [ ] Meadow cut state: standing, mown, windrowed, baled, regrown
- [ ] Grass senescence — the dry fraction exists per template and is not driven by a season
- [ ] Water level and flow varying by season
- [ ] Ice on a pond

---

## Band III — Vegetation

*The reference is a vegetation picture: canopy plus undergrowth plus grass, superposed from one
declared preset, plus mushrooms and herbs. Ours is one stem class and a stands-per-m².*

**Form before species, and the split is the band's whole argument.** A **growth form** is a shape the
generator must be able to make at all; a **species** is a declaration carried by a form. A species line
is cheap once its form exists and impossible before it, so forms stand first and every species section
below names the form it rides. Where a species needs a form nothing else uses, the line says so — that
is the expensive kind.

**Measured state:** 31 species files exist across seven growth forms — single-stem tree, multi-stem
shrub, bush, hedge, snag, stump and fallen log — and the grower takes the form as an input
(`generators/GrowthForm.h`). What no form yet has is a **cut response**, a severed shoot answering with
several, which is why the hedge reads as a row of saplings and why coppice stool, pollard and
stump-with-resprouts are all still blocked on one mechanism.
