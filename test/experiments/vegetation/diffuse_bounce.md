# One diffuse neighbour in the actual finite stand — #477

This is a CPU correctness reference for the existing reciprocal diffuse leaf
model. It adds one physically defined surface exchange, independently of the
EEVEE neighbour-shadow failure in #473. All four actual1920×1080 primary-ray
images were opened. Backlit grass receives a measurable, spatially varying
neighbour contribution; geometry, materials and direct light stay unchanged.
This is a bounded reference for subsequent scaling experiments, not a proposed
per-frame world renderer or complete natural-plant lighting.

## Sources and defined transport

Local SIGGRAPH1995 [Weber/Penn](../../../doc/references/vegetation/siggraph/1995-creation-and-rendering-of-realistic-trees.pdf)
provides the plant/organ-orientation and prototype/LOD context; page7 was opened.
It is not claimed as a derivation of this transport estimator.
EGSR2007 [Habel et al.](../../../doc/references/vegetation/egsr/2007-real-time-leaf-translucency.pdf),
optically opened page2, explicitly separates local reflection/translucency from
indirect/environment illumination. We reuse our already installed diffuse
`leaf_diffuse_response`/`optical_mixture`, not that paper's full BSSRDF.
[Bruneton/Neyret Forests](../../../doc/references/vegetation/eg/2012-real-time-forests.pdf),
optically opened pages4/5, uses distinct coverage and participating-medium
approximations. Its exponential extinction is not transmission through one leaf.
No vendor source, new numerical SpeedTree oracle or web research.

Let D(x,v) be the existing directly illuminated outgoing radiance, including
opaque geometric visibility toward each declared beam. For outgoing v, orient
n toward v. Our sheet has f=R/pi on that hemisphere and f=T/pi on the opposite
hemisphere. For the first intersection y along direction w, define

    (K D)(x,v) = integral f(x,v,w) D(y,-w) |n dot w| dw.

A miss contributes zero: the experiment has finite geometry and a black boundary.
The rendered result is D+KD. The neighbour term has two surface events and no
further exchanges. A black opaque neighbour does not relay the sun beam;
hemispherical T scatters over directions and is not multiplied onto a straight
shadow ray. Specular reflection, ballistic transmission and BSSRDF are absent.

`sample_sheet` reuses the existing maximum-channel R/T mixture. With probability
q_R/q_T select a hemisphere and sample density |n dot w|/pi there. The estimator
weight is R/q_R or T/q_T: the cosine/pi factors cancel. Absorption remains in the
weight; there is no extra energy injection or unreported ambient term. Existing
optical validation requires max(R)+max(T)<=1, so these weights are bounded by1.
Counter-based NumPy Philox assigns a full four-word counter to each path, retaining
identical samples across packet boundaries. Seeds73/74 separate camera/scatter
samples; front/back light states share geometry/path samples and common sky beams.

For a differential receiver under a parallel rectangle with half sizes a,b and
separation h, x=a/h, y=b/h, A=sqrt(1+x²), B=sqrt(1+y²):

    F = (2/pi) [ (x/A) atan(y/A) + (y/B) atan(x/B) ].

An illuminated uniform upper sheet gives KD=R_receiver*T_upper*E*F/pi.
Independent64×64 Gauss-Legendre area integration agrees with the closed form to
12decimal places.65536 cosine samples at h1/h2 satisfy the fixed six-worst-case-
Bernoulli-sigma allowance plus1e-6 bias tolerance. A naive T-only factor fails
both distances. White-furnace energy, cosine distribution, rotated weighted
angular moment, reversed winding, opaque/black neighbours and packet identity
have separate checks. Zero-energy receivers issue no neighbour geometry query.

The omitted higher orders are not certified negligible. For a channel bound
rho=max(R+T,ground_R), ||K||_infinity<=rho. A conservative remaining-tail bound is
rho²*Dmax/(1-rho), where Dmax=max(R,T,ground_R)*sum(beam irradiance)/pi.
For the authored green channel rho0.34 this bound is about0.0439, still large.
Thus this delivery establishes the first exchange and a reference method, not
convergence of the full canopy solution.

## Geometry, actual images and numerical evidence

The source remains saved stand
`1cb40500dc435ebf78573379766dbeae51cb3f452bf76053184a2f78fe041a9a`:
2368 roots,8 existing unidentified grass prototypes,284160 blade triangles.
Add the same flat6×6m ground:284164 vertices/284162 triangles total. No new roots,
organs, source adaptation or cached illumination. The existing saved reader checks
source hashes; all finished geometry is loaded, and no generator is invoked.
Its current reader also decodes the old contact maps; this reference does not
claim a minimal production asset reader.

