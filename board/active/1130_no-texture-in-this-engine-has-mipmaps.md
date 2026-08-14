Type: bug
Area: render
Depends: 1131, 1132, 1135
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

**Measured from the `uv` field: it is a JUMP, not a stretch.**

| | |
|---|---|
| ρ at the pixel against the median of its own 5×5 neighbourhood | **712×** p50, 985× p90 |
| isolated spikes (ratio > 10) | **98.7 %** |
| contiguous horizontal run length | **1 of 3** — single-pixel lines |
| share of covered pixels | **2.06 %** |

**Single-pixel derivative spikes in an otherwise ordinary neighbourhood** is the signature of a quad
straddling a **UV discontinuity** — and on this asset's 4×5 grid of cells that is the cell boundaries,
one pixel wide. It also explains why the index test was vacuous: it was reaching for exactly this, and
the boundary being crossed is a **UV island** boundary rather than a surface one, which one object and
one material cannot distinguish.

**The rung is still not chosen here, and now the question is sharp enough to choose properly.** The
artifact is intrinsic to computing LOD from **screen-space differences**, which every rasteriser does and
which Cycles does not — it takes the footprint analytically per intersection. So:

- ***fix the engine*** would mean the LOD selection surviving a discontinuity — clamping, or explicit
  gradients where the derivative is available analytically.
- ***patch the asset*** would mean padding the UV islands, which is the standard authoring answer to
  precisely this and is applied identically to both sides.

**What decides it is whether a rasteriser is expected to survive an unpadded atlas** — a question with a
literature answer, and the reason this item says *choose with the references rather than the first idea
that fits*.

**The references, and they describe this exactly.** The mechanism is documented rather than novel: *the
derivative of texture coordinates is much larger at the boundary of any texture patch … resulting in
selection of texels at higher mip-map levels … and the sudden jump of the mip-map level selection
produces the seam artifact.* The standard authoring answer is **edge padding** — extending valid edge
texels into a gutter so the level the seam quad reaches for still holds neighbouring colour rather than
an unrelated island. Kyle Halladay's *Minimizing Mip Map Artifacts In Atlassed Textures* is the same
problem end to end.

**Recommendation — rung 3, *patch the asset*, by padding — and the reason rung 1 does not apply.** A
*fix the engine* answer means computing the derivative some way other than differencing neighbours, and
at a quad straddling two islands **the information required is genuinely absent**: the quad holds
fragments from two unrelated regions of uv space, and no per-fragment quantity distinguishes them. That
is why the index-channel test was vacuous rather than merely under-powered. The engine-side alternatives
are explicit gradients — which need the island each fragment belongs to, i.e. the missing information —
or clamping `max_lod`, which blurs everything to hide a one-pixel edge.

**This is a recommendation with its evidence, not a ruling**, and it is written at the end of a long
session by someone who got two closures wrong tonight. **Evaluate it before taking it.** What would
overturn it: an engine-side derivative source that does distinguish islands, or a measurement showing
the artifact survives padding.

