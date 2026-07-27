# E — Temporal Reprojection: Bayer Update Pattern, Reprojection Math, History Handling

> **Legacy studies of the demolished FBCloud* chain; kept for the noise/raymarch groundwork, see [../flightbox/render/clouds.md](../flightbox/render/clouds.md).**

Source: Schneider & Vos, SIGGRAPH 2015 (slides 90–92, EXACT — the HZD scheme); Hillaire, SIGGRAPH 2016
(slide 44, EXACT — the Frostbite scheme, which is a different but related design point); Nubis³ 2023
(slide 50–51, EXACT — explicit statement that the HZD-style full-16-frame-amortization scheme is
**not flight-capable**, which matters directly for FlightBox as a flight sim).

This is very likely the single highest-leverage fix available: FlightBox currently has **no temporal
accumulation at all** for clouds (confirmed in [04-raymarch-strategy.md §6](04-raymarch-strategy.md)) —
it fully re-renders the quarter-res cloud buffer every frame with a jitter offset that changes every
frame. That combination is close to a textbook definition of "looks like image noise/static."

## 1. Horizon Zero Dawn scheme — EXACT (Schneider 2015, slides 90–92)

> "Every frame we could use a quarter res buffer to update 1 out of 16 pixels for each 4×4 pixel block
> within our final image. We reproject the previous frame to ensure we have something persistent...
> and where we could not reproject, like the edge of the screen, we substitute the result from one of
> the low res buffers."
>
> "Nathan's idea made the shader 10× faster or more when we render this at half res and use filters to
> upscale it. ... our target performance is around 2 milliseconds."

Concretely:

1. Render at **quarter resolution** (already matches FlightBox's `CloudW/CloudH = Width/4, Height/4`).
2. Each frame, only **1 of the 16 pixels in each 4×4 block** is actually re-marched (a fixed or
   rotating Bayer/checkerboard-style index selects which one); the other 15 keep last frame's value.
3. **Reproject** the previous frame's full-res(ish) result into the current frame's camera (camera
   moved/rotated since last frame — no object motion vectors are needed for a sky-anchored volume,
   just the camera's view/projection delta).
4. Where reprojection fails (off-screen last frame, e.g. screen edges after a turn) — **fall back to
   the current low-res (quarter-res, freshly marched) buffer** rather than showing garbage.
5. Full convergence to a "fresh" image takes **16 frames** (one full pass through every sub-pixel of
   the 4×4 pattern) — acceptable for slow-moving distant cloud decks, NOT for fast camera motion close
   to cloud structure (this is exactly why Nubis³ later says this method is "Flight-Capable: No",
   slide 51 — see §3).

## 2. Reprojection math for a camera-only (no motion vectors) volume layer

Since clouds have no per-object motion vectors, reprojection needs only the camera's view-projection
delta between the previous and current frame — reconstruct a world-space (or ECEF, for FlightBox)
position for the *previous* frame's pixel, then re-project it through the *current* frame's
view-projection matrix:

```
worldPos_prev  = InvViewProj_prev * ndc_prev      // ndc_prev = (px, py, depth) of the OLD sample
ndc_curr       = ViewProj_curr * worldPos_prev
uv_curr        = ndc_curr.xy * 0.5 + 0.5
```

For a **sky-anchored volumetric layer with no fixed depth per pixel** (the cloud shell's intersection
distance varies every pixel and isn't cheaply available without re-marching), a common simplification
used in production sky/cloud reprojection is to reproject along a **fixed reference sphere/shell radius**
(e.g. the cloud base or a representative mid-shell radius) rather than the true hit distance — this
is approximate (parallax error grows with the difference between the true hit distance and the
reference radius) but avoids needing a full G-buffer depth for the cloud layer. Given FlightBox
already tracks the shell radii (`C.p0.x`/`C.p0.y`, absolute Mm) this reference-radius approach is a
direct fit: reproject at the **midpoint radius `(rBase+rTop)/2`**, not per-pixel true hit distance.

**Rejection/clamp**: standard TAA-style neighborhood clamping (compare the reprojected historic
sample's color against the min/max of the current 3×3 or 2×2 neighborhood of freshly marched samples;
if outside, clamp or reject and fall back to the fresh value) prevents ghosting when a cloud edge moves
faster than 1 pixel/frame at quarter-res — this is the same mechanism TAA implementations use, applied
to a scalar (radiance+alpha) buffer rather than the final color.

## 3. Frostbite (Hillaire 2016) scheme — a DIFFERENT, exact, alternative design point (slide 44)

> "Main view (with temporal re-projection): 0.720 ms [at 720p] / 1.090 ms [at 900p]."
> "Cloud main view resolution = 640×360 (720p/2)."

This is **half-resolution per axis** (i.e. quarter the pixel count, same as HZD's "quarter res" — the
two schemes actually land on the *same* pixel-count reduction, described differently: HZD says
"quarter res buffer," Frostbite says "720p/2" meaning half each dimension = also quarter the pixels).
The difference is in the *update pattern*: Frostbite's slide does not describe a 4×4/1-in-16 Bayer
pattern explicitly — it states simply "with temporal re-projection," implying every pixel of the
half-res buffer IS re-marched each frame, with the *history* blended in via reprojection (closer to a
standard TAA-style exponential history blend, e.g. `result = mix(reprojectedHistory, freshSample,
alpha)` with `alpha` around 0.05–0.15) rather than only refreshing 1/16th of pixels per frame. Total
budget: **0.835 ms at 720p** (hemisphere sampling 0.09 ms + main view 0.72 ms + optional planar
reflection 0.035 ms), measured on Xbox One.

**Both schemes require SOME form of history buffer + reprojection.** Neither renders a fully fresh,
uncorrelated-jitter image every single frame — that combination (which is what FlightBox currently
does) is not one of the two published schemes; it's closer to "neither," and inherits the visual noise
of full quarter-res undersampling with none of the temporal averaging that makes either published
scheme look clean.

## 4. Convergence behaviour / flight-capability warning (Nubis³ 2023, slide 51 — EXACT, directly relevant)

Nubis³'s own comparison table between their two historical methods states, verbatim structure:

| Capability | Vertical Profile (HZD-style 16-frame amortization) | Envelope Method |
|---|---|---|
| High frame-rates | Yes | Yes |
| **Flight-Capable** | **No** | **Yes** |

The reasoning given (slide 49, in the discussion around Vertical Profile): "worked for distant clouds
but not for nearby clouds because the image would not be able to resolve in time when the camera moves
quickly." **This is a direct, sourced warning that FlightBox — an F-16 sim, where the camera moves
fast and close to cloud structure by design — should NOT adopt the literal 16-frame/1-in-16-pixel HZD
scheme for near clouds.** The safer target for a flight sim is the Frostbite-style every-pixel-
refreshed-half-res + reprojected-history blend (§3), or Nubis³'s own envelope two-pass resolution split
([04-raymarch-strategy.md §4](04-raymarch-strategy.md)), NOT the aggressive amortization scheme. This
is an important corrective to the initial research brief, which cited the HZD 4×4-Bayer scheme as if
it were the default recommendation — for FlightBox's use case it explicitly is not, per the same
studio's own later retrospective.

## 5. Recommendation for FlightBox given the above

1. Keep quarter-res (already correct scale).
2. Add a **history buffer** (a second `CloudLowTex`, ping-ponged) and reproject at the cloud
   mid-shell radius (§2).
3. Blend fresh-sample and reprojected-history with an **exponential moving average**
   (`alpha ≈ 0.1–0.2`, tune upward — closer to 1.0 — for near/fast camera motion, downward for distant
   decks), rather than the aggressive 1-in-16 Bayer scheme Nubis³ itself flags as not flight-capable.
4. Keep the jitter (it's needed to convert step-banding into dither), but its role changes from
   "hide banding within one frame" to "hide banding across the accumulation window" — same code, but
   now it actually converges instead of flickering, because there is now a history buffer to average
   into.
5. Add neighborhood clamp/reject on the history sample to avoid ghosting on fast cloud-edge motion
   (§2) — this, more than the exact blend factor, is what prevents visible smearing during a bank turn.
