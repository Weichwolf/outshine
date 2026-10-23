# Lossless field tables for complete building cells

Research [#1057](https://github.com/Weichwolf/outshine/issues/1057), supporting
[#1045](https://github.com/Weichwolf/outshine/issues/1045). This experiment preserves
the complete native geometry rather than approximating a camera view. It does not
change a native cache format, shader or binding. Keep the existing Raw product.

## Representation and controls

`vertex_field_cases.py` verifies the original archive/manifest before adapting six
native inputs. The dense Shell contains 576 sources, 28,672 vertices and 14,169
triangles. Other controls include complete Full/Fine bodies and the existing native
tinted-backface shader fixture, including its degenerate triangle slots.
Whole-record sharing has no reuse in five inputs (including the dense Shell),
and only one duplicate among 6,398 vertices in native-cell-Full. This counted control
does not justify a separate whole-record token implementation.

`vertex_field_words.py` shares identical **uint32 word tuples**, independently for
position, UV, packed normal and material metres. It neither welds vertex identities
nor quantizes floats. Four-field mode stores two uint32 tokens per vertex; the
position/normal control stores one token and retains raw UV/material metres.
Each token packs two u16 field IDs. Tables exceeding 65,536 entries decline to Raw;
the geometry index width is independently u16/u32. An optional size check rejects
packets no smaller than the supplied native core. This is size eligibility only.

RGBA remains raw, with an implicit exact-white suffix; raw material metres in the
partial control omit only an exact positive-zero suffix. There is no colour palette.
All six captured inputs have white vertex RGBA, including the tinted shader fixture:
its material tint is separate. Constructed tests additionally cover HDR, alpha,
signed zero, subnormals and arbitrary normal words; they are not HDR captures.
Original triangle order, corner words and existing source bindings remain intact.
Common metadata/bindings are retained separately and excluded from both byte totals.

The 48-byte experimental packet header is followed by tokens, field tables, raw
field prefixes and original topology. The scalar oracle manually interprets these
bytes independently of the vector layout/decoder. The tests compare both decoders
with original native triangle-corner bytes, including Fine inputs, u16 table limits,
u32 geometry indices and malformed packets. No lossy image threshold is involved.

## Measured result

Producer `82aa41bd5`, Linux/shared host, Python 3.13.5, NumPy 2.5.4, libzstd 1.5.7.
Three fresh normal processes per mode and a separate traced process per mode;
one BLAS thread. All runs reconstruct identical original corner words.
Later changes add only test coverage and this report.

| Input | Native core B | Four-field B | Pos./normal B | Raw Zstd1 B | Four Zstd1 B | Partial Zstd1 B |
|---|---:|---:|---:|---:|---:|---:|
| dense-shell | 943302 | 497126 | 713214 | 346662 | 372534 | 341933 |
| native-cell-Full | 175322 | 111446 | 162838 | 60500 | 84210 | 77462 |
| native-cell-Fine | 1017158 | 589134 | 946598 | 308172 | 430395 | 402837 |
| near-cell-Full | 788636 | 464904 | 575980 | 304767 | 327399 | 309472 |
| near-cell-Fine | 1123752 | 661784 | 903704 | 415157 | 469493 | 445191 |
| native-tinted | 157386 | 150234 | 150330 | 2481 | 2992 | 2880 |

The comparison uses native level 1, minimum size 64 B and Raw fallback from
`AssetPackageCodec.cpp`. It compresses each isolated whole core in its original
stream order, excluding shared package headers and actual package grouping. These
are not measured DB/SSD transfers. The four-field representation makes **every**
Zstd1 control larger; partial sharing saves only 1.36% in the dense Shell and loses
on the other five cases. Raw remains the appropriate compressed control.

Dense four-field construction takes 108.981/349.715/119.466 ms and vector decode
2.343/7.644/3.287 ms. Partial construction takes 61.264/83.377/62.942 ms, decode
1.585/1.458/1.460 ms. Keep the shared-host outlier; no device timing inference.
For the four-field runs, Zstd decompression takes 0.935/1.354/0.987 ms versus the
matched Raw control's 1.610/2.351/2.579 ms, **before** the extra field decode.
ctypes output copying is included; input verification/serialization is separate.

Dense traced construction peaks are 2,186,850/2,204,518 B for full/partial sharing;
both decoders peak at 1,609,205 B. These are separate Python stage measurements,
not application residency. Decoder outputs occupy 48 bytes per vertex (1,376,256 B
here), including expanded white RGBA and roof metre padding absent from the native
core. The index view also retains the entire encoded packet. Consequently the full
CPU decode retains at least 1,873,382 B before common metadata/transients, and does
not demonstrate lower RAM usage. Whole-process RSS around 277 MB includes fixtures,
JSON and both independent verifications. Known serialization copies are twice the
packet size, 994,252/1,426,428 B; total copies/allocations are not fully instrumented.

Direct GPU consumption could reduce encoded residency by 47.3% in the dense case,
but needs four dependent table gathers (partial: two) and logical wall-field reads
of 40/36 bytes versus Raw's 32, excluding common RGBA. Cache traffic and device time
are unknown. No GPU consumer was implemented; no native memory/frame claim follows.

The original static-020 view was checked at 1280×720, its 48×10 pixel ROI and four
rays/pixel: 1,920 hits/field results are bit-identical across all three paths. The
two-light opaque CPU diagnostic was actually opened. A fresh replay reads only
packets, binding and saved rays/reference fields, with original-fixture opens
forbidden. This is an enlarged diagnostic, not native PBR or movement qualification.

## Reproduce

Use the checkout's `.venv` with NumPy, Shapely, Trimesh and system libzstd. From the
repository root, set `PYTHONPATH=lib/src:test/experiments/render`,
`OPENBLAS_NUM_THREADS=1` and `OMP_NUM_THREADS=1`, then run:

```sh
.venv/bin/python -m unittest discover -s test/experiments/render -p test_vertex_field_words.py
.venv/bin/python test/experiments/render/compare_vertex_field_words.py --mode all --output /tmp/outshine-fields-all
.venv/bin/python test/experiments/render/compare_vertex_field_words.py --mode position-normal --output /tmp/outshine-fields-partial
```

Outputs must be fresh explicit system-temp directories. Repeat normal processes
before a separate `--trace` process. Commit experiment sources before measuring.
Reports retain input and expanded-corner hashes, stage timings, known copies and
scope. Local image/replay proofs and raw reports are linked from the PR; no images
are published in the repository.

SIGGRAPH starting point: [Deering, *Geometry Compression*, 1995](../../../doc/references/transport/siggraph/1995-geometry-compression.pdf),
particularly Figure 1/section 3 (page 2 visually inspected). It motivates the
representation/bandwidth question; this experiment does not implement its lossy
quantization, normal encoding, strip or entropy codec. A token/table GPU proposal
would require agreed Metal/Vulkan bindings and a matched device comparison first.
