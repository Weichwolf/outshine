Type: feature
State: open
Area: render
Tags: measured

# The frame has no anti-aliasing

**Benchmark** — Unreal: TAA, and TSR since 5.0, on by default; MSAA remains for forward paths. RAGE: MSAA on the deferred pass, later TAA. **They agree**, so the matter is closed: a shipped frame is anti-aliased, and sub-pixel geometry is the case it exists for.

MEASURED, in the tree rather than assumed: every pipeline in `Renderer.cpp` is built with
`SDL_GPU_SAMPLECOUNT_1` -- three call sites, no exception. `Renderer` carries `Jitter_` and
`PrevJitter_`, and `MvpCamRel` applies the jitter to the projection, so a TAA was intended; nothing
accumulates it, and the places draw two frames.

Found while chasing board:2029's roof slivers, which it turned out NOT to explain: the same frame at
2 560 x 1 440 shows them wider rather than gone. So this item stands on its own and is filed
separately rather than folded into a defect it does not cause.

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
