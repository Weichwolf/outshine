Type: task
Parent: 1137
Area: render
Tags: oracle, instrument

**The oracle's index passes get a reader, and the correspondence is checked rather than assumed**

`materialIndex` and `objectIndex` are rendered, stored and placed beside every case — 14 745 644 B each,
per case, per recipe — and **nothing reads them**: `git grep -l materialIndex -- src/ test/` returns
`test/harness/shared/corpus/prep/manifest.py` alone, which is the file that declares them. That file's own rule is *a
channel arrives when a test reads it*, and two channels are held against it.

Their stated purpose is exactly this feature's first question: *the picture bound asks `is this pixel
covered` when the question is `WHAT covers it`, and a surface swap read as 209 codes for want of this*.
The 209 is [MEASURED] and still on disk — `coverage/negative-scale` (717, 274), ours 209.3498 in blue.

**What this task delivers**

- The two passes read through `RawF32`, like every other quantity, with an absent file a refusal and
  never a skip.
- **The correspondence between Blender's `pass_index` and our own material and node identity, derived and
  checked.** The preparer assigns the index; the runner must map it back to the glTF material a draw
  wears. It is not enough that the numbers exist on both sides — the mapping is the claim.
- **A vacuity check before any verdict rests on it.** `board:1130` spent a round on `objectIndex` over an
  asset carrying **one** object index and **one** material index, where *the neighbour is the same
  surface* is true by construction and 6 038 of 6 069 said nothing. So: publish the number of **distinct**
  indices the pass carries over the covered region, and refuse to adjudicate where it is one.
- A per-pixel predicate — *the two sides agree about which surface is here* — beside the coverage
  predicate that exists, and its count published.

**What it must not do.** It must not become a metric with a bound. The count is `Direction::Reported`;
what it feeds is `board:1144`'s routing decision, and that is a routing change with its own item.

**Done when** a case can answer *which material does each side put at (x, y)*, the answer is refused where
the pass cannot discriminate, and the two channels are read by a test rather than merely produced.

## Comments

**CLOSES on the material half. The node half moves to `board:1153`, and the reason it is not a strike is
that the requirement never depended on this file to survive.**

**This item stated its requirement in two sizes, and the smaller one is the `Done when`.** The body asks
for the correspondence to *our own material **and node** identity*; the acceptance asks only *which
**material** does each side put at (x, y)*. The delivered work meets the acceptance completely and half
the body: `DrawList::Compile` merges primitives of different nodes into one batch when they share a
material, so the finest identity reachable through the per-slot uniform **is** the surface, and
`objectIndex` is read for discrimination only. **The inconsistency was mine, written into this item three
rounds ago — and it is the same defect I repaired in `board:1128` one round later, in someone else's
file.**

**Why that does not make this a `board:1128` repeat.** There, the smaller statement had **no successor and
no other home**: a round could meet it, call the feature finished, and nothing on the board would still be
asking. Here the node half is a box in the **parent feature** — `board:1137` says *which material and
which object each side says is at the pixel* — and the parent cannot be closed with an open child or a
ticked box it has not got. So closing this task ticks nothing it did not deliver.

**And why the `board:1127` precedent does not apply either, since two answers to one shape would be
worse than either.** There a clause was **struck** because it asked for something *about a different
subject* — a picture population for a shader-algebra defect, in the wrong suite, and wider than the item.
Here the unmet half is squarely this item's own second channel. It is not struck; it is **carried
forward intact** to a task that needs a different mechanism — a per-vertex part index, a sixth vertex
stream, a change to what a batch may merge. **One rule generates all three calls: close when the item's
own statement is true of the tree and the gap has a home that outlives the closure.** `1127` had `1147`;
this has `1137`'s box and `1153`; `1128` had neither, which is why its clause was repaired instead.

**What the round proved beyond its acceptance, and it is the more valuable half.** The naive count on
`negative-scale` is **190** disagreeing pixels and **only 1 is about us**: 189 carry `ChecksAndXMaterial`
in the index pass while the oracle's own picture carries `BackgroundMaterial`'s declared colour
**bit-for-bit** — 47 343 agreements, 189 differences, **nothing in between at any tolerance to 1e-6**, so
the predicate needs no threshold. Had 190 shipped, `board:1144` would have routed 189 pixels on a false
premise. The mechanism is `board:1155`. **A count published before its verdict caught it**, which is the
discipline this suite keeps asking for and is here shown paying.

**And the vacuity trap is most of the corpus, not an anecdote**: **18 of 35 cases** carry one material
index and one object index over their covered region, and every one refuses. `board:1130` spent a round
on that shape with a sample of one.
