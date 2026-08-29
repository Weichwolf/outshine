Type: feature
State: open
Area: render
Tags: measured

# The frame has no anti-aliasing

**Benchmark** — Unreal: TAA, and TSR since 5.0, on by default; MSAA remains for forward paths. RAGE: MSAA on the deferred pass, later TAA. **They agree**, so the matter is closed: a shipped frame is anti-aliased, and sub-pixel geometry is the case it exists for.

MEASURED, in the tree rather than assumed: every pipeline in `SceneRenderer.cpp` is built with
`SDL_GPU_SAMPLECOUNT_1` -- three call sites, no exception. `Renderer` carries `Jitter_` and
`PrevJitter_`, and `MvpCamRel` applies the jitter to the projection, so a TAA was intended; nothing
accumulates it, and the places draw two frames.

Found while chasing board:2029's roof slivers, which it turned out NOT to explain: the same frame at
2 560 x 1 440 shows them wider rather than gone. So this item stands on its own and is filed
separately rather than folded into a defect it does not cause.

## RE-MEASURED, and the stage is BUILT rather than intended -- it diverges

"Nothing accumulates it" was half right. `Stage::TemporalResolve` stands in the catalogue, reads
`SceneAerial`, `SceneVelocity` and `SceneDepth` and writes `SceneLinear`; `SceneVelocity` is already
in the default outputs; `RenderFrame` advances a Halton (2,3) jitter over a period of 8; and
`Compiled` computes `SettleFrames_ = 1 + kTemporalSettleFrames` with `kTemporalSettleFrames = 128`.
`SceneRenderer::SettleFrames()` returns it and had NO CALLER. So the resolve is written, wired and
costed, and the ONE thing missing was that `Live.cpp`'s default content list never names it:

    declaration.Content = {Stage::Subjects, Stage::Overlay};   (+ Sky, AerialPerspective, shadows)

**TURNING IT ON BLOWS THE FRAME OUT, and the picture is the evidence.** With the stage declared and
the places drawing the 129 frames the plan asks for, every one of the six comes back with the sky at
pure white and the ground at pure black, the horizon silhouette crisp between them -- `varies by
0.000 of 255` on the flatness instrument, which is saturation rather than emptiness. Reverted rather
than left half-on.

Two candidates, neither yet measured, and they are separable:

- **the history feedback diverges.** Over 129 frames a blend weight at or above 1 saturates exactly
  this way -- bright to white, dark to black -- and at the two frames the places drew before, it
  would not have shown
- **the exposure falls to neutral.** `Stage::AutoExposure` is also `Provenance::Content` and is
  ALSO absent from the default list, so `Resource::Meter` takes its `FallbackKind::Neutral`. Without
  the resolve the tonemap took its exposure from the declared uniform and the picture was right, so
  this alone does not explain it -- but the two stages are the only Content stages the engine never
  declares, which is not a coincidence worth ignoring

**The measurement that separates them**: draw ONE frame with the resolve declared. If it is already
saturated, the exposure is the cause; if it is correct and degrades over frames, the feedback is.

## What will be true

- [ ] a frame is anti-aliased, and which technique is a decision written down with its reason
- [ ] the jitter that already exists either drives a resolve or goes

## The measurements that would show I am wrong

1. **Edge pixels, counted.** Along a high-contrast silhouette -- a roof against the sky -- the share
   of pixels that are neither the roof's colour nor the sky's. At SAMPLECOUNT_1 that share is near
   zero by construction, which is exactly the defect; anti-aliased it must rise
2. **The cost, bounded rather than assumed.** Whatever is chosen is measured against the 16.7 ms
   budget on this hardware, p50/p95/p99 over a moving camera, never as a mean. The frame stands at
   11.9 to 13.5 ms today with 187 000 triangles and a standing camera, so the headroom is real but
   not large
