Type: feature
Area: generators
Tags: perf, instrument

**Four cases moved across the port, and the attribution is recorded WITH its cost so a later round overturns it on evidence**

*Recorded 2026-08-13 at the coordinator's decision, which I endorse and am writing down as a decision
rather than leaving to be assumed. Four cases moved across `0161f88` — `corset` 22 773 → 22 772 image
pixels, `lantern` 52 066 → 52 069 linear channels, `boom-box` and `water-bottle` at ≤ 2e-4 relative.*

- [ ] **What did not move is what makes this small.** Everything geometric is untouched on all four — coverage, silhouette, IoU, boundary percentiles, `exactness_*`, `plan_passes` — and **no criterion changed side**. All four movements are in metrics declared `reported`, which by § I.26.12 carry no threshold and decide nothing
- [ ] **The BRDF is ruled out by direct measurement and not by argument.** The tie test evaluates the emitted shader **on the device** against its C++ twin and came out bit-identical under both compilers. The movement is reproducible run to run, so it is not a seed
- [ ] **The attribution, stated as an attribution and not as a conclusion**: the non-BRDF tangent chain and the filtered f32 taps, where Metal may contract multiply-adds differently between Tint-generated and hand-written MSL. **The supporting pattern is that the four are exactly the four photographic metal-rough assets, while `scifi-helmet` and `normal-tangent` take the same mapped and lit arms and did not move** — which fits a difference in a shared filtered path and fits no difference in the BRDF
- [ ] **The decision not to prove it, with the price that decided it.** Proving it needs the same expression tree compiled both ways, which means **re-vendoring the 1.4 GB Dawn tree this round deleted**, to attribute a **one-pixel-in-921 600** movement in a metric with no threshold. **Disproportionate, and recorded as such.** *This is not "we could not measure it" — it is "we priced the measurement and declined", which is a different sentence and the only one that lets a later round overturn it*
- [ ] **What would overturn it, so the reopening is cheap.** Any of: a *geometric* metric moving on a fifth case · a movement appearing on an asset that takes neither the tangent chain nor a filtered tap · the residual growing when a `reported` metric becomes an acceptance · a case where the four-asset pattern breaks. **Any one of those, and the 1.4 GB is worth it**; none of them, and it stays a recorded attribution
