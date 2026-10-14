# Closed needle cost comparison

Owner: [#143](https://github.com/Weichwolf/outshine/issues/143).
The control is the complete four-species young tile from #131, delivered by #141.
All plants, wood, needle IDs/counts/poses/scales, materials, lights, camera and
2560×1440 remain fixed. Only twelve closed needle prototypes change.
The sparse crowns still lack botanical approval; this is a cost experiment.

```sh
.venv/bin/python test/experiments/vegetation/needle_lod_experiment.py
.venv/bin/python test/experiments/vegetation/needle_lod_experiment.py --position-only
.venv/bin/python test/experiments/vegetation/conifer_preview.py -- \
  --scenario build/experiments/vegetation/needle-lod/position-area/selected.json \
  --output build/experiments/vegetation/needle-lod/position-area/selected-render.png
```

Products/reports go to `build/experiments/vegetation/needle-lod/position-area`
or `position-only`, independently of the control cache and current showcase.
The first position-only trial is retained separately as a failed counterexample.
Render image/metadata names differ from the scenario; the renderer rejects output
paths that would overwrite its input scenario, product or material files.
It dropped projected needle area by up to 42% and produced pixels up to 27/255
brighter despite the positional envelope. Its lower cost is not an approval.

## Ring schedules and positional envelope

`Needle.mesh(..., axial_sampling="profile")` includes the radial maximum at
`taper_start`. Half the axial segments span the body, the remainder span the tip.
The final tip vertex is shared. Uniform sampling remains the default with exactly
the previous buffers and unchanged physical recipes. Doubling an even segment
count nests the profile rings; arbitrary segment counts need not nest.
This allocation is a compared heuristic, not a globally optimal triangulation.

For a smooth section cell, write its physical surface as
`F(t,phi) = c(t) + r(t) R(t) s(phi)`. Let `h`/`H` be that cell's actual axial/angular
spans, `rmax`/`r1`/`r2` bounds on radius and its derivatives, and `w=|bend|+|twist|`.
The circular-arc frame is a product of fixed-speed rotations. Thus
`|R'| <= w`, `|R''| <= w²`, `|c''| <= length*|bend|`, and `|s| <= 1`.
Use the section's one-sided `s1`/`s2` derivative bounds to obtain:

```text
Mtt    = length*|bend| + r2 + 2*r1*w + rmax*w²
Mphiphi = rmax*s2
Mtphi  = (r1 + rmax*w)*s1
e      <= Mtt*h²/8 + Mphiphi*H²/8 + Mtphi*h*H/4
```

Sequential one-dimensional linear interpolation gives the first two terms.
The bilinear patch differs from either diagonal triangle by at most one quarter
of its mixed finite difference, giving the last term. At the tip the upper edge
collapses to one point; the bilinear patch already lies within that triangle.
Hard section boundaries are explicit mesh edges. The radial profile is C1 across
the peak; each cell takes the maximum one-sided second derivative. The planar
convex base cap is bounded by its section-boundary term.

`Camera.projected_span` supplies a perspective Lipschitz bound over the complete
product bounds, including physical reach and largest scale. A candidate/control
pair uses the sum of their surface envelopes; an unchanged control mesh has zero
pair error. The predeclared pair limit is 0.25 px in this camera/profile.
The analytic real-arithmetic derivation uses Float64 with a roundoff allowance,
not certified interval arithmetic. Independent 9×9 samples per parameter cell,
including piecewise/negative-bend/twist/taper counterexamples, check interpolation.

## Independent coverage and actual rendering

Continuous unions of projected triangles are compared in 64 fixed sphere views
plus six axial views. GEOS snap rounding uses a 1 nm grid to avoid nearly coincident
triangle failures. The predeclared sampled area limit is 5% against the control.
These views do not prove a global coverage, normal, shadow or shader error bound.
Closedness, winding, positive volume and differential normals are checked separately.

Five alternating orders measure prototype construction CPU time; whole-tile
EEVEE renders use identical 64 samples. Open the resulting PNG and compare against
the control with `test/scripts/pixels.py`. Report local needle/shadow changes as
well as full-frame statistics: a small global mean can hide a failed plant.
Render metadata pins all input/renderer bytes. EEVEE elapsed time is an experiment
cost, not native GPU, frame, device or A18 Pro evidence. Only an accepted current
whole-tile image may replace one of the four showcase pictures.
