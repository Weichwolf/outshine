# GGX fragment precision

Owner: [#841](https://github.com/Weichwolf/outshine/issues/841).
Shared shader correction: [#829](https://github.com/Weichwolf/outshine/issues/829), PR #837.

`ggx_fragment_precision.py` reuses `ggx_stability`'s exact 3672 payloads and independent
angular-NDF/Smith-lambda reference. The fragment adapter changes execution/transport,
not the production GGX functions or reference. Two RGBA32F attachments retain all eight
values per case. No depth, blending, tone mapping, material atlas or temporal filter.

Build `test/harness/render/NativeGgxFragmentProbe.cpp` with the complete native-render
compiler/link flags. Record the actual C++ archive and shader commits separately.
Run it via the Python driver, with `GLSLANG`, `--binary`, `--output` in system temporary
storage, and optionally `--baseline` pointing to the previous shader source directory.
The native consumer always requests API validation. Output contains source/SPIR-V/raw
records and comparisons. Errors return nonzero; this diagnostic does not admit a renderer.

Initial A18/Metal findings: shared GGX fix passes compute and ordinary material raster,
but Relaxed fragment math still loses extreme normal-input results. The original grazing
mirror counterexample is fixed. SPIRV-Cross invariant math alone does not fix the rest.
Safe fragment math removes normal-input errors; subnormal losses remain separate.
Global Safe is rejected as a product change: paired moving-world GPU-p50 rises
10.162→14.614ms, p99 24.014→28.225ms, with unchanged inputs and incomplete moving worlds.
A bounded shader-precision policy remains open. Detailed receipts belong to #841.
