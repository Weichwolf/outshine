Type: feature
State: open
Area: render
Tags: perf, instrument

**The subject unit shades once per pixel, from a visibility buffer**

The geometry pass writes WHICH triangle covers each pixel and nothing else; a later pass re-fetches that
triangle, re-interpolates its attributes and evaluates the material. Shading is then a function of the
PIXEL rather than of the fragment, and its cost stops depending on how many triangles were drawn.

## Why THIS shape and not a G-buffer, decided by the device's own ratio

**This GPU has about a third of a PS4's memory bandwidth and more compute** (the owner's figure, and it
matches an A18 Pro's LPDDR5X against a PS4's 176 GB/s). **So the design question is not whether a
deferred pass is affordable -- it is which currency to spend.**

[DERIVED] at 720p60, which is 921 600 px times 60 = **55.3 Mpx/s**:

| what the geometry pass writes | bytes/px | traffic each way |
|---|---|---|
| a G-buffer: albedo, material row, emission as RGBA16F | 24 | **1.33 GB/s** |
| a visibility buffer carrying id AND barycentrics, RGBA32F | 16 | 885 MB/s |
| **id alone as R32Uint, barycentrics RECOMPUTED in the resolve** | **4** | **221 MB/s** |

**The last row is Nanite's own choice and it is the one this device's ratio asks for**: the barycentrics
are recovered from the triangle's projected corners and the pixel centre, which is arithmetic, and
arithmetic is the currency there is more of here. **A factor of six against the G-buffer, paid in ALU.**

**And the depth buffer is already there**, so the visibility word does not have to carry depth.

## What must be true

- [ ] **The geometry pass writes an id and stops shading.** Two shading paths would be one path and one
  trap, so the forward arm goes in the same round the deferred one lands
- [ ] **The barycentrics are RECOMPUTED and never stored**, or the bandwidth argument above is spent
- [ ] **The resolve re-fetches through readonly storage buffers**, which `board:1412`'s probe has
  already measured this device to accept from a fragment stage, with a control that says the answer is
  about the fetch
- [ ] **One material per resolve pass**, masked by the id -- a fullscreen pass cannot bind every
  material's textures at once and SDL_GPU has no bindless. This is Nanite's material classification in
  its simplest form
- [ ] **The picture does not move.** 148 corpus cases and the grown suite are the net, and a change of
  this size that reproduced the picture to six decimals would be evidence it never took effect
- [ ] **The frame cost is published before and after** by `test/outshine/frame/`, which already prints a
  comparison against what earlier runs left outside the tree

## The caveat, first and in full

**Apple GPUs are tile-based DEFERRED and already shade opaque geometry once per pixel in hardware.** So
the overdraw half of what a visibility buffer buys is a half this device largely has, and the honest
expectation is that the gain here is SMALL until triangles approach pixel size -- which is the regime
`ClusterDag.h` exists to reach and not the one the corpus is in.

**What it buys that the hardware does not** is decoupling: shading cost stops scaling with geometric
density, which is the whole point of a cluster DAG that can put a million triangles on screen. *So this
is infrastructure for the regime the LOD chain is aimed at, and the measurement that matters is taken
in that regime rather than on a 12-triangle case.*

**A measurement showing no gain at 720p on today's corpus would NOT refute it** -- and saying so before
the number arrives is what stops that number from being read as one.

## The instrument that will decide it exists, and the baseline is taken

`test/outshine/frame/` already runs four arms over a moving camera, 240 timed frames each after 20 warm,
five repeats, keyed by a digest of the sources -- so an after-run finds this and prints the comparison
without anything being stored in the tree.

[MEASURED] before any of this, 1280x720 against a 16.6667 ms budget, digest
`290000c2d587818950568abc109e05028ab9c31a2501c9e75336226f1d8975a6`:

| arm | p50 | p95 | p99 |
|---|---|---|---|
| `geometry` | 1.67 | 1.84 | 2.10 |
| `fill` | 2.03 | 3.25 | 3.42 |
| **`fill-twice-lit`** | **3.85** | **4.50** | **4.85** |
| `texture` | 2.20 | 2.96 | 3.25 |

**`fill-twice-lit` is the arm this feature is aimed at and it was already here**: two layers of
overdraw WITH lighting, which is precisely the cost a visibility buffer removes -- shade the pixel, not
the fragment. It costs 1.8 ms more than `fill` and 2.2 ms more than `geometry`.

**So the prediction is falsifiable and it is written before the work**: if deferred evaluation does what
it is for, `fill-twice-lit` moves towards `fill` plus one shading pass and the other three arms move by
the cost of one more attachment. **If `fill-twice-lit` does not move, the hardware was already doing it**
-- which is the TBDR caveat above, measured rather than argued.
