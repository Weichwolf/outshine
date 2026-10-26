# GGX fragment precision

Owner: [#841](https://github.com/Weichwolf/outshine/issues/841).
Shared shader correction: [#829](https://github.com/Weichwolf/outshine/issues/829), PR #837.

`ggx_fragment_precision.py` reuses `ggx_stability`'s exact 3672 payloads and independent
angular-NDF/Smith-lambda reference. The fragment adapter changes execution/transport,
not the production GGX functions or reference. Two RGBA32F attachments retain all eight
values per case. No depth, blending, tone mapping, material atlas or temporal filter.

Build the registered diagnostic target with the normal project configuration:

```sh
cmake --build "$build" --target outshine_ggx_fragment_probe --parallel
MTL_DEBUG_LAYER=1 GLSLANG="$glslang" python3 test/experiments/render/ggx_fragment_precision.py \
  --binary "$build/outshine_ggx_fragment_probe" --output "$TMPDIR/outshine-ggx-fragment"
```

`build` names the configured CMake build and `glslang` its selected GLSL compiler.
Set `MTL_DEBUG_LAYER=1` on Metal because the probe explicitly requests API validation;
other backends use their normal validation setup. The target belongs to no default test gate.
Record C++ and shader commits separately. The output directory must be system temporary
storage. Optional `--baseline` selects an older shader directory. Source/SPIR-V/raw
records and comparisons remain available; numeric errors return nonzero. This diagnostic
does not admit a renderer, and an expected failing run is not reported as a passing test.

Initial A18/Metal findings: shared GGX fix passes compute and ordinary material raster,
but Relaxed fragment math still loses extreme normal-input results. The original grazing
mirror counterexample is fixed. SPIRV-Cross invariant math alone does not fix the rest.
Safe fragment math removes normal-input errors; subnormal losses remain separate.
Global Safe is rejected as a product change: paired moving-world GPU-p50 rises
10.162→14.614ms, p99 24.014→28.225ms, with unchanged inputs and incomplete moving worlds.
A bounded shader-precision policy remains open. Detailed receipts belong to #841.

Current-master replay (A18 Pro/Metal, 2026-10-26): identical3672-case palettes and
SPIR-V produce2128normal-input errors/1008nonfinite results in production Relaxed,
versus0/0in an isolated Safe control. Native API validation passes in both runs.
Denormal accuracy losses remain1024/800and are not converted into passing parity.
The control replaces only the private native archive's MetalShader object; product
sources and precision policy stay unchanged. No new performance claim follows.