Sources: [Halladay, *Minimizing Mip Map Artifacts In Atlassed
Textures*](https://kylehalladay.com/blog/tutorial/2016/11/04/Texture-Atlassing-With-Mips.html) ·
[*Texture atlases, wrapping and mip mapping*, 0 FPS](https://0fps.net/2013/07/09/texture-atlases-wrapping-and-mip-mapping/) ·
[*Gutter space padding for texture atlases*](https://image-ppubs.uspto.gov/dirsearch-public/print/downloadPdf/10839590)

**RETRACTED: the padding recommendation is undermined by the texture itself.**

| | |
|---|---|
| normal map | 2048² |
| texels that are flat `(128,128,255)` | **93.4 %** |
| of the texels the frame samples, flat | **245 637 of 294 876 — 83 %** |

**The map is almost entirely neutral.** There is abundant gutter — 3 670 976 flat unsampled texels — so
padding would dilate flat into flat and change nothing. And a high-LOD fetch into a 93 %-neutral map
returns approximately the **neutral normal**, which makes a surface shade **flatter**. **That cannot
produce `ours 255 against 0`.**

**So the saturation is probably not the normal map.** The base-colour texture carries this asset's cell
labels and glyphs; averaging those toward grey against a black or white reference is exactly a 255-code
difference. **The seam mechanism stands** — the derivative spikes are measured and real — **but which
texture turns it into a saturated pixel is unestablished, and I assumed the one the item was about.**

**What tests it, and it is cheap**: enable `max_lod` for **one slot at a time** — colour, then normal,
then metal-rough — and see which one saturates the six tails. That is three runs and no new instrument.

**This is the fourth check tonight that undermined a conclusion**, and like the last one it ran before
anything was built on it. The recommendation above should be read as **withdrawn pending that test**, not
as guidance.

**The slot is named, by enabling the chain one slot at a time.** The four `Upload` calls partition
cleanly — colour `Srgb+Value`, normal `Linear+Direction`, metal-rough `Linear+Value`, emissive
`Srgb+Value` — so each predicate names exactly one.

| chain enabled for | `normal-tangent` | `normal-tangent-mirror` |
|---|---|---|
| baseline (nothing) | 229.330177 | 184.356962 |
| colour + emissive (`Srgb`) | **229.330177** | **184.356962** |
| normal (`Direction`) | **229.330177** | **184.356962** |
| **metal-rough (`Linear`+`Value`)** | **255** | **255** |

**It is the metal-rough map, and it was never the normal map** — the item's own subject was the wrong
suspect twice over, first as the padding target and then as the saturating slot.

**The mechanism is coherent**: averaging that texture across a seam gives a wrong **roughness**, and at a
specular highlight roughness is the parameter the picture is most sensitive to — a tight mirror dot
against a broad sheen is a full-scale difference. Occlusion rides in the same texture's red channel under
glTF's ORM packing and is averaged with it.

**Which reopens the padding question on the right texture.** The normal map is 93 % neutral so padding it
changes nothing; **whether the metal-rough map has the same neutrality is unmeasured**, and it is the
same cheap check applied to the right file. **Do that before choosing a rung.**

**The diagnostic is reverted** — three runs, no instrument left behind, and the baseline restored.

**And the channel, which completes the mechanism.** The metal-rough map's content:

| most common texel | share | reading under ORM |
|---|---|---|
| `(255, 76, 0)` | 10.1 % | roughness 76, **dielectric** |
| `(243, 0, 255)` | 8.6 % | **mirror-smooth**, **metal** |
| `(242, 5, 0)` | 5.3 % | near-smooth, dielectric |

**Roughness spans the full 0–255 and metalness is effectively binary — 0 and 255 in adjacent regions.**
Averaging across a seam therefore yields **metalness ≈ 128: a half-metal, which is not a material that
exists**, beside a mirror that has become mid-rough. That is a full-scale radiance difference and it is
the **ORM-packing hazard**, not anything about normals.

**So the padding question is answered on the right texture and the answer is no.** There is no neutral
gutter to dilate into — the map is packed with high-contrast meaningful values, and dilating one island's
metalness into its neighbour's is the same corruption by another route.

**Which moves the rung.** *Patch the asset* has nothing to pad. What remains is **fix the engine**, and
the literature is specific here: **binary metalness must not be naively box-averaged down a chain** —
the filtered value is not a material, and the standard answers are to keep metalness at level 0, to
filter it conservatively rather than linearly, or to carry the roughness change the averaging implies.
**That is a decision with references and it is the next round's**, not this one's — but the option space
is now three named engine-side treatments rather than an open question.

**The last link, measured rather than reasoned: metalness is EXACTLY binary.**

| channel | at 0–15 | at 240–255 | **in between** |
|---|---|---|---|
| metalness | 66.3 % | 33.7 % | **0.000 %** |
| roughness | 59.8 % | 0.3 % | 39.9 % |

**Zero texels lie between the extremes of metalness**, so **any box filter produces a value the asset
never contains.** The whole-map mean is 86 — so a seam fetch returns **metalness 0.34** whether the texel
under it was metal (1.00) or dielectric (0.00). Both become a third-metal, which appears nowhere in the
source and is not a material.

**Roughness is genuinely continuous** — 39.9 % of texels lie between — **so averaging it is correct.**

**Which narrows the engine fix to one channel rather than the texture.** Occlusion and roughness mip
correctly by a box filter; **metalness cannot, because its distribution has no interior.** The three
named treatments reduce to a choice about that channel alone: hold it at level 0, filter it by majority
rather than by mean, or accept a value the asset does not contain. **That is the decision, and it is now
small enough to make with a reference and a measurement rather than a preference.**

**The rung was chosen, implemented and measured, and it did not decide the item.**

`TexelKind` gained a companion: `IndexChannelsOf` reads the texels and reports which channels take **at
most two distinct values**, and those are snapped to a value the four sources contain instead of
averaged. The predicate needs no threshold — 2 against 85 and 180 has no midpoint — and it is derived
from the texture rather than read off the slot, so it says something about the next format too.
`test/unit/render/stages/AnIndexChannelKeepsOnlyItsOwnValues.cpp` holds both halves: that a three-valued
channel is still a quantity, and that the plain mean of a two-valued one is a value it never takes.

**It did not bring the tail down, and a sweep says why.**

| `max_lod` | `picture_max_delta_code`, `normal-tangent` |
|---|---|
| 0 | 229.330177 |
| **1** | **255** |
| 2 · 4 · 8 · 20 | 255 |

**Level 1 ALONE saturates.** One halving. Every argument about a seam fetch reaching the top of a chain
is dead, my own included — the LOD spike is real and it is not what this is.

**What one halving does is flatten the normal map**, and the metric that reports it is
`rowN_pairM_geometry_matches_normalmap_p95_relative`: `0.13879225` → `0.27571386` against `0.1579751`,
on five rows. **That metric is not collateral and it is not the wrong instrument** — it compares two
regions of our own render, and its bound is the geometric mean of two measured populations separated by
4.53×. It reports exactly what it was built to report.

**And the picture bound does not move at all: 20 of 34 cases within it either way, 48 failing tests
either way, no case changing verdict.** So the entire shipping decision rests on that one metric, which
is `board:1131`.

**The missing term is Toksvig's, and this tree already named it.** `TexelChain.h` records that the
shortfall of an averaged normal is lost perturbation, that carrying it as roughness is Toksvig 2005 and
LEAN/CLEAN, and that it was not taken *for a reason about the oracle*. That reason is now the open
question rather than a settled one, and `board:1131` is the measurement that settles it. Turning five
derived metrics red to reach a picture bound that does not move is not a trade worth making, so the
sampler stays pinned to level 0 with the blocker named in the source.

**`board:1131` is closed and it answered this item's open question.**

Asked the same self-consistency question, **the oracle's own two cells disagree by 0.04859..0.08692**
across the fifteen live cells — it does not reach zero either, but it stays there. Ours with the chain
readable reaches **0.27571386**, which is **3.2× the oracle on the same cell**. Cycles filters this
texture. So *filtering flattens for everyone* is false, the bound is not stale, and **our box-filtered
normal map over-flattens against a reference that filters and does not.**

**The cause is the order, not the kernel.** Cycles shades many rays per pixel, each against a normal at
its own differential, and averages RADIANCE; we average the NORMAL and shade once. The gap between
`average-then-shade` and `shade-then-average` is precisely the perturbation renormalising discards.

**And the recorded reason for not carrying it is refuted.** `TexelChain.h` says the Toksvig term was
declined because it *"would move us away from the thing we are measured against to make a number
smaller."* The oracle is not below us: it is 3.2× below our filtered result and beside our unfiltered
one, so the term moves us TOWARD the oracle. **That objection was an assumption, it is now measured, and
it was wrong.**

So this item's remaining work is one named thing: **carry the normalisation shortfall as roughness**
(Toksvig 2005; LEAN/CLEAN; *Real-Time Rendering* 4e ch. 9.13), then set `max_lod` and check the five
`geometry_matches_normalmap` rows against the oracle's own column rather than against zero. The
acceptance is stated before: ours must land in **0.04859..0.08692**, where the oracle is, and the picture
bound must not fall below **20 of 34**.

**Toksvig is built, and it closes 3.2 % of the distance.**

`ToksvigA2` and `RoughenedBy` live in `src/render/stages/MetalRoughBrdf.h` in both halves, as that file
requires — the C++ definition and its MSL transliteration, written twice on purpose. The chain keeps the
mean resultant length in the normal texture's alpha, which glTF leaves undefined, **accumulating rather
than remeasured**: every level above the first is built from already-renormalised texels, so a length
measured there would report a deep level as flat. `ALostPerturbationComesBackAsRoughness.cpp` holds both
halves and the three limits as values.

**One defect of my own, caught by measuring the thing I had asserted.** The comment claimed `l = 1` is
the identity so nothing moves for the code being present. True in algebra; false in binary32 — the round
trip through `r*r`, `alpha*alpha` and two roots moved an unfiltered picture in its fourth decimal,
`0.056763454` where it had been `0.056763588`. `RoughenedBy` now takes the identity as an identity, and
the test asserts **bit equality** rather than nearness, because *nearly* is exactly what the defect
looked like.

**The acceptance stated before the round was not met, so `max_lod` stays at 0.** Ours had to land in
`0.04859..0.08692`; it lands at `0.17024..0.26971`, two to three times outside, and
`picture_max_delta_code` stays 255. `board:1132` names why: Toksvig is a **specular** correction and the
dominant error is a **direction** error that moves the diffuse term, first order and out of its reach.
The proof is `pair1` — the cells where we were BEST unfiltered, better than the oracle itself — going to
`0.18338..0.25048` under filtering. A specular correction being too small cannot do that.

## The number this item was built on is wrong by about 300x

`board:1134` found the sampler's magnification filter deciding every pixel, which cannot coexist with
the **1.42 texels per screen pixel** and the **288 807 px at LOD ≈ 1.25** recorded above. So it was
measured directly, with an instrument that cannot be confused about a projection: build the chain, fill
**every level above 0 with zero**, and count how the differing-pixel population moves. A pixel that
never reads a level above 0 cannot notice.

| case | differing px | with levels > 0 poisoned | **pixels reading a level above 0** |
|---|---|---|---|
| `texture/simple-texture` | 34 797 | 34 797 | **0** |
| `materials/normal-tangent` | 198 860 | 199 780 | **~920** |
| `materials/normal-tangent-mirror` | 199 205 | 200 066 | **~861** |
| `materials/scifi-helmet` | 41 440 | 47 231 | **~5 791** |

**About 920 pixels, not 288 807.** `simple-texture` reads no level above 0 at all. **Mipmaps are not a
broad picture question on this corpus** — they are a question about roughly a tenth of a percent of the
frame, which is precisely the spike population already measured at UV island boundaries.

**Everything this item concluded stays true and changes meaning.** Enabling `max_lod` really did saturate
six cases at 255 — because a MAX is decided by exactly those isolated pixels, and they read *deep*
levels. The index rule, Toksvig's term and the diffuse-direction finding in `board:1132` are all correct
about the mechanism; what they were wrong about is the **population**, and this repository already names
that failure: *the number was right and about something else*, in its **input set too wide** face. A
correction sized for 288 807 pixels was being judged by 920.

**What this changes about the work.** Carrying the shortfall as roughness cannot be validated on a
population of 920 isolated pixels — `rowN_pairM_geometry_matches_normalmap` reads 64×64 rectangles that
are almost entirely magnified, so it is measuring the term at pixels the term does not apply to. **The
instrument for a mip correction has to be a case that actually minifies**, and no case in this corpus
does so broadly. That is the next thing to build, and it is `board:0078`'s territory: a subject at a
distance, where texels outnumber pixels across the whole frame.

## The blocker was never in this engine

`board:1135` built the case this item said was missing — `render/texture/four-texels-per-pixel`, every
covered pixel minifying at exactly 4 texels per pixel — and it answers the whole item:

| ours ＼ oracle | 1 spp / 0.01 px box | 256 spp / 1.0 px box |
|---|---|---|
| **single level** | **0.190 codes — PASS** | 37.864 — FAIL |
| **mip-filtered** | 37.412 — FAIL | **5.340 codes — PASS** |

**Our mip chain agrees with an integrating oracle to 5.340 codes, inside the picture bound of
6.4354338.** The chain is right. What it was measured against is a reference that renders one sample
through a 0.01 px box — a point sampler by design, which every manifest in this tree states as its own
purpose, and which has no opinion about a filter at all. The oracle disagrees with **itself** by up to
37.880 codes between the two recipes.

**So every number this item recorded against the chain was a number about the recipe**: `normal-tangent`
229.33018 → 255, `texture-coordinate-test` 10.295625 → 202.0679, `scifi-helmet` 15.457417 → 81.229848.
The index rule, Toksvig's term and `board:1132`'s diffuse-direction finding were each judged against an
instrument with no standing to judge them — the same failure this item already caught itself committing
about a population, now about the reference.

**What remains is not engineering, it is one decision**, and it is `board:1135`: which cases render an
integrating oracle. When they do, `kChainIsReadable` becomes `true` and this item closes on the number
already measured.

**One correction of my own, recorded because it briefly stood as a result.** An early run reported this
case at `63.750005` codes with the chain off and `37.412079` with it on, read as a 41 % improvement. The
`63.750005` came from a scratch binary still carrying a diagnostic that zeroed every level above 0 — it
measured the probe. The real single-level figure against the point-sampling oracle is `0.190`, and
against the integrating one `37.864`.
