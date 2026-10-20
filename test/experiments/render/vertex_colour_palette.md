# Lossless vertex colour indexing

Owner: [#904](https://github.com/Weichwolf/outshine/issues/904), parent
[#16](https://github.com/Weichwolf/outshine/issues/16).

## Decision

Keep the current product representation. A palette substantially reduces the
colour stream, but the full-renderer comparison does not establish a faster frame.
Do not introduce a product format, cache migration or binding from the byte ratio.
A future memory-driven comparison may revisit adaptive constant/raw/palette modes.
This result does not solve world movement (#844) or the shared 720p60 budget.

Native source: `e2ae8098599d142397c72e706499604fa3ffad60` (draft #901).
Normals already use Oct16. They are not a new compression opportunity here.

## Input and representation

Actual final Tokyo opaque indirect ranges reference 1,242,897 distinct colour
slots: 1,242,897 × 16 = 19,886,352 bytes, containing 160 distinct RGBA word tuples.
The complete allocation has 7,020,544 slots / 112,328,704 bytes and 525 tuples.
Referenced slots are not a measurement of GPU memory transfers.
Values reach 1.5384615659713745; UNorm8 would clamp valid HDR data.
UVs also carry categorical facade information and are not quantized by this test.

Each colour is four original UInt32 words, interpreted as Float32 by the shader.
A four-word header precedes per-slot indices, 16-byte padding and a word-exact
palette. The reference header stores palette offset in words, slot count, palette
count and index width. The private shader fixes index width at compilation.
UInt16 packs two indices per word; no Float16 or RGB quantization is used.

| Tokyo complete colour allocation | Bytes | Ratio to raw |
|---|---:|---:|
| RGBA32F | 112,328,704 | 1.000 |
| UInt32 indices + palette/header | 28,090,592 | 0.250 |
| UInt16 indices + palette/header | 14,049,504 | 0.125 |

## Full renderer countercheck

Private copies replace only colour binding/read and prepare the palette before
measurement reset from the live active scene. Original buffers remain resident;
these are not product RAM or cache-hit savings. Upload/readback/packing cost is
outside the measured frames and remains explicit (Tokyo direct UInt16: 46–57 ms).
There are no extra world frames. All original attributes, index order, content,
draws, resolution and 60-frame/360-degree measurement windows remain unchanged.

Metal API validation: raw, UInt32, UInt16 and dedicated UInt16 shader controls
complete; final Tokyo PNGs match the current place byte-for-byte. Every source
colour slot passes a four-word roundtrip. Actual palette binding is asserted;
an initial inactive probe was rejected rather than counted as evidence.

Normal-priority A/B/B/A runs, API validation disabled, full original shaders versus
dedicated UInt16 shaders, A18 Pro / 1280×720 / 60 frames. Both repeats per mode:

| Place | Raw GPU p50 ms | Palette GPU p50 ms | Raw GPU p99 ms | Palette GPU p99 ms |
|---|---:|---:|---:|---:|
| Wien | 10.58–11.80 | 10.93–11.06 | 24.35–24.35 | 22.31–22.69 |
| Tokyo | 13.47–13.59 | 13.95–14.21 | 18.86–21.35 | 17.72–17.95 |

All worlds and all 60 GPU samples complete; no unrefined frames. Final PNGs match
the original places exactly. Sparse repeats and first-use/load variation do not
establish a general p99 improvement; none meets the full 16.67 ms p99 condition.
The UInt32 shader with a per-draw dynamic raw/palette branch additionally regresses
Wien (raw p50 10.62–10.80 ms, palette 13.38–14.31 ms). Central Park and Flensburg
also completed unchanged, but their mixed timings do not rescue a universal claim.
Index density, dependency latency and shader specialization must be considered
together. Fewer stored bytes alone are insufficient.

## Reproduce the encoding independently

Use NumPy and a raw little-endian RGBA32F colour buffer read back after complete
world preparation. This reference sorts word tuples; its CPU cost is not the
native hash-table preparation cost or a proposed runtime loader.

```sh
python3 -B test/experiments/render/vertex_colour_palette.py \
  --input /tmp/capture/colours.bin --output /tmp/outshine-colour-reference
python3 -B -m unittest discover -s test/experiments/render \
  -p test_vertex_colour_palette.py
```

The report includes both exact encodings and packing time. Tests cover HDR,
alpha, signed zero, distinct NaN payloads, odd/empty counts, UInt16 capacity and
malformed spans/indices. Full native compiler/link/shader commands, source hashes,
private renderer copies and per-frame receipts are recorded in #904; this CPU
reference is not a stand-in for those device runs or a Vulkan proof.
