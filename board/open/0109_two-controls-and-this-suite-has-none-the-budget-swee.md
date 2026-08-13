Type: feature
Area: render
Tags: perf, instrument

**Two controls, and this suite has none: the budget sweep and the dolly zoom**

*Almost every case in the render suite asks **"does A match B"**. A control asks **"does anything change
when nothing should"** — which is how an undeclared variable is caught, and `directional-light` is the
cautionary case: three criteria simultaneously blind because each was invariant under the transformation
that was wrong. Owner's contribution, 2026-08-13; recorded here because it would otherwise live in a
transcript.*

| | vary | hold | what fails |
|---|---|---|---|
| **dolly zoom** | distance *and* focal length | projected size | selection driven by **distance** rather than by error |
| **budget sweep** | the budget | the camera | a coarser level that is **not a subset** of the finer |

- [ ] **The dolly zoom is an invariance control and not a comparison.** Screen-space error is `s·f/d`; raising `d` and `f` together leaves it unmoved, so **if selection is by projected error the render is invariant all the way back** — same crop, same rung, same pixels. If detail drops as the camera pulls away, the selector is distance-driven, which is the naive implementation and the most common way an engine gets this wrong
- [ ] **Three of the four quantities are invariant and the fourth is not — asserting all four would manufacture a permanent red.** Invariant: the **LOD rung** (projected error by definition), the **texture mip** (chosen from uv derivatives per pixel, and the derivatives are unchanged when the crop is), the **impostor takeover** (an error threshold, plus a view direction the dolly does not rotate). **Not invariant: shadow-map texel density.** A cascade split by view depth gives a texel a world size proportional to `d`, while a screen pixel's world size `d/f` is constant under the dolly — so texel-per-pixel degrades as `f`, in *any* conventional cascade scheme including a logarithmic split, which is derived for a fixed focal length. **Reported, never enforced**, until § I.10's shadow stage declares a split rule derived from projected texel error; if it ever does, this line becomes enforcement and the cost is that the cascade extents become a function of the focal length
- [ ] **Aerial perspective, fog and any distance-dependent term are declared off in the case, or the control measures them instead.** They are genuine functions of `d` and would break a pixel comparison for a correct reason — the confounded finding this repository keeps paying a round for
- [ ] **The control needs no chosen tolerance, because its primary metric is an INTEGER: the selected rung, identical at every station.** *A control whose enforced quantity is exact is better than one with a derived tolerance, and this one can be.* The pixel comparison stands beside it under the suite's existing raster floor, and the honest bound on how far the dolly may run is the **depth buffer**, not the selection: at f32 the relative error of `s·f/d` is a few `γₙ ≈ 5e-7` and never approaches a rung boundary, while depth precision at the declared near plane fails visibly first. **The case declares its maximum distance from that bound and states the derivation**
- [ ] **The two controls do not substitute for each other and both are owed.** The dolly zoom says **the selector is right**; the budget sweep says **the levels are right**. An engine can pass either while failing the other badly
