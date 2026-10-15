# Native ground-mask candidates

[#196](https://github.com/Weichwolf/outshine/issues/196) owns the filter choice.
[PR #242](https://github.com/Weichwolf/outshine/pull/242) supplies the spatial and
motion reference comparisons. The native candidates now use the same oriented
sample locations, weights and derivative-length fade in the actual material
composition path, as well as in the scalar component probe.

The internal translation definition `GROUND_LITTER_GAUSS_POINTS` selects the
existing box at zero (the default), four samples at two, or nine at three.
Other values are compilation errors. It is a temporary comparison mechanism,
not a new engine setting or scenario option. Native device cost and identical
Place pairs determine the final production filter; the alternatives are not
three permanent runtime paths.

Both oriented variants return the declared mean at two cells, use the existing
point approximation below 0.01 cells, and smoothly fade between one and two.
They consume the original two world derivatives rather than their bounding
rectangle. Four/nine samples use at most 16/36 corner hashes, versus at most 16
for the box. This count does not measure shader execution time or registers.
No material parameters, palette bytes, bindings, textures or passes are added.

`ground_litter_shaders.py` compiles the actual component probe and every ground
output variant declared by Make, for all three candidates. It translates each
SPIR-V module to MSL 2.3, compares reflected resource layouts and translated
resource-type/index maps with the box variant, checks the 64x1x1 compute shape,
proves default/explicit-box SPIR-V equality for the component probe and rejects
an invalid definition.
SPIRV-Cross translates SPIR-V directly. The checker compares buffer access,
set/binding, member offsets and array strides, and verifies the compute input/
output stride32 and uniform size16 against the native consumer. Translation
and layout checks do not prove native Metal compilation or execution.

Prepare the shared generated BRDF table with the repository toolchain, then run
with glslangValidator and the SPIRV-Cross CLI:

```sh
make build/shaders/brdfTables.glsl
python3 test/experiments/render/ground_litter_shaders.py --output /tmp/outshine-material-oriented
```

`GLSLANG` and `SPIRVCROSS` identify the installed tools. On Debian, the required
CLI package is `spirv-cross`; the already used `spirv-cross-c-shared` development
library alone does not provide that executable. The experiment
requires explicit system-temp output and records source/tool hashes, reflected
metadata and generated SPIR-V/MSL. Generated BRDF tables stay under `build/`.
With `--native-syntax`, it also checks the native consumer using `CXX` and the
standard/warning/optimization declarations from `test/run.sh`. The default
include root is `src/render/device`; `NATIVE_INCLUDE` can select a checkout of
the published shared contract during transition. Header hashes are recorded.
This option neither links a backend nor substitutes for a native execution.
The next native check uses the common Metal/Vulkan compute operations and the
existing 32-byte probe/result records; it does not require another backend API.
Native mask/complete-surface correctness, timestamped GPU costs and lit Places
remain required before selecting a candidate. The unchanged box remains the
production default until those measurements justify the change.

`test/harness/render/NativeGroundLitterProbe.cpp` consumes the shared native
contract published by Performance at `b4b361429`, without SDL or native API types.
It accepts the existing raw palette/probe fixtures, uploads once, dispatches the
actual component shader, copies results to readback and waits with an explicit
30-second diagnostic bound. The pipeline's reflected set/slot/access values,
local size and 16-byte uniform layout are checked before recording. Probe and
result records retain their checked 32-byte size/alignment/offsets. Palette
prefix/layer ranges are checked before GPU access.

```text
native_ground_litter shader.spv palette.f32 probes.bin result.f32 unmapped
```

`unmapped` is the palette producer's explicit fallback row. Five native buffers
request a total of `2*palette_bytes + 4*probe_bytes`; three copies transfer
`palette_bytes + 2*probe_bytes`. These are logical requested/copy sizes, excluding
driver allocation granularity, transient descriptors and CPU inputs. The probe
does not call wall time GPU time. It links against the common backend from #243/#240. Source, compile and native
execution must identify the corresponding backend commit; syntax checking alone
is not a backend execution result.
