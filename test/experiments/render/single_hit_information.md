# What a first-hit field does not contain

Owner #1842, parent #1797. `compare_single_hit_information.py` consumes the whole
native #1838 `building-world-opponents/ss2` packet. It diagnoses information;
it does not admit a model, replace geometry or add a runtime format.

The existing SIGGRAPH 1998 *Layered Depth Images*, PDF pages 2–3, distinguishes
sprites with depth from layers that also represent hidden surfaces. Its transfer
equations were re-opened optically. SIGGRAPH 2000 *Relief Texture Mapping*, PDF
page 3, supplies displacement pre-warp equations and the Figure 3 pipeline; it
does not recover information absent from the stored displacement surface.
Both PDFs are already under `doc/references/`.

## Finite native observation

The two poses have 204/201 actual native original hits at the shared 2560×1440
sample raster. Lift their actual Float32 raw depth using the executed World
matrix, then project with the executed Capture matrix. 37/38 points fall in empty
128² Capture cells. All remain in those same cells under the explicitly recorded
Float32 predecessor/successor depth and ±1e-6 World-pixel perturbation probe.
This probe is sensitivity evidence, **not a certified GPU arithmetic bound**.

All 37/38 are visible against terrain; 22/21 remain visible through the external
opening. Their fractions within an output 1280×720 pixel reach 1/0.75. Those
fractions are not image-error lower bounds: a different represented cell can
still spuriously cover the novel ray. A depth difference between neighboring
Capture samples cannot identify a hidden triangle. The audit deliberately does
not label the real points as proven disocclusions.

The unchanged first-hit-cell candidate fails all four filtered native opponents:
RGB maxima 4.73645/5.02441 and visible-source coverage maxima 1/0.75 against the
existing 0.25/0.125 research limits. Opaque final union alpha is unchanged even
with these errors. Every whole-original group fallback remains exact in filtered
colour/coverage and unfiltered sample depth. Eight local panels were opened.

## A genuine information lower bound

Scene A contains an opaque rectangle `[-.5,.5]^2` at z=2. Scene B additionally
contains the same footprint at z=1. Looking along decreasing z, their **entire
continuous** first-hit Capture colour and depth are identical: the front covers
the whole rear footprint. Fixed front/rear radiances are 0.2/1; no native PBR
equivalence is claimed.

For novel rays `x=u+z, y=v`, choose four common pixel samples at u=-1±.1,
v=±.1. All miss the front and hit the rear if present. One source pixel's
coverage and RGB differ by 1 between A/B. Every deterministic reconstruction
using only their identical first-hit operands returns the same answer for both,
hence has worst-case error at least 0.5 in one scene by triangle inequality.
Randomization supplies no guaranteed correct answer for both either. Opaque
backgrounds can keep both union alphas 1 while source coverage differs.

This is an analytic counterexample to general first-hit-only qualification, not
a claimed 0.5 lower bound on the specific native packet. Higher Capture resolution
or better interpolation cannot reveal that hidden rear surface. Adding a known
rear layer supplies information that the identical first-hit operands lack.

## Reproduction and cost scope

```sh
export PYTHONPATH=lib/src:test/experiments/render
export OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1
python -B -m unittest test_single_hit_information test_native_world_inputs
python -B test/experiments/render/compare_single_hit_information.py --output "$fresh_temp"
```

The audit evaluates 1836/1809 projected points including the eight perturbations;
no original triangle body is rebuilt. Its single-run preparation CPU times are
diagnostics, not frame/solver/alternative-renderer comparisons. Stored decoded
Capture payload is 327680 B per pose (RGBA Float32 plus Float32 raw depth).
Naively adding a full equal-size second layer would double this to 655360 B before
indices, material data and allocator overhead; its occupancy/error is unmeasured.
The whole-original reader separately validates 4850833 B of input operands and
joins 940488 B positions plus 923424 B Int64 indices. These CPU arrays are not native
residency, whole-scene peak or a shared-memory/device budget.

The existing `compare_native_world_fields.py --sample-axis 2 --cost` provides
the same-quality full-crop versus metadata-region comparison at these exact
samples. Both evaluate the same inadequate cell model. Timing excludes loading,
source verification, imports and this information audit; it cannot qualify the
model or serve as a new representation comparison.

The representation decision and next bounded contract live in #1842. First-hit
sprites may still be useful within a separately qualified pose/material envelope;
this result rejects a general envelope inferred from first-hit operands alone.
