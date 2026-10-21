# Lossless position words without dependent tables

Research [#1062](https://github.com/Weichwolf/outshine/issues/1062), supporting
[#1045](https://github.com/Weichwolf/outshine/issues/1045). This small comparison
reuses [the six original field controls](vertex_field_words.md). It changes only
the representation of Float32 position **bits**; there is no float quantization,
new colour coding, geometry reduction or native format/shader implementation.

## Representation

For each axis, choose the smallest unsigned uint32 position word as base. Subtract
that integer base from each word and determine the maximum difference's bit length.
If the three lengths sum to at most64, pack them consecutively into two uint32
words per vertex. Decode with shifts, masks, OR, three base additions and bitcasts.
Cross-word extraction uses uint32 only, without GPU uint64 or dependent table loads.
The three bases/widths are uniform per cell. Numeric float ordering is irrelevant;
signed zero and finite subnormal/negative words remain bit-identical.

An experimental 40 B header carries magic, vertex count, original core size, three
bases and three widths. Common native stream metadata supplies bounded vertex spans
(offset/count/stride); it is unchanged and excluded equally from both byte totals.
Replace only the first12 position bytes of each original24 B interleaved record
with8 B. Retain UV/normal bytes, stream/chunk order, index width/order, material
metres, RGBA and every gap/tail byte. The tinted fixture's position stream is SoA,
12→8 B; its remaining streams are unchanged. Source bindings remain separate.

Over64 bits, empty bodies or no size gain retain Raw. A following compressed-size
control also retains Raw if the encoded Zstd1 core is no smaller. These are byte
eligibility decisions, not GPU admission. Near-cell Full/Fine each require72 bits;
no smaller chunk, exception table, reanchoring or approximation is introduced.
The scalar oracle interprets the original packet layout independently using Python
integers. Both decoders must recover the **entire native core byte-for-byte**.

## Measured result

Producer `79a75c367`, Linux/shared host, Python3.13.5, NumPy2.5.4, libzstd1.5.7;
three fresh normal processes plus one separate traced process, one BLAS thread.
Other host work was active; measurements are Python/ctypes costs, not device times.

| Input | Bits X/Y/Z | Native core B | Packed core B | Raw Zstd1 B | Selected Zstd1 B |
|---|---|---:|---:|---:|---:|
| dense-shell | 21/24/19 | 943302 | 828654 | 346662 | 326372 |
| native-cell-Full | 20/20/19 | 175322 | 149770 | 60500 | 56653 |
| native-cell-Fine | 20/20/19 | 1017158 | 872638 | 308172 | 279330 |
| near-cell-Full | 24/24/24 | 788636 | Raw | 304767 | 304767 |
| near-cell-Fine | 24/24/24 | 1123752 | Raw | 415157 | 415157 |
| native-tinted | 21/21/22 | 157386 | 156482 | 2481 | 2221 |

The dense core saves12.15% uncompressed and5.85% after Zstd1. Remote Fine saves9.36%
compressed. The tinted fixture is small and dominated by its original degenerate
topology slots; its10.48% wire saving does not represent a dense geometry load.
Native-policy Zstd1 uses level1/minimum64 B/Raw fallback. As in #1057, these are
isolated original cores, excluding shared asset metadata and actual package grouping;
no actual DB/SSD transfer improvement follows yet. Header alignment affects Zstd:
the24 B preflight header is not the final40 B measured packet.

| Input | Build ms (three runs) | CPU expand ms (three runs) | Build/expand traced peak B |
|---|---|---|---:|
| dense-shell | 2.336/2.319/2.789 | 1.058/1.075/1.586 | 2495154/2460979 |
| native-cell-Full | 8.459/8.895/9.397 | 11.075/11.563/10.924 | 846252/479619 |
| native-cell-Fine | 4.944/4.635/4.589 | 3.129/3.300/3.467 | 2673595/2758075 |
| native-tinted | 0.188/0.249/0.192 | 0.117/0.126/0.119 | 321940/469218 |

The Full control has many small original spans; Python per-span overhead matters.
Input decoding/archive validation/native-core serialization is timed separately.
Dense Zstd decode is1.369/1.393/1.353 ms for Raw versus1.203/1.214/1.211 ms for packed,
**before** the extra full-core expansion. This CPU path is slower overall and keeps
the complete Raw output. It is not the desired product loading path.

Known serialization copies are twice the packet bytes (dense1,657,308 B); expanding
the full core writes/copies at least twice its original size (1,886,604 B). Position
extraction additionally allocates12 B/vertex, packed words8 B/vertex, and per-span
record buffers. The output does not retain the input packet, but both coexist during
decode. Traced peaks are stage-local Python allocations; whole-process RSS includes
original JSON, validation and both oracles. Total allocation events/copy traffic,
driver memory and application residency are not fully instrumented.

The worthwhile next hypothesis is **direct** packed GPU consumption: interleaved
vertex bytes24→20, with local integer extraction and no table gathers. No consumer
or native upload exists here. Agree Metal/Vulkan bindings and measure actual device
decode/vertex cost, upload/residency and frame/p99 before a product choice. Retain
Raw until such a comparison supports the total pipeline; smaller storage alone is
not a recommendation to adopt this codec.

## Correctness and visual scope

Six new tests cover all six original inputs, every critical cross-word width,
32-bit fields, zero-width fields, signed zero/subnormals, negative positions,
overwide Raw fallback, malformed extents, padding and overflow. Nine existing field
tests also pass after adding span metadata; original core bytes remain unchanged.
All four eligible cores/corner fields match both decoders in all four cost runs.
No source, index, normal, material, HDR/alpha word is changed. Real captured vertex
RGBA is white; arbitrary unchanged gap/tail bytes are not a native HDR capture.

A fresh saved-core process decodes the dense packet and parses its original vertex,
index and material streams, using existing common source bindings. Original fixture
opens are forbidden. All1,920 saved native static-020 rays yield identical hits and
material fields. The1280×720 view's48×10 ROI/four samples per pixel was rendered
under two diagnostic Lambert lights and actually opened. It is an enlarged opaque
CPU diagnostic, not native PBR, movement reserve or device qualification.

The discarded16-bit positional grid is retained as a counterexample: dense maximum
vertex displacement5.799 mm nevertheless collapses two original triangles and
flips one. A small metric error does not establish valid geometry. No repair sweep
or exception representation is part of this experiment.

## Reproduce

Use the repository `.venv` with NumPy/Shapely/Trimesh and system libzstd. Set
`PYTHONPATH=lib/src:test/experiments/render`, `OPENBLAS_NUM_THREADS=1` and
`OMP_NUM_THREADS=1` from the repository root:

```sh
.venv/bin/python -m unittest discover -s test/experiments/render -p test_packed_position_words.py
.venv/bin/python -m unittest discover -s test/experiments/render -p test_vertex_field_words.py
.venv/bin/python test/experiments/render/compare_packed_position_words.py --output /tmp/outshine-position-words
```

Use fresh explicit system-temp outputs, three normal processes, then a separate
`--trace` run. Freeze experiment sources before measuring. Local receipts, raw
reports and image/replay scripts are linked from the PR; no repository images.

SIGGRAPH starting point: [Deering1995, Geometry Compression](../../../doc/references/transport/siggraph/1995-geometry-compression.pdf),
§4/page3 visually inspected. It motivates object-relative position representation;
this reference uses exact Float32 **word** differences, not the paper's quantized
numeric-coordinate, strip, normal, colour or Huffman codec.
