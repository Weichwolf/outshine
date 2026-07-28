# Cloud Metrics & Known Limitations (2026-07-23)

> **Legacy studies of the demolished FBCloud* chain; kept for the noise/raymarch groundwork, see [../flightbox/render/clouds.md](../clouds.md).**

State of the volumetric cloud pass after the research-driven convergence loop. Deployed state = **(A)**:
soft S-curve base + opacity-from-extinction decoupling + `pow(d_eroded, 2.5)` post-erosion edge
sharpening. Passes the far/mid field of the triple regression (high-sun, AC7-sunset, undercast).

## Metric change: opacity replaces the d-core histogram

The old acceptance metric was "% of voxels with post-coverage density `d > 0.9` (solid cores)". After
the **decoupling** (soft smoothstep base + opacity moved into the extinction/density scale), the base
`d` is *intentionally* soft — cores no longer reach `d≈1` and never should. `FB_SHAPEHIST` reports
`solid cores ≈ 0.2%`, which is correct-by-design, not a regression.

- Opacity now = **optical depth** `σ·path = pow(d_eroded, γ)·kExtinct·densityScale·pathLength`, judged
  **visually** (is the deck opaque? do cores read solid?), not from a `d` histogram.
- `ShapeStats`/`FB_SHAPEHIST` still replicates the OLD dome formula and is therefore **stale** — it must
  be rewritten to replicate the shipping `density()` (post-decoupling, and post-cell-field once (B)
  lands) or it is decoration. Until then, treat its "solid cores" line as meaningless; use the render.

## Known limitations (suspected roots)

| Artifact | Where | Suspected root | Status |
|---|---|---|---|
| **Near-field grazing glass-plates / cliff-edges** | near/bottom of frame, worst at high sun | continuous undulating **sheet** density → large screen-space cells at grazing show translucent flat plates + straight cliff edges. Structural: a sheet has no horizontal discreteness. | THE remaining artifact family. Fix candidate = F1-cell discreteness (B), pending the one tuning round. |
| **High-sun residual smear / cliff-plates** | right/centre, flat noon light | same sheet root — flat light reveals the undulations as view-coherent bands; pow-extinction reduced but did not eliminate. | reduced (A), open. |
| **Head speckles / faint fringe** | lit crests | top-biased detail erosion at grazing light (fell-franse), now subtle | accepted non-blocking |
| **Horizon sky-weave** | grazing horizon band | faint quarter-res/temporal residual | accepted non-blocking |
| **Native `--pitch` EVS renders no stars** | native only | regression from the 720p-FrameTex present refactor + rgba16float switch (bisect those) | separate, fix before lights-client |

## What is proven / accepted

- **TAA-upsample splat**: proven to reconstruct full resolution from the quarter-res march in a **static**
  N=128 accumulation (the pipeline + jitter offset are correct; the win needed the weight-accumulation
  splat with an exact 4×4 phase grid, not exponential blend). Live path uses the same placement.
- **Two-tier march** (grazing-slab fix), **3D-noise mips** (grazing moiré), **cone sunOD** (low-sun
  banding), **dome envelope** (plateaus→domes), **Perlin-Worley→smooth-S-curve base** (angular Voronoi
  ridges → rounded billows), **decouple + post-erosion sharpening** (corduroy↔smear tension).
- Far/mid field of AC7-sunset and undercast **pass**. High-sun and near-field **do not** — one artifact
  family (near-field grazing plates), the target of the F1-cell round.

## Real vs current cloud structure (why the near-field plates persist)

Real stratocumulus is **cellular** — closed convection cells: discrete rounded hills with gaps in a
honeycomb, so no continuous surface exists to band or plate at grazing. Our density is a continuous
`coverage(3D-noise) × verticalProfile(h/topH)` **sheet**. The structural fix (Candidate 12) is a
**F1-round Worley cell field** (`(1−F1)²` = round bumps, *not* the angular F2−F1 Voronoi walls) driving
both the horizontal coverage discreteness and the tower height — with the coverage curve tuned so a
**closed** deck (high coverage) merges to a smooth sheet (the undercast criterion) while a **broken**
deck (mid coverage) is honestly discrete.

## High-cloud pivot (2026-07-23) + the 512² cell field + KNOWN LIMITATIONS

**Pivot** (user): default weather = a HIGH broken layer (base ~6–12 km, `FB_CLOUD_BASE_M`), so the
~2 km loiter always sees it in the mid/far field. The near-field artifact family (flythrough, glass
plates) is structurally bypassed and DEFERRED, not solved.

**512² cell texture** (ent-deferred, B mode `FB_CLOUD_CELLS=1`): the F1-round cell field moved from the
128³ base G channel to a dedicated **512² 2D texture** (`kCloudCellCS`, `pow(1−F1,2)` hills, ~57
texel/cell vs ~14). Root cause of the earlier **tilted plates** was NOT resolution alone: the 128³-G
cells came from `worley2D(uv.xy)` = constant along the texture **z**-axis, which maps to **ECEF-Z (the
polar axis)** when sampled → a ~43° tilt at 47°N. The 512² field is sampled by the **horizontal
tangent-plane position** (`p6/p7`, basis from the camera up) so cells extrude along **local up** =
vertical round columns. Shape uses a rounded arch cap + ragged base (per-column base-height jitter +
base erosion, B-only), and a horizon distance-fade (`t > ~60 km` dissolves into haze, B-only) hides the
quarter-res far mush. Base lighting: `ambGrad` gained a **0.30 floor** so dense sun-shadowed bases read
mid-grey (real cumulus), not near-black.

**KNOWN LIMITATION — overhead steep-up view** (accepted, team-lead 2026-07-23; the Half→Full-res cloud
rebuild is NOT approved — the perf mandate outranks steep-look-up aesthetics): a cloud viewed **base-on
from directly below** (steep up-pitch) still reads as a flattish **slab** with **quarter-res blocky
edges**. Two compounding causes: (1) from directly below you see the flat cumulus base (roughened but
fundamentally flat), and (2) the cloud march runs at **quarter resolution** — a near/overhead cloud
fills a large screen area so the 320×180 march + temporal reconstruction shows blocks. The real loiter
attitude (banked, ~level) never looks straight up, so the default experience (mid/far scattered field,
clean horizon fade) is unaffected; the artifact appears only when the pilot cranes straight up.

**Night clouds — MOONLIGHT term (added 2026-07-23)**: the march now lights from the **moon** as a
second source when it is up (generalized `lightOD(pos, dir, jit)` — the old `sunOD` — reused toward
`A.moonDir`; silver tint 0.72/0.80/1.0, intensity `sun * A.moonDir.w * 0.10 * FB_MOONLIGHT`,
smoothstep-faded near moonset, `*(1 - daylight)` so it never tints day). Moonlit clouds now read as
faint silver-grey masses that occlude stars over the city lights. NOTE the 0.10 (~sun/10) factor is a
RENDER value, not the physical ~sun/400 — the night exposure is boosted (stars/lights visible), so the
moonlight needs a matching boost to read. New moon (phase~0) or moon-down → near-black again, as real.
