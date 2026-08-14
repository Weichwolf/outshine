Type: task
Parent: 0127
Area: generators
Tags: oracle, scope, instrument

**The species acceptance, derived from the one species that already has it**

**The owner's standard**: *every species requires a task and a test with rendering as proof.* Roughly 330
species items exist and **not one carries an acceptance**; their entire body is a binomial and a common
name. Before 330 tasks are rewritten, the shape they are rewritten INTO has to exist and be proven — or
330 items get 330 different ideas of what proof means.

**It already exists once.** `test/render/foliage/beech/` is a case whose subject is **grown by our own
generator** from a species row — `"generator": {"shape": "grown-tree", "parameters": {"species":
"beech", …}}` — emitted as glTF, rendered by Cycles and decided by the picture bound. **The machinery is
not hypothetical; it is one case old.** This task derives the pattern from it and proves the pattern
generalises by landing a second species through it.

## What a species task's acceptance must contain, and all three or it is not proof

- [ ] **THE ROW, WITH ITS ORIGINS.** `form`, `crown`, `height_m`, `spread_m`, `height_sigma`, `dbh_cm`,
  `lai`, bark colour — each carrying its `_origin`, which is where the botany actually lives.
  `hazel.json`'s `form_origin` is four sentences of derivation and its `lai_origin` names the band and
  why the value sits inside it. **A row without its origins is a magic number in a table**, and the
  numbers are looked up against real references for the region, never recalled
- [ ] **THE GENERATOR KIND THAT READS IT**, named — `form` selects it, and a species whose form the
  grower cannot shape is a **refusal with a name**, not a silent absence. That is where a species item
  meets `III.2 Growth forms`, and it is why the two features are not the same list twice
- [ ] **THE RENDER CASE THAT PROVES IT**, and *proves* is the operative word: the subject is grown from
  the row, not authored beside it, so the case fails when the row, the grower or the picture moves
- [ ] **The verdict the case carries is declared, not assumed.** `foliage/beech` is `numeric` against the
  picture bound; a species whose oracle cannot be reduced is `self-describing` and judged by eye against
  the vegetation reference, with the residual printed and deciding nothing. **Which of the two, per
  species, is part of the task** — `board:0085` and `board:0088` already hold that ladder

## The cost, measured, because ~330 of these is the finding

| | |
|---|---|
| `test/render/foliage/beech/` on disk | **141 MB** |
| the whole render corpus, 35 cases | **4.4 GB** |
| 330 species at the beech case's product set | **≈ 46 GB** — against a 50 GB disk that already holds 19.9 GB of store |

**So the full product set per species does not fit, and the constraint is a declaration rather than a
wall.** Beech carries two recipes and five quantity passes; the `.raw` dumps are 14.7 MB each and
dominate. **A species case needs the beauty pair and the picture bound — roughly 30 MB — and nothing
else**, which puts 330 at ≈ 10 GB. The mechanism that makes that expressible is already filed:
`board:1143` moves quantity passes to a per-case declaration, and `board:0084` is the suite that is
**generated rather than typed**, which is what 330 cases must be. **A species case that declares the full
diagnostic set is the defect this paragraph exists to prevent.**

## The tail this task must not leave unexamined

[MEASURED] over `src/assets/world/species/` against every reader in the tree — the 13 declared templates
and the four declared mods:

| | |
|---|---|
| rows present | **31** |
| named by a template | 21 |
| named by a **declared mod**, as `"generator": "tree", "species": "…"` | 9 — `box` `dog_rose` `hedge_hornbeam` `log_beech` `snag_spruce` `stump_beech` among them |
| **named by nothing at all** | **4 — `dogwood` · `guelder_rose` · `hedge_privet` · `spindle`** |
| species a template names with **no row** | **22** |

**The caveat this table was written to check has fired.** Six of the ten rows that looked unreferenced are
read by `test/mods/demo/mod.json` through a **second selector** the first query could not see — exactly
the *this row is unreferenced* against *this selector is invisible to my query* distinction. **Four
survive it and are read by nothing**, and under the owner's rule they are the clearest case on the board:
a datum with no proof and no reader. **They are not deleted here** — each is either given the three-part
acceptance above or struck with the reason, and striking is the owner's.

**Done when** the acceptance above is stated once, a second species lands through it end to end, the
per-species product set is declared small enough that the corpus can hold the ladder, and the four
unread rows each have a task or a recorded reason.
