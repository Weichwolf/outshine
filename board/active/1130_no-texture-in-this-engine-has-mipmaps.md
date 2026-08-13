Type: bug
Area: render
Tags: perf, instrument

**No texture in this engine has mipmaps**

`src/render/stages/SubjectDraw.cpp:882` creates every surface texture with **`num_levels = 1`**, and
`:915` sets **`mipmap_mode = NEAREST`**. There is no minification filtering anywhere: a 2048×2048 map on
a subject spanning a few hundred pixels is bilinear-sampled from level 0.

**Found while investigating `board:1126`**, where it is the leading candidate for a shading-normal
disagreement — but it is a defect in its own right and larger than that item, which is why it is filed
separately rather than repaired inside it.

**Two costs, and the second is the one that matters at the target.**

**Aliasing.** Point-sampling a minified map is the textbook definition of it. On a *normal* map it does
not merely shimmer — it changes the shading, which is what `1126` measures: our perturbation stays at
full strength where a filtered one would flatten, by up to **2.33×** in tilt.

**Bandwidth.** This engine's stated budget is **720p60 on five GPU cores**, and sampling full-resolution
textures at high minification is the worst case for a texture cache: every neighbouring screen pixel
reads a distant texel. **An OSM-scale world with hundreds of building types and hundreds of plants will
be texture-bound long before it is triangle-bound**, and no mip chain means no locality.

**The repair is not simply *enable them*.** Averaging unit vectors shortens them, so a mipped normal map
loses perturbation strength with level — the literature normalises after, or carries the lost length as a
roughness term (Toksvig, LEAN). **A round that enables mipmaps without deciding that is trading one
wrong picture for another.**

**Done when** every surface texture carries a mip chain, the normal map's treatment names which of those
answers it takes and why, and **no case moves in the picture bound** — or a case that moves does so with
its move attributed and defended.

**REOPENED: the chain is built, uploaded, and never sampled.** `min_lod` and `max_lod` are never set on
the sampler, so the zero-initialised `max_lod = 0` clamps every fetch to level 0. `mipmap_mode = LINEAR`
selects between levels it is not allowed to reach. **The same *declared but not exercised* shape as the
eighteen catalogue rows** — the capability is present on the device and nothing uses it.

**Proven by the picture not moving at all.** The worst-appearance tails are **byte-identical** to before
the chain landed: `normal-tangent` 229.330177, `normal-tangent-mirror` 184.356962, `water-bottle`
149.26747, `boom-box` 166.694927. `outshine.raw` was re-rendered and came out the same. A picture that
*changed by design* does not reproduce to six decimals.

**And setting the range was measured and reverted, which is the finding this round produces.**
`min_lod = 0`, `max_lod = levels − 1` makes the chain reachable — and the picture gets **worse**:

| case | chain unreachable | chain reachable |
|---|---|---|
| `normal-tangent` | 229.330177 | **255** (saturated) |
| `normal-tangent-mirror` | 184.356962 | **255** |
| cases at a saturated tail | — | **6** |

`ours 255 against 0` at a single pixel, and **both materials are `OPAQUE`**, so it is not an alpha
cutout. PASS/FAIL counts did not move — 111/48 either way — so **nothing crossed a status while six
tails saturated**, which is the picture getting worse inside the same verdicts.

**Reverted, and the revert is confirmed to restore both tails byte-for-byte.** A worse picture with no
mechanism is not shipped. `max_lod` stays at zero **with the reason at the site**, so the sampler says
what it is rather than reading as a working chain.

**What the next round must explain before setting it again**: what makes a single pixel go fully bright
against a black reference when the chain becomes reachable, on an OPAQUE material. **UV derivatives at a
seam** are the obvious suspect — a discontinuity makes the hardware's LOD jump to the smallest level, and
the 1×1 level is the whole texture's average — **but that is a hypothesis and this item has already
spent two of those.** The instrument is to dump the selected LOD per pixel and look at where it is large.

**The mechanism, measured rather than hypothesised — and the instrument was the `uv` channel, not a
shader.** LOD is a property of the uv gradient and the texture size, both already on disk, so the
selected level is arithmetic. Over `normal-tangent`'s 294 876 covered pixels against its 2048-square map:

| selected LOD | pixels |
|---|---|
| ≈ **1.25** (ordinary minification, ~2.4 texels) | 288 807 |
| **> 8** | **6 069** |
| > 11 — the top of a 2048 chain | 1 149 |

**The distribution is bimodal and there is nothing in between.** 2 % of covered pixels select the
smallest levels, where the 1×1 level is *the whole texture averaged to one texel*. That is a **UV
discontinuity**: across a seam the finite difference is the width of the island, so ρ explodes and the
selection saturates.

**So the chain is not what is wrong.** LOD selection at a discontinuity is, and it was invisible while
`max_lod = 0` clamped every fetch to level 0 — **the clamp was hiding a second defect, which is why
removing it made six tails saturate.**

**Domain, because it is narrower than the headline**: the derivative here is a per-pixel finite
difference over **Cycles'** uv, not the GPU's per-quad derivative of ours. It is a proxy — but a split of
288 807 against 6 069 with nothing between is too stark to be an artefact of the proxy.

**What this makes the repair**: the chain needs LOD that survives a seam. The established answers are
padding the UV islands in the asset, or clamping the selection — and **which one applies is a ladder
question**, since padding is *patch the asset* and clamping is *fix the engine*. **Neither is chosen
here**, because this item has already spent two hypotheses and the next round should choose with the
references rather than with the first idea that fits.

**An instrument for the ladder question, tried and ruled out — so the next round does not spend it.**
The question is whether the high-LOD pixels are a **UV seam within one surface** (→ pad the islands,
*patch the asset*) or the derivative **crossing between two surfaces** in a quad (→ *fix the engine*).
The index channels look like the discriminator: compare the neighbour's `objectIndex` and
`materialIndex`.

**It is vacuous on this asset.** `normal-tangent` has **one** object index (2) and **one** material index
(3) over every covered pixel, so *the neighbour is the same surface* is true by construction and the
6 038-of-6 069 it reports says nothing. **Checked before the conclusion was written rather than after.**

**What would discriminate**: a per-pixel **primitive or triangle id**, which the corpus does not carry —
Cycles has no such pass among the ones enumerated on this host, so it is not one row in
`QUANTITY_PASSES`. The cheaper route is to look at the **uv field itself**: a seam shows as a jump whose
size is an island's width and whose direction is consistent along a line, while a stretched surface shows
a gradient that grows smoothly. **That is arithmetic on `oracle.uv.raw` and it is the next thing to try.**
