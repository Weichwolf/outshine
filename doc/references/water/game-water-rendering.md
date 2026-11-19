# Established game-water presentation

Primary source checked 2026-11-19. Product work: [#1966](https://github.com/Weichwolf/outshine/issues/1966).

- Mark Finch, GPU Gems, chapter 1 (2004): [Effective Water Simulation from Physical Models](https://developer.nvidia.com/gpugems/gpugems/part-i-natural-effects/chapter-1-effective-water-simulation-physical-models).
  - Method: summed analytic waves / Gerstner waves; analytic surface derivatives; geometric low frequencies plus texture-normal high frequencies.
  - Use: inexpensive scale-dependent water motion and normal maps on an independently generated water surface.
  - Limit: not a river-level/shoreline solver or full hydrodynamics; historical wave counts and GPU timings are not Outshine budgets.
- Yuksel et al., SIGGRAPH 2007: [Wave Particles](siggraph/2007-wave-particles.pdf).
  - Method: particle-based wave transport and interaction.
  - Use: later local interaction where analytic waves are insufficient; not required for the first useful water shader.

Surface topology, monotone river courses and water/terrain/transport contacts are the generation problem. See [Outshine's own height/profile methods](../procedural/outshine-generation.md).

Python references establish the chosen wave equations/derivatives and adequate cost; Python may dispatch the shared GLSL kernel. Native integration adds resource/lifetime engineering and target-device measurement. Do not develop a new presentation theory to make a plausible river visible.
