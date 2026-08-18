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
