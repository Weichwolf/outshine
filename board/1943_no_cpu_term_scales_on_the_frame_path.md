Type: feature
State: open
Progress: gpu-driven
Area: render
Tags: benchmark, target

# Nothing on the frame path scales with geometry, lights or pixels on the CPU

**Benchmark** — Unreal: the same answer as 1926 for the whole frame path, not only lights. RAGE: per-batch. **Taking Unreal**.

This is the area where the distance to both benchmarks is largest. Unreal drives its passes from
`FGPUScene`: instances live in GPU buffers, culling runs in compute, and the draw is indirect --
the CPU issues one call for a pass, not one per batch. Lights are assigned to clusters in a
compute pass and the shading stage reads the grid. RAGE is reconstructed and less certain, but
its drawlists are likewise fed from resident buffers.

outshine issues a uniform push per batch, twice per frame.

- [x] the tone chain is checkable end to end against a closed form, so a factor of two cannot
      hide in it
      proof: outshine/door/ScoreWhatALitSurfaceReads
**THE GEOMETRY HALF OF THIS INVARIANT IS BUILT IN board:1989**, which carries the measurement,
the five mechanisms of a GPU-driven path, the seven-step route and its oracles. This item keeps
the RULE; that one keeps the work. What stays here is everything the rule covers that is not
geometry: lights, shadows, the queries the frame makes, and the distribution it publishes.

- [ ] the shadow pass issues ONE indirect draw from the resident instance buffer, not one
      `PushGPUVertexUniformData` per batch (board:1926)
