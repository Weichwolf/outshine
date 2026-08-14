Type: task
Parent: 0127
Area: generators
Tags: scope

**Twenty-two species a declared template names and the tree has no row for**

The species requirement, derived from the declaration instead of listed. [MEASURED] over
`src/assets/world/vegetation.json` and `src/assets/world/species/`:

| | |
|---|---|
| templates declared | **13** |
| distinct species those templates name | **43** |
| species rows present | **31** |
| **named by a template and missing a row** | **22** |
| rows present that no template names | **10** — `box` `dog_rose` `dogwood` `guelder_rose` `hedge_hornbeam` `hedge_privet` `log_beech` `snag_spruce` `spindle` `stump_beech` |

The 22: `alpine_aster` `bilberry` `bramble` `broom` `brown_knapweed` `campion` `chamois_cress`
`cornflower` `dandelion` `fern` `gentian` `heather` `meadowsweet` `mountain_avens` `osier` `pennycress`
`poppy` `reed` `saxifrage` `wild_carrot` `wood_anemone` `yarrow`.

**THE REQUIREMENT IS THE COVERAGE, NOT THE LIST.** *Every species a declared template names has a row* is
the statement, and the twenty-two above are what it evaluates to today. **Adding a template changes the
number and no board item needs editing** — which is the whole reason this replaces `board:1164`'s class A
rather than restating it in twenty-two files.

**Each row carries the shape the existing ones already carry**, and that shape is the work: `form`,
`crown`, `height_m`, `spread_m`, `height_sigma`, `dbh_cm`, `lai`, bark colour — **each with its
`_origin`**, which is where the botany actually lives. `hazel.json`'s `form_origin` is four sentences of
derivation and its `lai_origin` names the band and why the value sits in it. **A row without its origins
is a magic number in a table**, and the deleted board items carried none of that: their entire content
was a binomial and a common name.

**The ten unreferenced rows are a finding and not an error.** They are deadwood and hedge forms —
`log_beech`, `stump_beech`, `snag_spruce`, `hedge_privet`, `hedge_hornbeam` — reached by something other
than a template's species list. **Before any of them is called dead, name what reads it**: the deadwood
forms have their own feature and the hedge forms theirs, so the likeliest answer is that a second selector
exists and this task's own instrument does not see it. **Check that before concluding**, because *this
row is unreferenced* and *this selector is invisible to my query* are the same observation.

**Which of the 22 are botanically real is looked up, never recalled.** The region's own field data decides
growth form, height band, LAI and stand density; a beech leaf is 6–10 cm, and a number off by a factor of
ten is only found by looking. **Every value names the source it came from in its `_origin`.**

**Done when** every species named by a declared template has a row, every row's numbers carry their
origin, the ten unreferenced rows are each attributed to the selector that reads them or removed with the
reason, and the count is derived by the tree rather than written down anywhere.
