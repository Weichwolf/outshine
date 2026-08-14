Type: task
Parent: 1137
Area: render
Tags: oracle, instrument

**The oracle's index passes get a reader, and the correspondence is checked rather than assumed**

`materialIndex` and `objectIndex` are rendered, stored and placed beside every case — 14 745 644 B each,
per case, per recipe — and **nothing reads them**: `git grep -l materialIndex -- src/ test/` returns
`test/corpus/prep/manifest.py` alone, which is the file that declares them. That file's own rule is *a
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
