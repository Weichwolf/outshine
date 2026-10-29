# Reusable terrain material fields — #1684

This bounded Python comparison follows the measured A18 material cost in #1257.
It changes no native shader, terrain normal, geometry LOD, class selection, asset
address or cache. Current `groundRock.glsl`, `rockCrevice.glsl` and
`surfaceNoise.glsl` own the rock response. The separate 2D reference retains
`groundSubstrate.glsl` and `groundLayers.glsl` substrate/litter algebra, including
the current nine-point oriented mask and its small-footprint point shortcut.
Rock/class composition and actual native fragment inputs remain separate.

The reference separates each original noise value and physical world gradient
from the current footprint-dependent crevice and frequency-fade computation.
The latter stays at query time. Independent literal shader algebra checks both
slope directions and near/far footprints within 2e-13 in ideal Double arithmetic.
This is not GPU Float32 identity, a normal bake or captured native image evidence.
The existing Python gradient/noise reference remains unchanged.

Two candidate representations use the same current recipe and query response:

- Raw RGBA32 grids store value plus three world-gradient components on an explicit
  diagnostic plane. Bilinear reconstruction introduces measured approximation.
  Ordinary MIPs are deliberately not sampled: a controlled counterexample proves
  that raw averaging and the nonlinear crevice response do not commute.
- Original integer lattices store the exact low 24 hash bits in UInt32 words.
  Eight original corners retain the current cubic interpolation and derivative.
  Analytic hashes and prepared lookups share that reconstruction and must return
  exactly equal full raw fields, material response and diagnostic radiance.
  No interpolation or prefiltering of the integer lattice is introduced.

The latter retains existing footprint filtering; it does not improve its physical
NDF treatment. A bounded box owns at most 262144 words per octave. Preparation
checks its product before allocating, queries require all original corners, and
immutable arrays own their backing. This is an experiment, not a proposed native
texture/codec/binding. No scheduler, persistent cache or global catalogue is added.
Missing boxes do not justify clamping/wrapping or dropping material detail.

`terrain_material_plane.py` holds original 2D words with explicit kernel halos:
one cell for noise/grain neighbours, three for the bounded oriented mask.
Substrate and litter retain the same current footprint tests before any word
query. Whole-far queries, inactive grains and coverage-zero/one layers perform no
unused hash/lookup work. Existing twenty material recipes and fourteen rows of
the versioned captured native palette have independent reference controls; the
palette is not a capture of current Jura/Koerbersee fragments.

The probe uses a real unchanged limestone recipe but explicitly controlled phase,
plane slopes, derivatives, contrast and lighting. Neither plane is a Jura or
Koerbersee source capture. The same existing GGX diagnostic is used on both sides;
its simplified diffuse term and missing sky/shadows/energy table are not a full
world renderer. All output goes to an explicit system-temp directory.

```sh
PYTHONPATH=lib/src:test/experiments/render OPENBLAS_NUM_THREADS=1 \
  OMP_NUM_THREADS=1 .venv/bin/python -B -m unittest test_terrain_material_fields
PYTHONPATH=lib/src:test/experiments/render OPENBLAS_NUM_THREADS=1 \
  OMP_NUM_THREADS=1 nice -n 10 .venv/bin/python -B \
  test/experiments/render/probe_terrain_material_fields.py \
  --output /tmp/outshine-terrain-material-fields
PYTHONPATH=lib/src:test/experiments/render OPENBLAS_NUM_THREADS=1 \
  OMP_NUM_THREADS=1 nice -n 10 .venv/bin/python -B \
  test/experiments/render/probe_terrain_material_substrate.py \
  --output /tmp/outshine-terrain-substrate-fields
```

The composition probe uses the unchanged versioned native palette and explicit
controlled coordinates/footprints. Its test-driver selection uses two known patch
bounds before creating fine raster queries, and prepares only needed
coarse/fine/grain tables and retains the analytic path on a word-capacity miss.
The partial-coverage mask shares the base coarse table with a three-cell halo.
Coverage endpoints skip unused layers; these diagnostic requests are not a native
asset-address or streaming contract. No source is discarded on a capacity miss.

Nominal integer-lattice reads are eight UInt32 words per active octave versus
four RGBA32 texels for a bilinear raw-gradient grid. These are logical addresses,
not physical GPU transactions or cache misses. The shader already skips inactive
octaves. CPU work, preparation, known copies, held data and allocation peaks remain
separate from native sampling/IO/residency and A18 frame times. Hypothetical full
2D mip storage is arithmetic only; the diagnostic does not use those MIPs.

## Sources and remaining contract

[Frequency Domain Normal Map Filtering, SIGGRAPH 2007](../../../doc/references/vegetation/siggraph/2007-frequency-domain-normal-filtering.pdf)
was read and optically inspected, especially figure 1 and the NDF/BRDF argument.
[LEAN Mapping, I3D 2010](../../../doc/references/vegetation/i3d/2010-lean-mapping.pdf)
was read and inspected on pages 3–4, equations 3–6 and figures 6–8. The
[authors' errata](https://userpages.cs.umbc.edu/olano/papers/lean/) apply.
Linearly filtered mean/second slope moments do not establish exact GGX filtering;
LEAN's Beckmann treatment is not substituted for Outshine's GGX response.

The actual bound A18 inputs are now the fixed
`fixtures/terrain-material-fragments/` data from #1684/688eda01c. Their README owns
the instrumented-frame limitations. The loader retains binary32 observations,
signed fallback selections, all palette words and per-point word hashes. In
particular, raw fine normals remain unnormalized. Invalid selections use the
fallback optical row and the distinct unmapped substrate recipe, as the shader does.
Full material reconstruction consumes the observed class/rock weights rather
than inventing a new class selector. Negative weights are outside the current
clamped shader contract and cannot justify the support optimization below.

```sh
PYTHONPATH=lib/src:test/experiments/render OPENBLAS_NUM_THREADS=1 \
  OMP_NUM_THREADS=1 nice -n 10 .venv/bin/python -B \
  test/experiments/render/replay_terrain_material_observations.py \
  --input test/experiments/render/fixtures/terrain-material-fragments/jura.json \
          test/experiments/render/fixtures/terrain-material-fragments/koerbersee.json \
  --output /tmp/outshine-terrain-material-observations
```

One simpler alternative avoids work before requiring additional field storage.
For finite material values and the original nonnegative clamped mixture weights,
the first class has no effect when the other class is used at class weight zero.
Rock is needed only on a class side with positive support:

```text
other_used = edge < no_edge && class_weight < 1
first_used = !other_used || class_weight > 0
rock_needed = (first_used && first_rock > 0) || (other_used && other_rock > 0)
```

No threshold or product of tiny weights defines support. Unused first-class
substrate work is skipped as well. Both exact alternatives retain every one of
the six current Double material/derivative channels on the actual observations.
Singleton integer fields are constructed inside this quality control: cumulative
construction bytes do not estimate world residency or an amortized cache hit.
The warm bounded-plane comparison reports preparation separately.

Double-reference residuals against the instrumented GPU values are observations,
not new acceptance tolerances. Final shading normals are not independently
revalidated because exact scene-position derivatives were not captured. No full
renderer, uninstrumented image, native field address/precision/sampler, IO/residency,
motion, GPU cost or device budget is admitted here. Procedure decision, terminal
evidence and current delivery scope belong in the PR. Existing NDF limitations
remain unchanged; native adoption needs its own ordinary frame/image/cost check.
