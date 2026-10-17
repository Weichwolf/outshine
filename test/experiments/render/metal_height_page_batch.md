# Metal height-page batching countercheck

Owners: [#441](https://github.com/Weichwolf/outshine/issues/441),
[PR #586](https://github.com/Weichwolf/outshine/pull/586).
Apple A18 Pro, 2026-10-17. This is a local native candidate countercheck,
not acceptance of the complete renderer port or the Vulkan input dataset.

## Fixed comparison

Before: `bad383a2e`; after: `55e562337`, a local merge of PR #586's
`8f05e4cba` into that baseline. Both use their own complete SDK and headers.
The same 518 generated shader artifacts are retained; the change has no shader
or public GPU API modification. Both builds terminate successfully.

The production native-cache decoder exports every 35x35 float32 page in source
order from each selected region. Neither input contains skipped sheets:

| Place | Pages | Bytes | Region product key |
|---|---:|---:|---|
| Flensburg | 2620 | 12838000 | `e3fa67a3b80899c9701b694ec38367884099412607f68827215578886fda1a26` |
| Koerbersee | 16255 | 79649500 | `918eb07443ee78446bf6e5385e460faeac4f40d28de8bdf2c6466875ecd60aad` |

Input SHA-256: Flensburg
`d1d081e30ba01657a0ea43163b07b66585e01513a7876ad439da366dcbe8cda1`,
Koerbersee `c8ba01ffb7254c1da737ff998d194006d026414ce68b7970ba32b0d226955301`.
These differ from Performance's 4132/20812-page Linux inputs.

The unchanged C++ production probe from PR #586 is compiled twice. Serial ABBA
processes run without own concurrent build/lint/GPU jobs; desktop remains active.
Every run reads back and bit-compares all 3209500/19912375 actual GPU floats.

| Place | Host completion before, ms | After, ms | Staging allocations before -> after |
|---|---|---|---|
| Flensburg | 144.492 / 141.002 | 54.254 / 54.328 | 2620 -> 55 |
| Koerbersee | 562.329 / 556.399 | 254.138 / 252.715 | 16255 -> 339 |

Allocations follow `ceil(pages / 48)`. Upload bytes and atlas-growth copy bytes
remain identical. This interval includes host recording, submission and waiting;
it is not GPU execution time. Whole-process RSS includes input/setup/readback and
shows no substantial memory saving. No universal Metal/Vulkan factor follows.

The driver needs Darwin's existing native link helper, resolved temporary paths
and a portable timeout. The private counterdriver corrects those execution
issues; the C++ workload is unchanged. Details were sent to the PR owner.

## Runtime and image check

Four fresh 1280x720/30 processes per Flensburg/Koerbersee, ordered ABBA, plus one
before/after pair each for Tokyo, Wien and CentralPark: all 14 processes succeed,
all 30 frames have GPU timings, all turns cover 360 degrees and all five PNGs
remain byte-identical. Koerbersee loading: 990.597/1003.048 -> 738.066/753.685 ms.
Flensburg's first baseline loading is noisy: 1943.468/724.414 -> 700.152/691.827 ms.
Do not infer a stable overall speedup from the first baseline.

Koerbersee's GPU maximum remains 37.03-37.22 ms before and 37.05-37.14 ms after,
above the 33.333-ms target. Residency submission/retry and actual R32 atlas
readback tests pass. The readback case requires `MTL_DEBUG_LAYER=1`; an initial
incorrect invocation without it failed at device opening, not inside the case.

A separate instrumented Metal profile attributes the 37.001-ms first frame's
27.435-ms overlapping stage span to terrain. It submits 4334488 terrain triangle
references; the last frame submits more, 4753824, but totals only 16.277 ms.
Triangle count alone therefore does not explain the first-frame penalty.
Stage durations overlap and must not be added. The profiler adds work and is
not a device gate. Remaining selection/first-use work belongs to
[#515](https://github.com/Weichwolf/outshine/issues/515).

Original inputs, source/SDK/binary hashes, commands, raw ABBA records, validation
logs and inspected PNGs remain in system temp `outshine-metal-height-batch-bad383a2e`.
Product cache data is retained. The native candidate remains separate from master.
