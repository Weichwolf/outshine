# Receiver rectangle unions

Compare the boolean union of inclusive integer receiver rectangles before its
first consumer reads it. The existing receiver-footprint hypothesis only writes
zero depth or leaves infinity; it needs occupancy, not a general depth field.
This experiment changes no renderer, world demand, asset loading or GPU contract.

Four native alternatives share one explicit rectangle trace and identical output:

- Float minimum over every covered cell, the original receiver representation.
- Byte rows filled with one, avoiding float depth storage.
- Rows of 64-bit words updated with exact rectangle masks.
- A signed 64-bit 2D difference array: four corner updates per rectangle, then
  one horizontal/vertical prefix scan before any occupancy query.

All input coordinates are validated once. Side length is in [1,1024], with at most
16,777,216 rectangles. Consequently difference counters and their intermediate
sums are far inside int64 range. Each grid has one mark phase, exactly one
`Finish`, then read-only queries; marking after finalization is outside the
experiment contract. Row words and difference sentinels are internal CPU storage.

```sh
python3 test/experiments/streaming/receiver_coverage/compare.py \
  --trace /tmp/receiver-rectangles.bin --side 256 --output /tmp/python.json
python3 test/experiments/streaming/receiver_coverage/run.py \
  --compile-database /tmp/native/compile_commands.json \
  --trace /tmp/receiver-rectangles.bin --side 256 --output /tmp/native-union
python3 test/experiments/streaming/receiver_coverage/verify.py \
  --binary /tmp/native-union/probe --output /tmp/union-checks
```

The trace consists of little-endian uint32 `[left,right,top,bottom]` records; it
has no place names or fixed cameras. The native build uses the complete actual
`PreparedRoadManifests.cpp` flags from the supplied compilation database and
links only the selected C++ runtime. It does not build or link the engine archive.
The output directory must be empty and under the system temporary directory.

The Python comparison uses an independent per-cell oracle and reports its own
limited trace prefix. Python timings are not native times. The native comparison
rotates four methods over seven rounds, compares full masks and records allocation,
mark, finalization and snapshot times separately. State/snapshot payload bytes and
logical cell/word/corner updates are explicit; they are not process memory peaks
or measured memory traffic. Builds use nice 10; native timing processes retain
normal priority and record host load in the receipt.

The independent fixtures cover singleton/full/sparse shapes, overlap, repetition,
word boundaries 63/64, partial final words and side 1024. Normal tests neither
write an oracle nor bless a captured result. All variants retain
`qualified_for_page_suppression=false`: equal union masks do not qualify receiver
geometry, raster precision, unseen consumers or world-generation lifetimes.
A faster union kernel alone does not prove faster world loading.
