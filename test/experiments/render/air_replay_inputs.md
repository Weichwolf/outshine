# Fixed air replay solar input (#663)

The fixed dense `veil.bin` still contained the pre-#439 768×64 interior-domain
solar table. The current shared sampler expects physical endpoints mapped to
texel centres; the production profile became512×64 in #439. Pairing current
code with that stale input produced a4.249% dense RTE failure. Both original
Weather layouts retained the same error; the112 B density-frame migration did
not cause it.

Only the generated solar replay input and its provenance migrate. The current
`mediumTransmittance.comp`, original dense Medium80, a regular verified native
consumer and the512×64 RGBA16F extent produce exactly the earlier #439 capture.
No new atmospheric model, sampler, source fit, shader or binding is introduced.
The other11 fixture files, including all rays/queries/Psi/medium and independent
FP64 reference metadata, remain byte-identical. Normal tests write no inputs or
oracles; the historical prior declaration is retained in provenance.

## Native checks

The same frozen six-binding consumer and20868 queries give:

| Solar input /consumer | Max RGB error | Max absolute T error |
|---|---:|---:|
|Stale /clear|4.249084%|7.497534e-6|
|Current freshly generated /clear|0.980528%|7.497534e-6|
|Current /joint Weather cells|0.982420%|7.497534e-6|

The1% RGB requirement with1e-5 floor, T requirement0.002, original repeated-query
records and63-source bound remain unchanged; at most54 source positions occur.
The old input genuinely fails. Separate accurate-solar-table trials isolated
lookup/integration effects; those alternatives are not installed or passed off
as the production input.

The complete existing `cloud_air_native.py` replay from Weather's clean
cc62080a8af321af6ecc5c77799fbf27c0f22160 completes24 commands as expected, runner0:
clear/broken/overcast/fresh/clear-control, dense repartition, invalid inputs,
sparse schedule and packed-counter negative control. The existing producer/API
binary hash is recorded; no new headers are linked against a foreign archive.
The private one-page PDF contains actual old/current native values and was
optically reviewed. This is no Place/Metal/A18 or general convergence claim.

## Reproduction and resource scope

Compile `mediumTransmittance.comp` and `air_prefix_probe.comp` with the current
source/include roots. Use the existing regular `air_prefix_gpu.cpp` consumer
with dense rays/queries, a512×64 zero RGBA16F extent declaration, original Psi
and Medium80, then `output.bin 0 solar-kernel.spv` to generate/replay. Its
`output.bin.veil` is the generated input. Compare with
`check_air_prefix_gpu.py --fixtures <dense> --samples <output.bin> --output <temp.json>`.
The existing Weather runner accepts `--air-root <this checkout>` and the same
verified `--probe`; no second renderer is needed. Full argv, source/input hashes
and terminal receipts belong to the delivery PR's local evidence link.
`plot_air_replay_inputs.py --input <evidence> --fixtures <dense> --output <temp.pdf>`
reproduces the plot; NumPy/Matplotlib are needed.

The stored payload decreases393,224→262,152 B including8 B dimensions; logical
RGBA16F texels decrease384→256 KiB, matching the existing production choice.
No source-position, dispatch, binding or per-frame budget grows. These file/
logical counts are not process/driver peaks or new GPU-time measurements.