- [ ] lights are assigned to clusters or tiles by a compute stage, and `kMaxSubjectLights` MOVES
      rather than stops. **Both benchmarks cap, and both cap somewhere else than this tree does.**
      Unreal (UE5, deferred + clustered): a compute pass fills a froxel grid and a pixel reads only
      the lights of ITS cell -- the scene holds thousands, the per-CELL list is short, and that is
      where the constant lives. Forward+ adds a scene-wide buffer cap on top. RAGE: lights are
      volumes in the deferred pass with no per-material cap at all, sorted by importance and
      truncated per frame, with SHADOW casters a separate and much smaller budgeted set.
      **They agree on the structure**: a scene-wide buffer with no small cap, a culling step that
      bins into cells, a short per-cell list the shader reads, and shadow casters budgeted apart.
      So `kMaxSubjectLights = 16` is a cap in the one place neither engine caps -- what the whole
      SUBJECT PASS can see. The number is not wrong; its PLACE is. As a per-cell constant 16 is
      plausible and in Unreal's own order of magnitude.
      **TAKING RAGE: thousands in the scene, few with shadows.** Three reasons, and the third is
      the one that decides.
      *One*, the target. outshine holds 720p60 on five GPU cores over a driven city -- the exact
      shape RAGE was built for on console hardware of its day. Unreal's froxel grid assumes a GPU
      with headroom to spend on binning.
      *Two*, the asymmetry is where the cost is. A light WITHOUT a shadow is a few ALU in the pass;
      a light WITH one is a render target, a pass and a matrix. RAGE budgets the expensive half by
      name. Unreal has the same split underneath its grid -- per-light shadow budgets, capped CSM
      resolution -- it simply does not lead with it.
      *Three*, and this is where the first pass at this decision was WRONG and is corrected here.
      RAGE's answer is not "raise the constant" -- it is "a local light is a VOLUME in a deferred
      pass", and only the pixels a light covers pay. That is right for RAGE and it is the more
      expensive answer HERE. outshine's subject pass is FORWARD. Deferred means a G-buffer --
      normal, albedo, roughness/metalness, depth -- four targets over 921 600 pixels EVERY FRAME,
      whether the scene holds ten lights or a thousand. On five GPU cores with shared memory
      bandwidth is the constraint, so that trades a permanent cost for a problem that only appears
      when many lights stand close together.
      **Unreal's clustered grid needs no G-buffer.** Forward+ is exactly this: a compute pass, a
      grid buffer, and a shader that reads the short list of its own cell. The transmission and
      composite path that already works here is untouched.

      **So the decision takes from BOTH, at two different questions, which is what CLAUDE.md asks
      for where they differ:**

      | question | taken from | why |
      |---|---|---|
      | how many lights may exist, and how does a pixel find them | **Unreal**, clustered grid | outshine is forward; a grid forces no G-buffer, volumes do |
      | what is expensive, and what is budgeted | **RAGE**, a small separately declared shadow set | the asymmetry is real and belongs named |

      What this predicate therefore owes: the scene buffer loses its ceiling, a compute stage bins
      lights into cells, `kMaxSubjectLights` becomes the PER-CELL constant (16 is plausible and in
      Unreal's own order of magnitude), and the SHADOW-caster set is budgeted separately and small.

      **The measurement that would show this wrong**: if a G-buffer's bandwidth at 720p on this
      device is cheaper than the compute pass plus the per-pixel cell walk. That is measurable and
      the number decides, not the derivation above.
      **The exact Unreal defaults (grid pixel size, Z slices, culled lights per cell) are to be
      read before they are quoted** -- CLAUDE.md's rule is that a declared number is right before
      it is written, and this item will carry them.
      being the light budget (board:1926)
- [ ] the shadow atlas carries cascades and the shader picks one (board:1926)
- [ ] the shadow centre folds INSTANCE bounds, not every vertex (board:1926)
- [ ] no query the frame path makes blocks, allocates, locks or touches disk (board:1937).
      **NOTHING STALLS ANY MORE, AND THE ALLOCATION IS 25 TO 28 TIMES SMALLER.**
      The instrument came first: `Heap::Tagged` had counted bytes per named tag since it was
      written and nothing outside the library could read the count, so this predicate was a
      sentence. The engine publishes `heap taken under <tag>` cumulatively and `apps/bench --heap`
      differences it across frames -- the engine aggregates, the consumer counts.

      **The stalls went with board:2007.** `harness/claims/NoFramePathCallReachesABlock` walks the
      PICTURE now as well as the simulation, and reaching a `Readback` from it is a refusal.

      **The allocation, measured per frame, before and after:**

          scene            before        after      wall over 24 steps
          DamagedHelmet    43 640 833     1 720      493.7 ms ->  38.4
          Sponza           45 214 081     1 820      521.9    ->  72.8
          ABeautifulGame   42 593 809     2 877     2028.7    -> 1860.4
          VirtualCity      50 700 646   231 820      543.3    ->  92.7
          Fox                 997 496   265 929        --     ->  10.5
          BrainStem        66 279 113 6 764 249      685.4    -> 221.3

      Two causes and neither was the renderer. The measures read a render target back on every
      frame (board:2007). And `Gltf::Subject::Flatten` built a fresh `outshine::Geometry` plus a
      `std::vector` per part per attribute per frame -- for a POSED subject that is every frame.
      Unreal poses a skeletal mesh into pre-allocated storage (`FSkeletalMeshObjectCPUSkin`) or
      not at all (GPU skinning off a matrix palette); RAGE the same. **Neither rebuilds the mesh
      from its source asset per frame**, so the scratch persists here: `Geometry::Restarts()`
      keeps the parts and clears them, and every intermediate buffer in `Flatten` is a member.
      Scratch is not a value, so a copied `Subject` copies none of it.

      **What is left and why it is not zero**: BrainStem still takes 6.76 MB a frame. It is the
      one scene here whose vertices are SKINNED, and the remaining cost is the per-part `Part`
      and the vertex-splitting map. The bound CLAUDE.md states is BOUNDED rather than zero, and
      6.76 MB that does not grow with the frame count is bounded -- but it is not defensible
      against a matrix palette, which is what the two benchmarks actually do. That is the next
      step and it is named here rather than ticked.
      proof: outshine/door/ScoreWhatAFramePathTakes, outshine/door/ScoreWhatManyCastersCost,
      harness/claims/NoFramePathCallReachesABlock
- [ ] the frame publishes p50/p95/p99 over a moving camera, not a mean (board:1457)
- [ ] geometry carries its own detail ladder and the pool holding it is a slot table
      (board:1512)


**FOLDED IN 1926**, the light-and-shadow half of this. Its own words: "'light and shadow are GPU-side' is the intent; it is not measurable as written". This item states the measurable version over the whole frame path, and 1926's benchmark already pointed here.
