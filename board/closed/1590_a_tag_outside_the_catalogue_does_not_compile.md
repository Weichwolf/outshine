Type: issue
Area: core
Tags: scene

**A tag outside the catalogue does not compile**

The reference design's sentence is "a misspelled tag is a compile error" (board:1583, the GAS
row), and CLAUDE.md's is "the consumer selects from a constexpr catalogue and cannot add to it".
`Tag` (`src/scene/Register.h:15-23`) is an open aggregate with a public `Value`: any caller mints
`Tag{0xDEADBEEF}` and the store accepts it.

The tree already exercises the hole:
`test/unit/scene/AnAdvertisedInteractionIsClaimedBeforeItIsUsed.cpp:15-16` mints its own family
-- `kOffersRefuel{0x02010000}`, `kOffers{0x02000000}` -- that `Register.h` never declares. Two
vocabularies exist on day one, which is how GAS-without-the-central-tag-registry rots: two
modules mint the same prefix independently and set algebra silently answers across them.

What must be true:

- construction is owned by the catalogue: a `consteval` factory or private constructor with the
  `tags` namespace as the only author, so a tag value outside `Register.h` is unspellable
- the catalogue carries every family in use -- `Does*` and the offer/activity family the slot
  machinery already tests with
- uniqueness and prefix shape are `static_assert`ed over the table, the way
  `scene_register_checked` already proves the relation rules

The render plan's catalogue is the model in this same tree: unspellable beats refused-at-load,
and refused-at-load beats accepted-silently -- the store today sits at the third rung.

---

**Closed.** Tag's value constructor is private; only `TagCatalogue` mints, and `namespace tags`
re-publishes the catalogue's values under the old spelling. The second vocabulary the review
caught (the test-minted 0x02 family) is now the catalogue's own Offers branch. Proving test:
`unit/scene/AMisusedTagHasNoSpelling` judges `unit/compile/scene/ATagIsMintedOnlyByTheCatalogue`
-- a forged tag is refused by the compiler for the declared reason, with the layer's own
include set.
