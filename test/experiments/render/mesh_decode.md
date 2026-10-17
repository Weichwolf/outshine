# Native mesh decoding without duplicate buffers

Owner: [#565](https://github.com/Weichwolf/outshine/issues/565), parent [#15](https://github.com/Weichwolf/outshine/issues/15).

The asset decoder previously read attributes into reusable scratch vectors and
then copied them into Geometry. It now reads directly into seven owned mesh
streams. The public `Geometry::adoptMesh(part, mesh)` validates before transferring
ownership. Rejection preserves both inputs and destination. Existing incremental
span setters and publication validation remain available; binary assets, shader
layouts and cache versions are unchanged.

A controlled 131,072-vertex fixture counts allocations of at least 512 KiB only
during decoding. Six large attribute streams require 9,437,184 bytes (9 MiB).
Before: eight requests totaling 14,155,776 bytes (13.5 MiB). After: six requests
and exactly 9 MiB. The difference is 4,718,592 bytes = 4.5 MiB, or one third of
these allocation requests. This counts requested storage, not simultaneous live
bytes, RSS or process footprint. Re-encoding retains every byte.

`BinaryValueReader` also constructs little-endian numeric vectors from a sized
standard-library range instead of resizing, zeroing and overwriting them.
Unaligned input is read with memcpy. Opaque records retain the previous bulk-copy
path, including padding. Fresh/reused destinations, empty/truncated input,
integer boundaries, signed zero, denormals and NaN payloads are covered.

The three regression sources are `MeshOwnershipTransfersAtomically`,
`DecodedStreamsNeedOnlyTheirOwnedAllocations` and
`ArraysPreserveUnalignedBitsAndBounds`. Fifteen affected Geometry/codec tests
pass; the two tests receiving final diagnostic-only edits pass again. Focused
analysis of changed production/test units is clean; public documentation reports
52/52 headers and no diagnostics. Frozen API integration lint on `ccb37550d`
passes: 430/430 C++ units, 29/29 repository checks, 2,051 formatted files,
491 shader artifacts and 9 compute contracts. The first isolated attempt lacked
the provisioned shader SDK and built archive; its failure is retained separately.
The complete retry uses the same source and explicitly supplies those dependencies.
Later adaptive-cell integration and general world/device gates remain separate.

## Whole-process comparison

Apple A18 Pro, macOS, integrated SDL_GPU backend, 1280×720/30. Fresh offline
processes at normal priority, no concurrent owned heavy work. Actual sequence:
before / after / after / before, Wien then Koerbersee each time. Every process
loads the complete prepared world and records exactly 30 frames / 360 degrees;
all eight PNGs match their preceding integrated images byte for byte.

| Metric | Before, two runs | After, two runs |
|---|---:|---:|
| Wien loading, ms | 1528.665 / 1513.532 | 1530.788 / 1505.520 |
| Wien peak footprint, bytes | 3,268,856,208 / 3,212,233,224 | 3,201,337,864 / 3,238,644,112 |
| Koerbersee loading, ms | 1036.870 / 1077.221 | 1090.319 / 1076.895 |
| Koerbersee peak footprint, bytes | 1,630,258,808 / 1,642,776,184 | 1,633,207,952 / 1,639,761,552 |

Wien's mean loading difference is 2.944 ms and footprint difference
20,553,728 bytes; both are smaller than the individual-run spread. Koerbersee's
mean loading increases 26.562 ms while footprint changes only 32,744 bytes.
These observations establish no whole-process speed or memory improvement.
The controlled allocation reduction remains the supported result. SDL_GPU does
not expose valid direct GPU timestamps here; no GPU budget or speed claim.

Full inputs, commands, binary hashes, process counters and logs are retained in
system-temp `outshine-decoded-mesh-ce3d84a8c`. Baseline binary SHA-256:
`d73987d04a9d6694fc673adcc04e6a9ae145f6c0bf86f8485a855cf930dad2aa`.
The reference baseline is master/ce3d84a8c; implementation commits are
`d35974465`, `6b540a4e0` and `016a89371`.