Full orthographic camera: distance4m, width1.4m, view(sqrt(3)/2,0,0.5),1920×1080,
four independently jittered primary/scatter samples per pixel. Complete geometry
depths are checked inside0.1..10m. Both original light states retain their sun and
four sky beams, leaf R/T and substrate albedo. RGB is averaged linearly, then
encoded as sRGB without tone-map/exposure adjustment. All direct, neighbour and
uncertainty arrays and original image hashes are retained locally. Four images
show direct versus direct+one-neighbour under each light state.

Full-image mean green is0.135471 direct +0.009734 neighbour in front light;
0.041666+0.009824 in back light. Estimated standard error of the whole-image
neighbour mean is5.05e-6/5.38e-6, under independent-sampling assumptions. Mean
per-pixel error is larger; four samples leave visible local noise/aliasing.
These errors exclude model truncation, Float32 geometry and finite-boundary bias.
The two light states use correlated samples, so their uncertainties are not
independent. The EEVEE image is not an acceptance oracle and no coefficient is
fitted to it. Botanical shape/surface validation remains separate and unfinished.

`scatter_conformance.py` independently checks96 actual full-resolution primary
rays,96 additional cosine rays at their contacts and576 sunlight/sky rays against
Float64 slab boxes plus plane/Gram intersections: all768 agree. Nearest-hit
distances/points agree within the fixed1e-9m limit. The Embree path reconstructs
in Float64 and sends ambiguous hits to the independent nearest-triangle query.
Float32 misses are not globally certified; sampled agreement is not a proof for
all grazing rays or a native/device conformance claim. Metric origin bias2um
is explicit. Full world neighbours beyond this finite scene are not inferred.

## Work, costs and retained alternatives

Proof root `/tmp/outshine-vegetation-diffuse-bounce-proof-a517943ec`; receipts pin
source commits, load and terminal status. Full two-light-state rendering at
760651e64 terminates0:370.57s wall/370.05s CPU,459.72MiB peak RSS, Intel i5-1135G7,
NumPy/embreex CPU, no GPU claim.8,294,400 camera paths,16,588,800 total nearest
rays,86,973,936 declared-beam shadow rays. Primary/secondary geometry and four
common sky terms are reused between light states instead of rebuilding a scene.
One finite acceleration structure is built once; no unbounded ray recursion.

Known encoded input copies:464,486,400B nearest +2,435,270,208B shadow queries.
There are three encoded buffers per packet:2026 nearest and12156 shadow packets,
42546 such buffers, plus two encoded geometry buffers6,819,912B. Joined input
geometry13,639,824B, lazy independent geometry/bounds34,099,440B, output arrays
149,299,200B. These are identified buffers/copies, not total NumPy/Embree/OS
allocations, internal traversal steps or driver memory; total process peak is
measured separately.1163 nearest and2341 shadow rays use the explicit fallback.
The renderer caps33,554,432 paths,16samples/pixel and65536-ray packets; the
measured packet is8192. World frame/GPU/A18/RTX budgets are not admitted.

At320×180/4samples, legacy99c2ca600 takes19.16s wall/19.15s CPU/218.18MiB;
cached-bounds00861e8ec takes14.31/13.97s/197.97MiB. All six numerical arrays and
four output images are identical. Loads differ and this is a local trend, not
an isolated speedup gate. The measured128×72 profile attributed2.59 of4.35s in
the render loop to13 legacy whole-geometry Float64 fallback projections. The
retained `--shadow-backend legacy` supports fair follow-ups; cached bounds avoid
rebuilding those triangle/projection inputs, preserving the final visibility test.

## Reproduction and open admission

Existing `.venv` NumPy/SciPy/Shapely/embreex/Pillow dependencies. Explicit fresh
system-temp outputs, no image publication or showcase server:

```sh
PYTHONPATH=lib/src:lib/tests:test/experiments/vegetation .venv/bin/python -m unittest diffuse_bounce_checks
PYTHONPATH=lib/src:test/experiments/vegetation .venv/bin/python test/experiments/vegetation/scatter_conformance.py --inputs <saved463-products> --output <temp-json>
PYTHONPATH=lib/src:test/experiments/vegetation .venv/bin/python test/experiments/vegetation/scatter_render.py --inputs <saved463-products> --output <fresh-temp> --width 1920 --height 1080 --samples 4
```

One-sample diagnostics explicitly retain unknown per-pixel error in NPZ and emit
JSON null, not a fabricated zero or nonstandard JSON NaN.9 focused numerical
checks run with source and the previously installed library. No library/native
API changes or new-wheel claim. The method remains an experiment; a verified
world-scaling product, full multiple scattering, continuous sky, specular detail,
procedural surface variation, botanical/world coverage, wind, temporal/LOD and
native device acceptance remain open at #23/#451 and their feature owners.
