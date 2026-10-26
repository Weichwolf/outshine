# Native material comparison caller

This caller runs the component cases of #196 using the common native consumer
from [PR #246](https://github.com/Weichwolf/outshine/pull/246). It imports the
existing probe generation and complete-surface validator; it does not create
another GPU abstraction or use SDL GPU operations.

The input is a raw enriched palette exported by the real native producer. The
current producer appends the unmapped row last. All three candidates receive the
same 115,200 probes, including 2,304 scalar controls, and the same palette values.
The legacy control clears only the layer offset; the neutral control clears
only the two height amplitudes, preserving all optical parameters and scales.

A small shared input is pinned at
`test/outshine/data/materials/ground-litter.palette`. Its neighbouring JSON records
the exact producer commit, both recipe hashes, encoding, fallback colour and
payload hash. It contains the real producer's 319 floats (1,276 bytes), including
the layer offset at float 199 and unmapped row 13. It is component input, with no
expected output, Place geometry or world-cache contents. Normal runs never
rewrite it. Changing this input requires an explicit fixture migration; it is
not a claim that the pinned recipe equals the latest live world material state.

```sh
python3 test/experiments/render/ground_litter_native.py --palette palette.f32 --output /tmp/outshine-material-native-inputs --prepare-only
python3 test/experiments/render/ground_litter_native.py --palette palette.f32 --output /tmp/outshine-material-native-results --binary /path/to/native_ground_litter
```

Use `--palette test/outshine/data/materials/ground-litter.palette` for the shared
cross-device comparison. The fixture supplies the same input on each device without an external producer
or another agent's private directory. The producer
call was `GroundMaterialPalette(templates, {{.11f, .22f, .33f, .9f}})` after loading
the two shipped catalogs at the recorded commit. Native-generated probe outputs
remain in system temp and are checked against the mathematical references.

`GLSLANG` identifies the existing glslangValidator tool. NumPy is required; the mathematical reference module has no GPU dependency. Shader compilation runs at nice10;
actual native execution uses normal priority. Outputs must be in system temp.

Preparation emits the exact probe bytes, three palette variants, the three
compiled SPIR-V candidates and CPU field references. `preparation.json` describes
that phase only and explicitly records `gpu_executed: false`. Preparing inputs
is not GPU validation. A native comparison requires the actual linked consumer;
there is no fallback to software, a mock backend or archived result files.

The explicit box comparison uses the existing independent 64x64 midpoint reference. Oriented
variants use NumPy's two-/three-node Legendre quadrature as a numerical parity
reference, including the declared near-point approximation and far fade. This
checks the implementation of the selected candidate. The independent pixel-area
quality comparisons remain in PR #242; agreement with the quadrature itself does
not prove exact physical pixel integration.

For each candidate, the native, legacy and neutral palettes are captured in
separate real consumer runs. Full-surface colour/roughness/gradient checks reuse
the existing validator and its unchanged tolerances. No-litter surfaces must
remain exactly equal to legacy, the far field retains its existing tolerance,
and the old constant composition must fail the same detailed reference.
Only after every capture and validation succeeds is `comparison.json` written
with `gpu_executed: true`. Starting another attempt removes that success marker;
failed launches and incomplete results cannot retain an earlier pass. Detailed
per-launch logs preserve the actual error.

These are component parameters. The default correctness captures make no frame-rate, GPU-time, complete
Metal/Vulkan renderer or Place-image claim from either input preparation or the
component checks. The caller needs the published shared backend implementation
to execute; the consumer/caller are integrated through #268. Production now selects nine
points by default; explicit box/four definitions remain compile-time comparisons.

The consumer's optional `--gpu-timing` mode repeats the dispatch 32 times. When
available, `profile_batch_gpu_ms` measures the completed command buffer, including
uploads, all dispatches and readback copy. It is not a pure shader time or a frame
budget result. Per-dispatch timestamps remain separate in `profile_sum_ms`;
unsupported measurements explicitly report unavailable. The consumer retains GPU
validation, so Metal requires `MTL_DEBUG_LAYER=1`. Compare identical inputs and
validation state; divide neither batch time nor phase sums into claimed frame costs.
