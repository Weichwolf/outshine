Type: bug
State: open
Area: render
Tags: perf, oracle
Supersedes: 1126, 1132

# Every surface texture is sampled through its mip chain, and the filtered normal keeps its energy

**Benchmark** — Unreal: mips are part of the cooked texture and streaming feeds them; a normal map is filtered down the same chain so roughness and normal stay consistent. RAGE: mipmapped textures in the mapped resource, never built at draw. **Both agree** — a mip chain is cooked, not derived per frame.

**RE-MEASURED, AND `max_lod` IS NO LONGER THE DEFECT.** board:1134 opened it to 1000 after
finding that a zero clamps lambda BEFORE the magnification test, which made `min_filter`
unreachable by construction. What stands in its place is one line:

    constexpr bool kChainIsReadable = false;    (src/render/stages/SubjectResidency.cpp:30)

so every subject texture is created with `num_levels = 1`. The sampler may reach a chain that
is not built.

**AND TURNING IT ON IS NOT A SWITCH, WHICH IS THE FINDING.** Setting it true and running the
whole glTF corpus:

    khronos/glTF/ABeautifulGame
    METRIC linear_channels_differing_between_renders   2460 channels   at most 0   FAIL

That is not a picture disagreeing with the oracle -- it is TWO RENDERS OF THE SAME FRAME
disagreeing with each other. The chain makes the renderer non-deterministic, which no amount of
"is the filtering better" can be weighed against: a frame that differs from itself cannot be
compared to anything.

The cause is NOT established and is not guessed at here. What is known: each level is halved
and uploaded in its own copy pass (`SubjectResidency.cpp:253-285`), and the gate's two cases
stayed green -- WaterBottle moved from p99 1 code to 2 against a bound of 6.435, so the chain
does reach the sampler. Whatever is undefined is undefined for some textures and not others.

So `kChainIsReadable = false` is an honest declaration rather than a forgotten switch, and the
work here is to find what differs between renders before the flag moves.

Two costs. **Aliasing**: point-sampling a 2048-square normal map on a subject a few hundred
pixels wide keeps the full perturbation where a filtered one flattens — measured against Cycles
at 1.129x in tilt over the steep decile, unchanged before and after the chain landed, which is
the proof that nothing samples it. **Bandwidth**: 720p60 on five GPU cores with an OSM world of
hundreds of building and plant types is texture-bound long before it is triangle-bound, and no
chain means no locality.

Enabling it is not neutral (the two items it absorbs): averaging unit normals shortens them,
Toksvig closes 3.2 % of the distance to the oracle because the dominant error is the DIFFUSE
term — a filtered normal is a different direction, so `N·L` moves at every roughness. The
candidates that reach it are shade-then-average, a footprint that is not the screen-space quad
derivative (it spikes 712x at UV island boundaries), and an anisotropic filter.

## What will be true

- [ ] Every surface texture carries a chain and the sampler may reach it — `max_lod` set from
      the level count, proven by a readback that differs from level 0.
- [ ] The normal map's treatment names which answer it takes and why.
- [ ] No corpus case moves in the picture bound, or a case that moves has its move attributed.
- [ ] A vacuous relative check (both sides exactly 0) is not reported as 1.00000 disagreement.
