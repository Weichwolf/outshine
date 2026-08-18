Type: bug
Area: render
Tags: khronos

**A normal-mapped cell reproduces its geometry half as faithfully as the oracle's**

`NormalTangentTest` is a grid of paired cells -- real geometry on the left of each pair, the same shape
as a normal map on the right -- and the invariant is a comparison **inside one render**: the two cells
of a pair should look alike. Both sides are measured against it.

[MEASURED] p95 relative, geometry against normal map, per pair:

| pair | ours | the oracle's |
|---|---|---|
| row1 pair1 | 0.091199719 | 0.078861113 |
| **row1 pair2** | **0.18078045** | **0.086919908** |
| row1 pair3 | 0 | 0 |
| row2 pair1 | 0.097875199 | 0.06785536 |

**Pair 2 of every row fails and pairs 1 and 3 pass.** Pair 3 is zero on both sides because that column
is black by the asset's own design -- the reference is black there too, which was checked by looking.

**AND THE ORDERING IS THE OTHER WAY ROUND HERE**, which is why this is a defect and not a reduction: on
`PointLightIntensityTest` the oracle missed every invariant by more than we did and the case declared
that; here **we are twice as far out as the reference**, so there is nothing to reduce.

## Two readings, and the second is already measured elsewhere

- [ ] **Our tangent frame or normal-map decode is wrong for that column.** Pair 2 is the second column
  of test cells and it fails in all four rows, which is a column-shaped fault
- [ ] **OR it is `board:1363`'s diffuse coupling seen twice.** The two cells of a pair carry different
  per-pixel normals, and our diffuse attenuates by `1 - F(v.h)` where Cycles' attenuates by
  `1 - E(n.v)`; two normal distributions then diverge by different amounts on the two sides, and the
  ratio between the cells is what this invariant reads. **`NormalTangentMirrorTest` is green**, which
  constrains the first reading and not the second

## What is NOT wrong with it

**The picture agrees.** `board:1361` looked at this case side by side and found *the same layout, the
same spheres, the same highlights*, with the only visible difference the thickness of the small arrow
glyphs in each tile's corner. This item is about a comparison the case makes with ITSELF.

## The cause is inside the case, in its third column

**This subject is ONE material** painted by a base-colour texture -- the three columns are regions of that
texture, not three materials, and not three tangent setups. That is what makes the third column an
experiment rather than a curiosity.

| column | its base colour | ours | oracle |
|---|---|---|---|
| 1 | pink | 0.091199719 | 0.078861113 |
| 2 | blue-grey | **0.18078045** | 0.086919908 |
| 3 | **black** | **0** | **0** |

**Where the base colour is zero, the disagreement is zero -- exactly, on both sides.** No diffuse term,
no divergence. That is `board:1363`'s discriminator reproduced inside a single render: it measured on a
generated sphere that removing the diffuse term drops the residual by 7.1x, and named the cause as Cycles
attenuating its diffuse by `1 - E(n.v)` where glTF Appendix B specifies `1 - F(v.h)`.

**Why an invariant BETWEEN two cells is sensitive to it at all**, which is the part that needed
explaining: the two cells of a pair carry different per-pixel normals -- one geometric, one from the
map -- so the two attenuations are evaluated over two different normal distributions and diverge by
different amounts. The invariant reads that difference and calls it a tangent fault.

**The first reading -- a column-shaped tangent or decode fault -- is refuted by the same column.** A wrong
tangent basis does not become right when the albedo goes to zero.

**No Blender node graph can reach the microfacet half-vector**, so the reduction is the ladder's second
rung and not the fourth, and it is declared per `(case, metric)` on the four `pair2` metrics only.
