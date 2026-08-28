Type: feature
State: active
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

**AND THE PREMISE ABOVE IS WRONG, measured before a line of it was written.** Sponza's 103
placement rows are all the SAME row: `apps/bench` reads `103 placement(s) ... 1 differ`. The node
transforms are already baked -- `Live::Stand` writes the identity matrix for every part -- so
nothing needs a cooker and nothing needs baking. What splits the batches is that `SameState`
compares the placement's INDEX and not its ROW: 103 slots holding one value, and a merge refused
between two parts that wear the same surface and stand in the same place.

So the item is smaller and safer than it was filed: give equal rows the same SLOT, and the
existing merge condition does the rest. It already requires adjacency, and the shader already
reads `rows[first_instance]`, so two merged parts with an equal row draw exactly what they drew.

board:1989 established why the merge cannot simply drop `ModelSlot`: every part owns its index
range, so one instance over two ranges gives both the first part's row. But that is only a problem
because the rows DIFFER, and for a static subject they need not exist at all -- a node transform
that never changes belongs in the vertices, which is what a cooker is for.

## What will be true

- [x] **the sort puts MATERIAL before DEPTH where nothing blends**, which was the larger half and
      is the one that landed. `DrawKey::Of` packed depth above material, so an opaque material's
      parts scattered across the depth range and no two were ever adjacent. Unreal sorts opaque
      mesh draws by STATE and gets front-to-back from a depth PREPASS, not from the sort; RAGE the
      same. Depth-first is for TRANSLUCENT, where it is correctness -- so blended surfaces keep it
      and nothing else does.
      Measured: Sponza's subject stage goes from 4.19 to 8.09 million tri/ms and ABeautifulGame
      from 26.9 to 77.8, with the draw COUNT unchanged. The win is state: consecutive draws now
      share a material, so the eight texture bindings and the uniform push happen 25 times over
      103 draws instead of 103 times.
      proof: khronos/glTF 444/444 -- the picture does not move, which is the control that matters
      when draw ORDER changes; outshine/door 33/33; gate GREEN.
- [ ] equal placement rows share a SLOT, so `SameState` splits on where a part stands and not on
      which index happens to hold it. **ATTEMPTED AND ROLLED BACK.** Deduping the slot in
      `SubjectProxy::Places` took Sponza to 25 draws and ABeautifulGame to 21 -- exactly their
      material counts -- and khronos stayed 444/444. But the door suite broke in a way I could not
      explain: `ScoreWhatManyCastersCost` overflowed the staging ring by 1312 bytes and
      `ScoreWhatAMovingSceneResends` timed out. Sizing the ring for the placement rows closed 128
      of those bytes and not the rest, so the interaction between the canonical slot and the
      staging budget is not understood, and a change I cannot explain does not stay in.
      **The refusal was made to say what it carries, and the numbers do not add up.** It read
      `stage 864 bytes over the 848` and named nothing; it now names the hand and its crossings,
      and the second attempt read:

          45056 over 43616 -- 0 already staged and this hand adds 36864
          (4608 + 4608 + 4608 + 6144 + 3072 + 3072 + 6144 + 4608)

      `wanted = StagingUsed_ + total`, so 45056 must equal 0 + 36864 and does not. The missing
      **8192 bytes are exactly 64 rows of 32 floats** -- the placement crossing. So `total` counts
      a crossing the listing does not, and both walk `what[0..count)`. That is the thread the next
      attempt pulls, and it is a defect in the residency's own accounting rather than in the
      dedupe: a refusal whose two numbers disagree cannot be trusted to say when to grow the ring.
      What the next attempt needs first: WHY a smaller set of named slots makes a frame stage MORE
      -- and the answer starts with that 8192.
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
