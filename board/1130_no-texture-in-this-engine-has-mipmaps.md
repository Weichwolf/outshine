Type: bug
State: open
Area: render
Tags: perf, oracle
Supersedes: 1126, 1132

# Every surface texture is sampled through its mip chain, and the filtered normal keeps its energy

The chain is built and uploaded and never sampled: the sampler's `max_lod` is left
zero-initialised, so every fetch clamps to level 0 while `mipmap_mode = LINEAR` selects between
levels it may not reach. Four textures are created outright at one level
(`src/render/Renderer.cpp:239,288,320,880`).

Two costs. **Aliasing**: point-sampling a 2048-square normal map on a subject a few hundred
pixels wide keeps the full perturbation where a filtered one flattens — measured against Cycles
at 1.129x in tilt over the steep decile, unchanged before and after the chain landed, which is
the proof that nothing samples it. **Bandwidth**: 720p60 on five GPU cores with an OSM world of
hundreds of building and plant types is texture-bound long before it is triangle-bound, and no
chain means no locality.

Enabling it is not neutral (board:1126, 1132 absorbed): averaging unit normals shortens them,
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
