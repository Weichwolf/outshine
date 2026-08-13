Type: bug
Area: corpus
Tags: oracle, instrument

**The oracle key does not cover the preparer's own code**

The key covers Blender, the subject pins, the whole declared scene and the recipe. It does **not** cover
`test/corpus/prep/in_blender_render.py`, which decides which passes are enabled, how materials are
built, and where lights and cameras sit — all of which change what Cycles renders while the declaration
is untouched.

Measured at `e175895`: enabling three render passes changed 8 of 58 beauty products under keys that
still matched. `coverage/quad` 116 013 differing samples of 3 686 400 at max **2 ulps**;
`cameras-perspective` 23 850 at 2 ulps; `point-light-intensity` 453 862 at **8 ulps**, where 256 spp
accumulate. Cycles' arithmetic ordering moved in the last one to three bits. No verdict moved, which is
luck rather than design.

The first explanation offered for this was *stale oracles*, and it was **retracted**: the comparison used
`git stash` at a moment when only the channel trim was uncommitted, so the "old preparer" was the new one
minus an inert change. Redone against a worktree at `7df7e47`, every case differs.

**Done when** the preparer's digest is in the key — one line in `jobs.py`'s `derived_key` payload — so a
preparer change invalidates the corpus instead of being found by hashing on a hunch. There is no second
cache, so the repair is in the key and never in a parallel store.
