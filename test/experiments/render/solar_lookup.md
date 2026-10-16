# Solar-table boundaries and interpolation (#286)

The old 256×64 table samples strictly inside its physical domain. Ground, zenith
and atmospheric top therefore clamp to interior samples. The unchanged dense
clear-prefix control fails its 1% light target at 2.038785%. This follows the
separate fixed-budget integration correction in #307, not a change to its oracles.

The selected 512×64 table includes the physical endpoints. Shared CPU/GLSL
`mediumTransmittanceTexel/Sample` map texel centres to/from that domain. Every
consumer and the CPU exporters migrate together. The unoccluded tangent column
integrates to atmospheric top: a numerical ground intersection must not truncate
this boundary value. Planet visibility remains separate. Ordinary
`MediumTransmittance` still stops at solid ground; the solar producer explicitly
uses `MediumTransmittanceToTop`. Medium80, RGBA16F and shader slots are unchanged.

## Actual native comparisons

Code freeze: d4f183620. Linux/RTX 3060, immutable Vulkan API/archive producer
5ea6f823b; all 1,785 manifest files verified. The additional consumer uses its
matching native headers. No mixed virtual API or foreign archive modification.

| Unchanged prefix control | Old max RGB light error | Selected |
|---|---:|---:|
| Dense clear Place rays | 2.038785% | 0.980528% |
| Zenith Haze0 | 0.503125% | 0.094813% |
| Haze0.25 | 0.591961% | 0.053790% |
| Haze1 | 0.655630% | 0.057660% |
| Haze4 | 0.992225% | 0.110413% |

Original references, repeated queries and source-position bounds are retained.
The source fitting and multiple scattering have not thereby become general
physical references. The dense direct-to-top 320/640-step diagnostic passes at
0.824041%/0.824505%, isolating remaining table error from the finite integrator.

`export_solar_lookup_probes.py` emits 7,638 quantized physical looks per haze,
including 43 planet-blocked controls and seeded off-grid points. Independent
zonal FP64 optical integration must refine from 32 to 64 nodes within absolute
T=1e-7. `solar_lookup_probe.comp` uses the existing six-binding native prefix
consumer; zero query/multiple-scatter inputs preserve its binding control.
`check_solar_lookup.py` measures actual generation → RGBA16F → GPU lookup.
Maximum absolute T error falls from 0.015041 to 0.004739 for Haze0/0.25/1/4;
all blocked controls are exactly zero. This is no new global relative-T threshold.

Eight actual solar-disc raster cases at 1280×720 pass 141 checks before and after.
Each has 435 independent physical interior-pixel probes, with optical refinement
below 1e-7 T. The 600-km atmosphere crossing improves max absolute T error from
0.013508 to 0.001022; ground-low improves 0.000631 to 0.000256. All non-solar
background pixels and velocity products remain exact; this is a controlled
SkyView input, not a full Place image. The three-page PDF is optically inspected:
`build/shots/reference/solar-lookup-d4f183620/comparison.pdf` in the main checkout.

The complete public offline Körbersee client comparison at 1280×720/60 preserves
124/124 ground and 49/49 vector tiles, 22,709 triangles and zero bare tiles.
Before/after/after-repeat load in 9.987/8.267/9.830 s; p99 is
9.4708/5.4605/14.3565 ms, no overbudget frame in each 60-frame/360° run.
The after-repeat PNG is byte-identical. The existing production client still
uses SDL_GPU pending #328; these are not native-backend frame times.
`place-comparison.pdf` beside the first PDF includes the actual PNGs, difference,
and dated 2026-09-07 12:40 local webcam; all four pages optically checked.
The hosts also differ in their #400 generator base, so Place pixels alone are
not an isolated solar causal proof. The scene remains visibly far from the photo;
no general quality, 1920×1080/60, Metal or A18 acceptance follows.

## Alternatives and cost

At equal 128-KiB storage, full endpoints reach 1.1576%, angular endpoints alone
1.1296%, quadratic angular concentration 1.0466%, and 512×32 reaches 1.7384%.
They fail the unchanged dense light target. Full endpoints with the old ground
termination fail at 390.9%; always-Gauss40 does not repair the fitting/lookup error.
These measured counterexamples are retained. No clamp or changed expectation
conceals a failed case; no claim of global optimality.

The selected logical product grows 128→256 KiB (+131,072 bytes). Generation
doubles 16,384→32,768 texels, still 40 extinction evaluations each, plus the
existing conditional aerosol estimate. No new pass, sampler or per-query copy.
Eight interleaved fresh-process samples per state under concurrent host work
measure LUT dispatch median 0.010304→0.015360 ms, maximum 0.010784→0.015808 ms.
GPU utilization/clocks were unavailable. These are real dispatch timestamps,
not fence waits or frame/p99 times; whole-device peaks and Metal/A18 remain open.

Reproduce input export with `--medium <Medium80.bin> --output <temp-dir>`;
run the existing `air_prefix_gpu.cpp` consumer with the lookup shader and generated
rays/queries/medium, a zero Psi texture, and the production solar compute shader.
Then use `check_solar_lookup.py --fixtures <temp-dir> --samples <samples.bin>
--output <report.json>`. Full commands, producer/input hashes, captures and
terminal statuses: `/tmp/outshine-light-next-563e696dc/`, especially `native/`,
`production/`, `lookup/`, `timing/` and `sky/`. No normal test writes oracle pins.

Basis: Hillaire, EGSR2020 §5.2, local paper page4 optically checked. Physical
endpoints and this measured resolution choice are our implementation decisions.
Full Places, temporal transitions and device budgets remain #286/#206;
finite-air/Thin/cloud composition remains #266/#193, and #259 remains a draft.

Verification: six canonical CPU cases, six affected stage/path/ground cases
(including the declared sanitized projection replay), regular 450-object archive,
230-member generator archive and public client pass. Seven production shaders
compile to SPIR-V/MSL2.3 with resolved uniform/resource layouts unchanged.
Focused tidy passes all 39 dependent production units and the new tests/exporter
without own diagnostics. The migrated existing aerial test keeps 17 baseline
diagnostics; its expectations are unchanged. Later edits only name test constants,
use explicit test initializers and document evidence; production remains d4f183620.
