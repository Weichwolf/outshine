Type: task
Parent: 0078
Area: corpus
Tags: khronos, oracle

**`TextureTransformMultiTest`, and the three rows it waits on**

The case is wanted and it is blocked. **Re-homed from `board:1177`**, whose fifth clause asked for it —
and asked it to prove something the asset does not contain (`board:1179`). **The requirement survives; the
claim about what it proves does not.**

**What it actually separates**: *transformed on every socket* from *transformed on base colour only*, over
**29 materials**. So it exercises the transform on sockets the engine must first be able to read at all.

**The dependency chain, and it is arithmetic rather than an estimate** — of its 27 cells:

| row | cells it unblocks |
|---|---|
| **`TEXCOORD_1`** — `board:0079` row 3 | **9** |
| **`KHR_materials_clearcoat`** | **18** |
| **occlusion texture** | **3** |

`Depends:` is written as the edge rather than left as prose, because *cannot start until that is closed*
is exactly true here: a case scored against sockets the engine does not read reports the missing socket
and not the transform.

**Occlusion is the one that will not arrive on the normal ladder.** `board:0079` gated it — the
specification's *"Direct lighting is not affected"* against a subject path that reads no irradiance and a
corpus where **`bounces.max` is 0 in all 61 render declarations**. So this case is **partly blocked behind
a row that is itself blocked behind `board:1150`**, and that is stated here so the chain is visible from
one place rather than reconstructed.

- [ ] **The case may land before every cell is readable, and if it does the unread cells are a named
  refusal** — not a fallback, not a silent zero. A case that scores 27 cells while reading 9 is reporting
  a number about a different picture
- [ ] **Its verdict shape is decided when it lands, not now.** 29 materials over one frame is a
  comparison grid, and `board:0079`'s note on the `Compare*` family applies: a grid's verdict is not
  automatically the picture bound's
- [ ] **It does not carry the per-reference claim.** That is `board:1179`, and conflating them again is
  the mistake this task exists to record

**Done when** the three rows above are closed or their cells are refused by name, the case is in the tree
with a declared verdict shape, and `board:0078`'s matrix carries it at the rung its dependencies imply.
