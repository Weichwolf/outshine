Type: feature
State: open
Parent: 1995
Area: render
Tags: gpu-driven, benchmark

# a static subject carries ONE placement, and its node transforms are baked at cook time

**Benchmark** — Unreal: a `UStaticMesh` has ONE transform and its SECTIONS share it; the authored
node hierarchy is flattened by the cooker, and a section splits only on MATERIAL. RAGE: a
`grmModel`'s geometries share the entity's matrix for the same reason. **Both agree, and neither
carries a transform per section.** Taking that.

## What is measured

`apps/bench --all` over Khronos's own, subject stage, twelve steps each:

| scene | draws | surfaces | placements | colour images |
|---|---|---|---|---|
| DamagedHelmet | 1 | 1 | 1 | 1 |
| BrainStem | 59 | 59 | 59 | 59 |
| ABeautifulGame | 49 | **15** | 49 | **15** |
| VirtualCity | 167 | 167 | 167 | 167 |
| **Sponza** | **103** | **25** | **103** | **25** |

**Sponza draws 103 times for 25 materials.** The split is not the material -- it is the
PLACEMENT: 103 parts, each carrying its own node transform, so `SameState` refuses to merge two
parts that wear the same surface. ABeautifulGame is the same shape, 49 draws for 15 materials.

board:1989 established why the merge cannot simply drop `ModelSlot`: every part owns its index
range, so one instance over two ranges gives both the first part's row. But that is only a problem
because the rows DIFFER, and for a static subject they need not exist at all -- a node transform
that never changes belongs in the vertices, which is what a cooker is for.

## What will be true

- [ ] a subject whose document declares no animation is cooked with its node transforms applied to
      the positions, so it carries ONE placement and its parts split only on MATERIAL
- [ ] Sponza's subject stage draws 25 times where it drew 103, and ABeautifulGame 15 where it drew
      49 -- measured by `apps/bench`, which is where the numbers above came from
- [ ] a subject that DOES animate is untouched: BrainStem keeps its 59 placements, because a node
      that moves cannot be baked into a vertex. The same split board:1991 drew for the cluster DAG,
      and for the same reason
- [ ] the picture does not move: khronos/glTF 444/444, since baking a transform into a position is
      the same arithmetic the shader was doing per frame

**The measurement that would show this is wrong**: if Sponza's draw count does not fall, the split
was never the placement and this item's table was misread. If the picture moves, the bake is not
the same arithmetic and the precision boundary is the first place to look -- a node transform in
`double` applied to a `float` position is exactly the kind of cast CLAUDE.md bounds at the camera.
